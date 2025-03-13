#include "pod_manifest.h"
#include <furi.h>
#include <storage/storage.h>
#include <lib/flipper_format/flipper_format.h>
#include <stdlib.h>
#include <string.h>
#include <furi/core/log.h>

#define TAG "PodManifest"

// Maximum allowed containers per manifest to prevent memory exhaustion
#define MAX_CONTAINERS 8

struct PodManifest {
    char* name;
    char* namespace;
    PodContainerSpec* containers;
    uint32_t container_count;
};

PodManifest* pod_manifest_load_from_file(const char* path) {
    furi_assert(path);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* format = flipper_format_file_alloc(storage);
    
    PodManifest* manifest = NULL;
    FuriString* temp_str = furi_string_alloc();
    
    do {
        if(!flipper_format_file_open_existing(format, path)) {
            FURI_LOG_E(TAG, "Failed to open %s", path);
            break;
        }
        
        uint32_t format_version;
        if(!flipper_format_read_header(format, temp_str, &format_version) || 
           furi_string_cmp_str(temp_str, "Flipper Pod Manifest") != 0 ||
           format_version != 1) {
            FURI_LOG_E(TAG, "Invalid manifest format");
            break;
        }
        
        // Allocate manifest with minimal memory footprint
        manifest = malloc(sizeof(PodManifest));
        if(!manifest) {
            FURI_LOG_E(TAG, "Out of memory");
            break;
        }
        
        memset(manifest, 0, sizeof(PodManifest));
        
        // Read metadata with minimal memory usage
        if(!flipper_format_read_string(format, "Name", temp_str)) {
            FURI_LOG_E(TAG, "Missing pod name");
            break;
        }
        manifest->name = strdup(furi_string_get_cstr(temp_str));
        
        if(!flipper_format_read_string(format, "Namespace", temp_str)) {
            FURI_LOG_E(TAG, "Missing namespace");
            break;
        }
        manifest->namespace = strdup(furi_string_get_cstr(temp_str));
        
        // Read container count
        uint32_t container_count;
        if(!flipper_format_read_uint32(format, "ContainerCount", &container_count, 1) || 
           container_count == 0) {
            FURI_LOG_E(TAG, "Invalid container count");
            break;
        }
        
        // Limit container count to prevent memory exhaustion
        if(container_count > MAX_CONTAINERS) {
            FURI_LOG_W(TAG, "Container count %lu exceeds maximum, limiting to %d", 
                      (unsigned long)container_count, MAX_CONTAINERS);
            container_count = MAX_CONTAINERS;
        }
        
        manifest->container_count = container_count;
        manifest->containers = malloc(sizeof(PodContainerSpec) * container_count);
        if(!manifest->containers) {
            FURI_LOG_E(TAG, "Out of memory for containers");
            break;
        }
        
        memset(manifest->containers, 0, sizeof(PodContainerSpec) * container_count);
        
        // Process containers one-by-one to minimize memory usage
        for(uint32_t i = 0; i < container_count; i++) {
            // Container key names include index: Container0, Container1, etc.
            char container_key[32];
            snprintf(container_key, sizeof(container_key), "Container%lu", (unsigned long)i);
            
            PodContainerSpec* spec = &manifest->containers[i];
            
            if(!flipper_format_read_string(format, container_key, temp_str)) {
                FURI_LOG_E(TAG, "Missing container spec %s", container_key);
                break;
            }
            spec->name = strdup(furi_string_get_cstr(temp_str));
            
            snprintf(container_key, sizeof(container_key), "Image%lu", (unsigned long)i);
            if(!flipper_format_read_string(format, container_key, temp_str)) {
                FURI_LOG_E(TAG, "Missing container image %s", container_key);
                break;
            }
            spec->image = strdup(furi_string_get_cstr(temp_str));
            
            // Read resource limits (optional)
            snprintf(container_key, sizeof(container_key), "Memory%lu", (unsigned long)i);
            uint32_t memory;
            if(flipper_format_read_uint32(format, container_key, &memory, 1)) {
                spec->resources.max_memory = memory;
            } else {
                spec->resources.max_memory = 32 * 1024; // 32KB default
            }
            
            snprintf(container_key, sizeof(container_key), "CPU%lu", (unsigned long)i);
            uint32_t cpu;
            if(flipper_format_read_uint32(format, container_key, &cpu, 1)) {
                spec->resources.cpu_time_share = cpu;
            } else {
                spec->resources.cpu_time_share = 50; // 50% default
            }
            
            snprintf(container_key, sizeof(container_key), "Threads%lu", (unsigned long)i);
            uint32_t threads;
            if(flipper_format_read_uint32(format, container_key, &threads, 1)) {
                spec->resources.max_threads = threads;
            } else {
                spec->resources.max_threads = 3; // 3 threads default
            }
            
            // Read restart policy (optional)
            snprintf(container_key, sizeof(container_key), "RestartOnCrash%lu", (unsigned long)i);
            uint32_t restart;
            if(flipper_format_read_uint32(format, container_key, &restart, 1)) {
                spec->restart_on_crash = restart != 0;
            } else {
                spec->restart_on_crash = true; // Default to true
            }
            
            // Read system privileges (optional)
            snprintf(container_key, sizeof(container_key), "SystemPrivileges%lu", (unsigned long)i);
            uint32_t sys_priv;
            if(flipper_format_read_uint32(format, container_key, &sys_priv, 1)) {
                spec->system_privileges = sys_priv != 0;
            } else {
                spec->system_privileges = false; // Default to false
            }
            
            // Default health check type to None
            spec->health_check.type = HealthCheckTypeNone;
            
            // Volume mounts not implemented in this version
            spec->volume_mounts = NULL;
            spec->volume_mount_count = 0;
        }
        
        FURI_LOG_I(
            TAG,
            "Loaded pod manifest: %s/%s with %lu containers",
            manifest->namespace,
            manifest->name,
            (unsigned long)manifest->container_count);
        
    } while(false);
    
    // Cleanup
    furi_string_free(temp_str);
    flipper_format_free(format);
    furi_record_close(RECORD_STORAGE);
    
    if(manifest && (manifest->container_count == 0 || !manifest->name || !manifest->namespace)) {
        // Failed to load essential fields, free manifest
        pod_manifest_free(manifest);
        manifest = NULL;
    }
    
    return manifest;
}

