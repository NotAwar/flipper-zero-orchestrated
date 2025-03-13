#pragma once

#include <furi.h>
#include <furi/core/mutex.h>
#include <furi/core/thread.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Container Runtime for Flipper Zero
 * 
 * This component implements a lightweight containerization model
 * suitable for microcontroller environments, inspired by Kubernetes.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ContainerRuntime ContainerRuntime;
typedef struct Container Container;

/** Container states */
typedef enum {
    ContainerStatePending,
    ContainerStateRunning,
    ContainerStatePaused,
    ContainerStateTerminated,
} ContainerState;

/** Resource limits for a container */
typedef struct {
    uint32_t max_memory;     // Maximum memory in bytes
    uint32_t cpu_time_share; // CPU time share (0-100%)
    uint32_t max_threads;    // Maximum number of threads
} ContainerResourceLimits;

/** Container configuration */
typedef struct {
    const char* name;
    const char* image;       // FAP file path or built-in app name
    ContainerResourceLimits resource_limits;
    void* args;              // Application arguments
    bool restart_on_crash;   // Auto-restart on abnormal termination
    bool system_container;   // Has access to system resources
} ContainerConfig;

/** Container status information */
typedef struct {
    ContainerState state;
    uint32_t memory_used;
    uint32_t cpu_usage;
    uint32_t uptime;
    uint32_t restart_count;
} ContainerStatus;

/**
 * @brief Create a container runtime
 * 
 * @return ContainerRuntime* 
 */
ContainerRuntime* container_runtime_alloc();

/**
 * @brief Free container runtime
 * 
 * @param runtime 
 */
void container_runtime_free(ContainerRuntime* runtime);

/**
 * @brief Start the container runtime
 * 
 * @param runtime 
 */
void container_runtime_start(ContainerRuntime* runtime);

/**
 * @brief Create a new container
 * 
 * @param runtime 
 * @param config 
 * @return Container* 
 */
Container* container_create(ContainerRuntime* runtime, const ContainerConfig* config);

/**
 * @brief Start a container
 * 
 * @param container 
 * @return true if started successfully
 * @return false if failed to start
 */
bool container_start(Container* container);

/**
 * @brief Pause a container
 * 
 * @param container 
 */
void container_pause(Container* container);

/**
 * @brief Resume a paused container
 * 
 * @param container 
 */
void container_resume(Container* container);

/**
 * @brief Stop a container
 * 
 * @param container graceful shutdown if true
 * @param force force kill if true
 */
void container_stop(Container* container, bool force);

/**
 * @brief Get container status
 * 
 * @param container 
 * @param status 
 */
void container_get_status(Container* container, ContainerStatus* status);

/**
 * @brief Get the count of containers
 * 
 * @param runtime 
 * @return uint8_t Number of containers
 */
uint8_t container_runtime_get_count(ContainerRuntime* runtime);

#ifdef __cplusplus
}
#endif
