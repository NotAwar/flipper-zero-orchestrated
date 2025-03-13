#pragma once

#include <furi.h>
#include "container_runtime.h"

/**
 * @brief Pod manifest for declarative application management
 * 
 * Similar to Kubernetes Pod spec - defines a group of containers
 * that should be deployed together
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PodManifest PodManifest;
typedef struct PodSpec PodSpec;

/** Pod health check type */
typedef enum {
    HealthCheckTypeNone,
    HealthCheckTypeCommand,
    HealthCheckTypeHttp,
} HealthCheckType;

/** Pod health check */
typedef struct {
    HealthCheckType type;
    union {
        const char* command;
        const char* endpoint;
    };
    uint32_t initial_delay_ms;
    uint32_t period_ms;
    uint32_t timeout_ms;
    uint32_t success_threshold;
    uint32_t failure_threshold;
} HealthCheckSpec;

/** Volume mount */
typedef struct {
    const char* name;
    const char* mount_path;
    bool read_only;
} VolumeMountSpec;

/** Container spec within a pod */
typedef struct {
    const char* name;
    const char* image;
    ContainerResourceLimits resources;
    HealthCheckSpec health_check;
    VolumeMountSpec* volume_mounts;
    uint32_t volume_mount_count;
    bool restart_on_crash;
    bool system_privileges;
    void* args;
} PodContainerSpec;

/**
 * @brief Load a pod manifest from file
 * 
 * Loads and parses a pod manifest file with containers definition.
 * Similar to 'kubectl apply -f' in Kubernetes.
 * 
 * @param path Path to manifest file
 * @return PodManifest* or NULL if loading failed
 */
PodManifest* pod_manifest_load_from_file(const char* path);

/**
 * @brief Validate that a pod manifest can be deployed
 * 
 * Checks if all required resources (FAP files, etc) exist.
 * Similar to 'kubectl apply --validate=true' in Kubernetes.
 * 
 * @param manifest The manifest to validate
 * @return true if valid, false otherwise
 */
bool pod_manifest_validate(PodManifest* manifest);

/**
 * @brief Free a pod manifest
 * 
 * @param manifest 
 */
void pod_manifest_free(PodManifest* manifest);

/**
 * @brief Get the name of a pod manifest
 * 
 * @param manifest 
 * @return const char* 
 */
const char* pod_manifest_get_name(const PodManifest* manifest);

/**
 * @brief Get the namespace of a pod manifest
 * 
 * @param manifest 
 * @return const char* 
 */
const char* pod_manifest_get_namespace(const PodManifest* manifest);

/**
 * @brief Get the container specifications from a pod manifest
 * 
 * @param manifest 
 * @param containers Pointer to array that will be populated
 * @return uint32_t Number of containers
 */
uint32_t pod_manifest_get_containers(
    const PodManifest* manifest,
    const PodContainerSpec** containers);

/**
 * @brief Create containers from a pod manifest
 * 
 * Creates and starts containers defined in the manifest.
 * Similar to 'kubectl apply' in Kubernetes.
 * 
 * @param runtime The container runtime
 * @param manifest The manifest with container definitions
 * @return Container** Array of created containers
 */
Container** pod_manifest_instantiate(
    ContainerRuntime* runtime,
    const PodManifest* manifest);

#ifdef __cplusplus
}
#endif
