#include "furi.h"
#include <furi/core/log.h>
#include <furi/core/thread.h>
#include <furi/core/record.h>
#include <furi_hal.h>

// Include containerization headers
#include <furi/containerization/container_runtime.h>
#include <furi/containerization/service_registry.h>
#include <furi/containerization/containerization.h>

#define TAG "Furi"

// Containerization globals
static ContainerRuntime* furi_container_runtime = NULL;
static ServiceRegistry* furi_service_registry = NULL;

// Core initializers
extern void furi_core_init_early();
extern void furi_core_init_late();

// Services
extern void furi_record_init();

void furi_init() {
    // Core initialization
    furi_core_init_early();
    FURI_LOG_I(TAG, "Starting services");
    furi_record_init();
    furi_core_init_late();

    // Initialize containerization
    furi_container_runtime = container_runtime_alloc();
    furi_service_registry = service_registry_alloc();

    // Start container runtime
    container_runtime_start(furi_container_runtime);
    
    // Start system containers if containerization initialized properly
    if(containerization_init()) {
        containerization_start_system_containers();
    }
    
    FURI_LOG_I(TAG, "Started with container support");
}

void furi_run() {
    // Execution passes to RTOS
    furi_thread_yield();
}

void furi_exit() {
    // Stop and free container runtime
    if(furi_container_runtime) {
        container_runtime_free(furi_container_runtime);
    }

    // Free service registry
    if(furi_service_registry) {
        service_registry_free(furi_service_registry);
    }
    
    FURI_LOG_I(TAG, "Goodbye!");
}

// Global accessor functions for containerization components
ContainerRuntime* furi_get_container_runtime() {
    return furi_container_runtime;
}

ServiceRegistry* furi_get_service_registry() {
    return furi_service_registry;
}
