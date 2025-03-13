#include "container_runtime.h"
#include <furi.h>
#include <furi/core/mutex.h>
#include <furi/core/thread.h>
#include <furi/core/timer.h>
#include <furi/core/log.h>
#include <applications/services/loader/loader.h>
#include <storage/storage.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define TAG "ContainerRT"

// Reduced maximum number of containers to conserve memory
#define MAX_CONTAINERS 8

typedef struct Container {
    ContainerConfig config;
    ContainerStatus status;
    void* app_handle;
} Container;

struct ContainerRuntime {
    FuriMutex* mutex;
    FuriTimer* scheduler_timer;
    Container containers[MAX_CONTAINERS];
    uint8_t container_count;
    bool running;
};

// Ultra-minimal container health checker - just checks if the underlying app is still running
static void container_check_health(Container* container) {
    if(!container || container->status.state != ContainerStateRunning) {
        return;
    }
    
    Loader* loader = furi_record_open(RECORD_LOADER);
    bool app_running = false;
    
    if(strstr(container->config.image, ".fap") != NULL) {
        // External FAP app
        app_running = loader_is_app_running(loader, container->config.image);
    } else {
        // Built-in app
        app_running = loader_is_locked(loader);
    }
    
    furi_record_close(RECORD_LOADER);
    
    if(!app_running && container->status.state == ContainerStateRunning) {
        container->status.state = ContainerStateTerminated;
    }
}

// Enhanced scheduler callback with health checking
static void container_runtime_scheduler_callback(void* context) {
    ContainerRuntime* runtime = context;
    
    if(!furi_mutex_acquire(runtime->mutex, 0)) return;

    for(uint8_t i = 0; i < MAX_CONTAINERS; i++) {
        Container* container = &runtime->containers[i];
        
        // Skip empty slots
        if(container->config.name == NULL) continue;
        
        // Check health first
        container_check_health(container);
        
        // Update uptime for running containers
        if(container->status.state == ContainerStateRunning) {
            container->status.uptime++;
            
            // Minimal resource usage sampling - avoid frequent calls to save CPU
            if(container->status.uptime % 10 == 0) { // Every 10 seconds
                // Estimate memory usage - very lightweight approximation
                container->status.memory_used = container->config.resource_limits.max_memory / 2;
                
                // Set CPU usage to a simple estimate
                container->status.cpu_usage = container->config.resource_limits.cpu_time_share / 2;
            }
        }
        
        // Auto-restart containers that crashed but should be restarted
        if(container->config.restart_on_crash && 
           container->status.state == ContainerStateTerminated) {
            
            // Add exponential backoff for restarts to prevent rapid cycling
            uint32_t delay_factor = container->status.restart_count > 5 ? 5 : container->status.restart_count;
            if(container->status.restart_count == 0 || 
               (container->status.uptime > delay_factor * delay_factor)) {
                container_start(container);
                container->status.restart_count++;
            }
        }
    }

    furi_mutex_release(runtime->mutex);
}

// Ultra-minimal FAP existence check - skip for built-ins
static bool container_check_local_image(const char* image_path) {
    if(!image_path) return false;
    
    // Skip check for built-in apps (no .fap extension)
    if(!strstr(image_path, ".fap")) {
        return true;
    }
    
    // Ultra-minimal validation - just check if file exists
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo file_info;
    bool exists = storage_common_stat(storage, image_path, &file_info) == FSE_OK;
    furi_record_close(RECORD_STORAGE);
    
    if(!exists) {
        FURI_LOG_E(TAG, "FAP not found: %s", image_path);
    }
    
    return exists;
}

ContainerRuntime* container_runtime_alloc(void) {
    ContainerRuntime* runtime = malloc(sizeof(ContainerRuntime));
    if(!runtime) {
        FURI_LOG_E(TAG, "Failed to allocate runtime");
        return NULL;
    }
    
    memset(runtime, 0, sizeof(ContainerRuntime));
    
    runtime->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!runtime->mutex) {
        FURI_LOG_E(TAG, "Failed to allocate mutex");
        free(runtime);
        return NULL;
    }
    
    runtime->container_count = 0;
    runtime->running = false;
    
    return runtime;
}

void container_runtime_free(ContainerRuntime* runtime) {
    furi_assert(runtime);
    
    // Stop the scheduler
    if(runtime->running && runtime->scheduler_timer) {
        furi_timer_stop(runtime->scheduler_timer);
        furi_timer_free(runtime->scheduler_timer);
    }
    
    // Free all containers
    for(uint8_t i = 0; i < MAX_CONTAINERS; i++) {
        Container* container = &runtime->containers[i];
        
        if(container->config.name == NULL) continue;
        
        if(container->status.state != ContainerStateTerminated) {
            container_stop(container, true);
        }
        
        // Free allocated config strings
        if(container->config.name) free((void*)container->config.name);
        if(container->config.image) free((void*)container->config.image);
    }
    
    furi_mutex_free(runtime->mutex);
    free(runtime);
}

void container_runtime_start(ContainerRuntime* runtime) {
    furi_assert(runtime);
    
    if(runtime->running) return;
    
    runtime->running = true;
    
    // Start the scheduler timer - check every 1 second (minimal overhead)
    runtime->scheduler_timer = furi_timer_alloc(
        container_runtime_scheduler_callback, 
        FuriTimerTypePeriodic, 
        runtime);
    furi_timer_start(runtime->scheduler_timer, 1000);
}

