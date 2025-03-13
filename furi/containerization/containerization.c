/**
 * @file containerization.c
 * @brief Flipper Zero Lightweight Containerization System Implementation
 */

#include "containerization.h"
#include <furi.h>
#include <furi/core/log.h>
#include <storage/storage.h>

#define TAG "Container"

// System containers configuration path
#define SYSTEM_CONTAINERS_PATH "/ext/resources/containerization/system-pod.json"

bool containerization_init(void) {
    ContainerRuntime* runtime = furi_get_container_runtime();
    ServiceRegistry* registry = furi_get_service_registry();
    
    // Register core system services in the service registry
    if(registry) {
        ServiceDescriptor system_services[] = {
            {
                .name = "storage",
                .namespace = "system",
                .type = ServiceTypeInternal,
                .name_persistent = true,
                .namespace_persistent = true,
            },
            {
                .name = "gui",
                .namespace = "system", 
                .type = ServiceTypeInternal,
                .name_persistent = true,
                .namespace_persistent = true,
            },
            {
                .name = "notification",
                .namespace = "system",
                .type = ServiceTypeInternal,
                .name_persistent = true,
                .namespace_persistent = true,
            },
            {
                .name = "loader",
                .namespace = "system",
                .type = ServiceTypeInternal,
                .name_persistent = true,
                .namespace_persistent = true,
            }
        };
        
        // Register all system services
        for(uint8_t i = 0; i < sizeof(system_services)/sizeof(ServiceDescriptor); i++) {
            service_registry_register(registry, &system_services[i]);
        }
        
        FURI_LOG_I(TAG, "System services registered");
    }
    
    return (runtime != NULL) && (registry != NULL);
}

bool containerization_start_system_containers(void) {
    ContainerRuntime* runtime = furi_get_container_runtime();
    if(!runtime) {
        FURI_LOG_E(TAG, "Container runtime not initialized");
        return false;
    }
    
    // Fast path - check if manifest exists before trying to load it
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool manifest_exists = storage_file_exists(storage, SYSTEM_CONTAINERS_PATH);
    furi_record_close(RECORD_STORAGE);
    
    if(!manifest_exists) {
        FURI_LOG_I(TAG, "No system containers manifest found");
        return true; // Not an error, just no system containers
    }
    
    // Load and instantiate system containers
    PodManifest* manifest = pod_manifest_load_from_file(SYSTEM_CONTAINERS_PATH);
    if(!manifest) {
        FURI_LOG_E(TAG, "Failed to load system containers manifest");
        return false;
    }
    
    Container** containers = pod_manifest_instantiate(runtime, manifest);
    bool success = (containers != NULL);
    
    if(success) {
        FURI_LOG_I(TAG, "System containers started");
        free(containers); // Free container array
    } else {
        FURI_LOG_E(TAG, "Failed to start system containers");
    }
    
    pod_manifest_free(manifest);
    return success;
}

size_t containerization_get_stats_string(char* buffer, size_t size) {
    if(!buffer || size == 0) return 0;
    
    ContainerRuntime* runtime = furi_get_container_runtime();
    if(!runtime) {
        snprintf(buffer, size, "Container runtime not available");
        return strlen(buffer);
    }
    
    // Ultra-minimal stats - just container count and free memory
    uint8_t count = container_runtime_get_count(runtime);
    uint32_t free_heap = memmgr_get_free_heap();
    uint32_t max_block = memmgr_heap_get_max_free_block();
    
    return snprintf(
        buffer, 
        size, 
        "Pods: %u/8, Mem: %luK, Blk: %luK", 
        count, 
        (unsigned long)(free_heap/1024), 
        (unsigned long)(max_block/1024));
}
