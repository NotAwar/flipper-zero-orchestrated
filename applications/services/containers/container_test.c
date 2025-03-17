#include <furi.h>
#include <furi/core/log.h>
#include <furi/containerization/container_runtime.h>
#include <furi/containerization/pod_manifest.h>

#define TAG "ContainerTest"

// Mark as used to avoid unused function warning
__attribute__((used))
static int32_t test_app_main(void* args) {
    FURI_LOG_I(TAG, "Test app started with args: %s", args ? (char*)args : "null");
    
    // Simulate some work
    for(int i = 0; i < 10; i++) {
        furi_delay_ms(500);
        FURI_LOG_I(TAG, "Test app running... %d/10", i+1);
    }
    
    FURI_LOG_I(TAG, "Test app completed");
    return 0;
}

int32_t container_test_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Container test starting");

    // Initialize container runtime
    ContainerRuntime* runtime = container_runtime_alloc();
    if (!runtime) {
        FURI_LOG_E(TAG, "Failed to initialize container runtime");
        return -1;
    }

    // Start the runtime
    container_runtime_start(runtime);
    
    // Create a simple container config
    ContainerConfig config;
    config.name = "test_container";
    config.image = "test_app";
    config.args = "test arguments";
    config.restart_on_crash = true;
    config.system_container = false;
    
    // Set resource limits
    config.resource_limits.max_memory = 4 * 1024; // 4KB
    config.resource_limits.cpu_time_share = 10;   // 10%
    config.resource_limits.max_threads = 1;
    
    // Create the container
    Container* container = container_create(runtime, &config);
    if (!container) {
        FURI_LOG_E(TAG, "Failed to create container");
        container_runtime_free(runtime);
        return -2;
    }
    
    // Start the container
    FURI_LOG_I(TAG, "Starting container");
    if (!container_start(container)) {
        FURI_LOG_E(TAG, "Failed to start container");
        container_runtime_free(runtime);
        return -3;
    }
    
    // Check container status
    ContainerStatus status;
    
    // Wait for container to finish
    while(true) {
        container_get_status(container, &status);
        if (status.state == ContainerStateTerminated) {
            break;
        }
        
        FURI_LOG_I(TAG, "Container status: %d, uptime: %lu", 
                 status.state, status.uptime);
        furi_delay_ms(1000);
    }
    
    FURI_LOG_I(TAG, "Container test completed");
    container_runtime_free(runtime);
    return 0;
}
