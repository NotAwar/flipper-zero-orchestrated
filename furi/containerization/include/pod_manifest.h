#pragma once

#include "container_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for all types
typedef struct PodManifest PodManifest;
typedef struct PodSpec PodSpec;

// Define missing types that were causing errors
typedef struct PodTaint {
    const char* key;
    const char* value;
    const char* effect;
} PodTaint;

typedef struct ConfigMap {
    const char* name;
    const char** keys;
    const char** values;
    size_t count;
} ConfigMap;

typedef struct ResourceQuota {
    size_t max_memory;
    uint8_t max_cpu;
    uint8_t max_pods;
    uint8_t max_containers;
} ResourceQuota;

// Container spec within a pod
typedef struct {
    const char* name;
    const char* image;
    ContainerResourceLimits resources;
    bool restart_on_crash;
    bool system_privileges;
    void* args;
} PodContainerSpec;

/**
 * @brief Load a pod manifest from file
 */
PodManifest* pod_manifest_load_from_file(const char* path);

/**
 * @brief Validate that a pod manifest can be deployed
 */
bool pod_manifest_validate(PodManifest* manifest);

/**
 * @brief Free a pod manifest
 */
void pod_manifest_free(PodManifest* manifest);

/**
 * @brief Get the name of a pod manifest
 */
const char* pod_manifest_get_name(const PodManifest* manifest);

/**
 * @brief Get the namespace of a pod manifest
 */
const char* pod_manifest_get_namespace(const PodManifest* manifest);

/**
 * @brief Get the container specifications from a pod manifest
 */
uint32_t pod_manifest_get_containers(
    const PodManifest* manifest,
    const PodContainerSpec** containers);

/**
 * @brief Apply taints and tolerations
 */
bool pod_manifest_apply_taints(PodManifest* manifest, const PodTaint* taints, size_t taint_count);

/**
 * @brief Apply a ConfigMap to a pod manifest
 */
bool pod_manifest_apply_config_map(PodManifest* manifest, const ConfigMap* config_map);

/**
 * @brief Apply resource quotas to a pod
 */
bool pod_manifest_apply_resource_quota(PodManifest* manifest, const ResourceQuota* quota);

#ifdef __cplusplus
}
#endif
