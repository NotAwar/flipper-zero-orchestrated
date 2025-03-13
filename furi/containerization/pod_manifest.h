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
 * @brief Memory-optimized pod manifest validator
 * Similar to 'kubectl apply --validate=true' in Kubernetes.
 * @param manifest Pod manifest to validate
 * @return bool true if manifest is valid
 */
bool pod_manifest_validate_optimized(const PodManifest* manifest);

/**
 * @brief Batch apply multiple pod manifests
 * Similar to 'kubectl apply -f' with multiple files in Kubernetes.
 * @param manifests Array of pod manifests
 * @param count Number of manifests in array
 * @return int Number of successfully applied manifests
 */
int pod_manifest_batch_apply(PodManifest** manifests, size_t count);

/**
 * @brief Create a memory-efficient pod manifest from JSON
 * @param json_data JSON string containing manifest data
 * @return PodManifest* Newly created pod manifest or NULL on failure
 */
PodManifest* pod_manifest_create_from_json(const char* json_data);

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

/**
 * @brief Apply a ConfigMap to a pod manifest
 * 
 * Updates environment variables and volume mounts in the pod
 * based on a configuration map, similar to K8s ConfigMaps.
 * 
 * @param manifest The pod manifest to update
 * @param config_map The configuration map to apply
 * @return bool true if successful
 */
bool pod_manifest_apply_config_map(PodManifest* manifest, const ConfigMap* config_map);

/**
 * @brief Apply resource quotas to a pod
 * 
 * Enforces resource limits defined in a resource quota,
 * similar to K8s ResourceQuotas.
 * 
 * @param manifest The pod manifest to update
 * @param quota The resource quota to apply
 * @return bool true if successful
 */
bool pod_manifest_apply_resource_quota(PodManifest* manifest, const ResourceQuota* quota);

/**
 * @brief Convert a pod manifest to JSON
 * 
 * Serializes a pod manifest to JSON format,
 * similar to 'kubectl get pod -o json'.
 * 
 * @param manifest The pod manifest to convert
 * @param pretty Whether to format the JSON for readability
 * @return char* JSON string (must be freed by caller)
 */
char* pod_manifest_to_json(const PodManifest* manifest, bool pretty);

#ifdef __cplusplus
}
#endif