// Find an available container slot
static Container* container_find_free_slot(ContainerRuntime* runtime) {
    for(uint8_t i = 0; i < MAX_CONTAINERS; i++) {
        if(runtime->containers[i].config.name == NULL) {
            return &runtime->containers[i];
        }
    }
    return NULL;
}

Container* container_create(ContainerRuntime* runtime, const ContainerConfig* config) {
    furi_assert(runtime);
    furi_assert(config);
    
    if(!config->name || !config->image) {
        FURI_LOG_E(TAG, "Invalid config");
        return NULL;
    }
    
    // Validate image exists before allocating anything
    if(!container_check_local_image(config->image)) {
        return NULL;
    }
    
    if(!furi_mutex_acquire(runtime->mutex, FuriWaitForever)) {
        FURI_LOG_E(TAG, "Failed to acquire mutex");
        return NULL;
    }
    
    // Check if we've reached max containers
    if(runtime->container_count >= MAX_CONTAINERS) {
        FURI_LOG_E(TAG, "Max containers reached");
        furi_mutex_release(runtime->mutex);
        return NULL;
    }
    
    // Find unused container slot
    Container* container = container_find_free_slot(runtime);
    if(!container) {
        FURI_LOG_E(TAG, "No container slots");
        furi_mutex_release(runtime->mutex);
        return NULL;
    }
    
    // Initialize container with proper error handling
    memset(container, 0, sizeof(Container));
    
    char* name_copy = strdup(config->name);
    if(!name_copy) {
        FURI_LOG_E(TAG, "Out of memory");
        furi_mutex_release(runtime->mutex);
        return NULL;
    }
    
    char* image_copy = strdup(config->image);
    if(!image_copy) {
        FURI_LOG_E(TAG, "Out of memory");
        free(name_copy);
        furi_mutex_release(runtime->mutex);
        return NULL;
    }
    
    container->config.name = name_copy;
    container->config.image = image_copy;
    container->config.args = config->args; // Just store the pointer
    container->config.restart_on_crash = config->restart_on_crash;
    container->config.system_container = config->system_container;
    
    // Use provided resource limits with ultra-minimal defaults
    container->config.resource_limits.max_memory = 
        config->resource_limits.max_memory > 0 ? 
        config->resource_limits.max_memory : 2 * 1024; // Even more minimal: 2KB default
    
    container->config.resource_limits.cpu_time_share = 
        config->resource_limits.cpu_time_share > 0 ?
        config->resource_limits.cpu_time_share : 3; // Even more minimal: 3% default
    
    container->config.resource_limits.max_threads = 
        config->resource_limits.max_threads > 0 ?
        config->resource_limits.max_threads : 1; // Keep 1 thread as absolute minimum
    
    container->status.state = ContainerStatePending;
    container->status.restart_count = 0;
    container->status.uptime = 0;
    
    runtime->container_count++;
    
    furi_mutex_release(runtime->mutex);
    
    return container;
}

static bool container_run_app(Container* container) {
    // We already checked that the app exists in container_create
    Loader* loader = furi_record_open(RECORD_LOADER);
    bool success = false;
    
    // Check if app is external FAP
    if(strstr(container->config.image, ".fap") != NULL) {
        success = loader_start_with_gui_error(
            loader, 
            container->config.image, 
            container->config.args);
        
        if(success) {
            container->app_handle = (void*)container->config.image;
        }
    } else {
        // Built-in application
        success = loader_start(loader, container->config.image, container->config.args);
        container->app_handle = (void*)1; // Placeholder for built-ins
    }
    
    furi_record_close(RECORD_LOADER);
    return success;
}

bool container_start(Container* container) {
    furi_assert(container);
    
    // Already running? Return success immediately
    if(container->status.state == ContainerStateRunning) {
        return true;
    }
    
    // Start the application
    bool success = container_run_app(container);
    
    if(success) {
        container->status.state = ContainerStateRunning;
        container->status.uptime = 0;
    }
    
    return success;
}

void container_stop(Container* container, bool force) {
    furi_assert(container);
    
    if(container->status.state == ContainerStateTerminated) {
        return;
    }
    
    // Stop the application
    Loader* loader = furi_record_open(RECORD_LOADER);
    
    // External app vs built-in app
    if(strstr(container->config.image, ".fap") != NULL) {
        loader_stop_external(loader, container->config.image);
    } else {
        loader_stop(loader);
    }
    
    furi_record_close(RECORD_LOADER);
    
    container->status.state = ContainerStateTerminated;
}

void container_get_status(Container* container, ContainerStatus* status) {
    furi_assert(container);
    furi_assert(status);
    
    // Check if application is still running
    container_check_health(container);
    
    *status = container->status;
}

// Ultra-optimized container counter - direct access with mutex
uint8_t container_runtime_get_count(ContainerRuntime* runtime) {
    furi_assert(runtime);
    
    furi_mutex_acquire(runtime->mutex, FuriWaitForever);
    uint8_t count = runtime->container_count;
    furi_mutex_release(runtime->mutex);
    
    return count;
}