// Enhanced validation - check each container
bool pod_manifest_validate(PodManifest* manifest) {
    if(!manifest) return false;
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool all_valid = true;
    
    // Validate manifest metadata
    if(!manifest->name || !manifest->namespace) {
        FURI_LOG_E(TAG, "Invalid manifest: missing name or namespace");
        all_valid = false;
    } else if(manifest->container_count == 0) {
        FURI_LOG_E(TAG, "Invalid manifest: no containers defined");
        all_valid = false;
    }
    
    // Check that all containers have required fields
    for(uint32_t i = 0; all_valid && i < manifest->container_count; i++) {
        const PodContainerSpec* spec = &manifest->containers[i];
        
        if(!spec->name) {
            FURI_LOG_E(TAG, "Container %lu: missing name", (unsigned long)i);
            all_valid = false;
        } else if(!spec->image) {
            FURI_LOG_E(TAG, "Container %s: missing image", spec->name);
            all_valid = false;
        } 
        // Only check FAP files - built-ins always exist
        else if(strstr(spec->image, ".fap") && !storage_file_exists(storage, spec->image)) {
            FURI_LOG_E(TAG, "Container %s: image not found: %s", spec->name, spec->image);
            all_valid = false;
        }
        
        // Validate resource limits
        if(spec->resources.max_memory > 0 && spec->resources.max_memory < 1024) {
            FURI_LOG_W(TAG, "Container %s: memory limit too low (%lu bytes), minimum is 1KB",
                spec->name, (unsigned long)spec->resources.max_memory);
            // Don't fail validation but warn user
        }
    }
    
    furi_record_close(RECORD_STORAGE);
    return all_valid;
}

void pod_manifest_free(PodManifest* manifest) {
    if(!manifest) return;
    
    free(manifest->name);
    free(manifest->namespace);
    
    if(manifest->containers) {
        for(uint32_t i = 0; i < manifest->container_count; i++) {
            PodContainerSpec* spec = &manifest->containers[i];
            
            free((void*)spec->name);
            free((void*)spec->image);
            
            // Free volume mounts if any
            if(spec->volume_mounts) {
                for(uint32_t j = 0; j < spec->volume_mount_count; j++) {
                    free((void*)spec->volume_mounts[j].name);
                    free((void*)spec->volume_mounts[j].mount_path);
                }
                free(spec->volume_mounts);
            }
            
            // Free health check data if any
            if(spec->health_check.type == HealthCheckTypeCommand) {
                free((void*)spec->health_check.command);
            } else if(spec->health_check.type == HealthCheckTypeHttp) {
                free((void*)spec->health_check.endpoint);
            }
        }
        
        free(manifest->containers);
    }
    
    free(manifest);
}

const char* pod_manifest_get_name(const PodManifest* manifest) {
    furi_assert(manifest);
    return manifest->name;
}

const char* pod_manifest_get_namespace(const PodManifest* manifest) {
    furi_assert(manifest);
    return manifest->namespace;
}

uint32_t pod_manifest_get_containers(
    const PodManifest* manifest,
    const PodContainerSpec** containers) {
    furi_assert(manifest);
    
    if(containers) {
        *containers = manifest->containers;
    }
    
    return manifest->container_count;
}

// Ultra lean container instantiation
Container** pod_manifest_instantiate(
    ContainerRuntime* runtime,
    const PodManifest* manifest) {
    furi_assert(runtime);
    furi_assert(manifest);
    
    // Skip validation if no containers
    if(manifest->container_count == 0) {
        return NULL;
    }
    
    // Verify all FAPs exist before proceeding
    if(!pod_manifest_validate((PodManifest*)manifest)) {
        return NULL;
    }
    
    // Minimal container array allocation
    Container** containers = malloc(sizeof(Container*) * manifest->container_count);
    if(!containers) return NULL;
    
    // Create containers with direct string references to avoid copies
    for(uint8_t i = 0; i < manifest->container_count; i++) {
        const PodContainerSpec* spec = &manifest->containers[i];
        
        // Direct reference to spec strings to avoid allocation
        ContainerConfig config = {
            .name = spec->name,
            .image = spec->image,
            .args = spec->args,
            .restart_on_crash = spec->restart_on_crash,
            .system_container = spec->system_privileges,
            .resource_limits = {
                .max_memory = spec->resources.max_memory > 0 ? 
                    spec->resources.max_memory : 8192,  // 8K default - even more minimal
                .cpu_time_share = spec->resources.cpu_time_share > 0 ? 
                    spec->resources.cpu_time_share : 10,  // 10% default - more conservative
                .max_threads = spec->resources.max_threads > 0 ? 
                    spec->resources.max_threads : 1,   // 1 thread default - most minimal
            },
        };
        
        // Create and immediately start container
        containers[i] = container_create(runtime, &config);
        if(containers[i]) {
            container_start(containers[i]);
        }
    }
    
    return containers;
}
