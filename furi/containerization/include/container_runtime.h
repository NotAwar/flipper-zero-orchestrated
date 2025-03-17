#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ContainerRuntime ContainerRuntime;
typedef struct Container Container;
typedef struct ContainerHandle ContainerHandle;

typedef enum {
    ContainerStatePending,
    ContainerStateRunning,
    ContainerStateTerminated,
    ContainerStateError
} ContainerState;

typedef struct {
    size_t max_memory;
    uint8_t cpu_time_share;
    uint8_t max_threads;
} ContainerResourceLimits;

typedef struct {
    bool enabled;
    uint8_t type;
    uint32_t initial_delay_seconds;
    uint32_t period_seconds;
    uint8_t failure_threshold;
} ContainerHealthCheck;

typedef struct {
    const char* name;
    const char* image; 
    const char* args;
    ContainerResourceLimits resource_limits;
    ContainerHealthCheck liveness_probe;
    bool restart_on_crash;
    bool system_container;
} ContainerConfig;

typedef struct {
    ContainerState state;
    uint32_t uptime;
    uint32_t restart_count;
    size_t memory_used;
    uint8_t cpu_usage;
    uint8_t liveness_failures;
} ContainerStatus;

// Create and initialize container runtime
ContainerRuntime* container_runtime_alloc(void);

// Free container runtime
void container_runtime_free(ContainerRuntime* runtime);

// Start container runtime
void container_runtime_start(ContainerRuntime* runtime);

// Get singleton instance
ContainerRuntime* container_runtime_get_instance(void);

// Create a container
Container* container_create(ContainerRuntime* runtime, const ContainerConfig* config);

// Start a container
bool container_start(Container* container);

// Stop a container
void container_stop(Container* container, bool force);

// Get container status
void container_get_status(Container* container, ContainerStatus* status);

#ifdef __cplusplus
}
#endif
