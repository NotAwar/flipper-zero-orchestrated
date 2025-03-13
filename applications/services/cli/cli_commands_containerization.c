#include <furi.h>
#include <cli/cli.h>
#include <furi/core/string.h>
#include <furi/core/log.h>
#include <furi/containerization/containerization.h>
#include <furi/containerization/container_runtime.h>
#include <furi/containerization/service_registry.h>
#include <furi/containerization/pod_manifest.h>
#include <storage/storage.h>

#define TAG "CliContainer"

// Global container runtime for CLI access - lazy initialization
static ContainerRuntime* container_runtime = NULL;

static void cli_command_kubectl_help(Cli* cli) {
    UNUSED(cli);
    printf("Kubernetes-inspired Container Management\r\n");
    printf("Usage:\r\n");
    printf("  kubectl start <name> <image> [args] - Start container\r\n");
    printf("  kubectl stop <name> - Stop container\r\n");
    printf("  kubectl list - List containers\r\n");
    printf("  kubectl apply <manifest> - Apply manifest\r\n");
    printf("  kubectl health - Check container runtime health\r\n");
    printf("  kubectl debug <name> - Debug container\r\n");
}

// Memory-efficient container lookup by name
static Container* find_container_by_name(ContainerRuntime* runtime, const char* name) {
    if(!runtime || !name) return NULL;
    
    // This implementation assumes container_runtime has containers in an array 
    // that can be accessed directly. We iterate through all containers to find a match.
    
    for(uint8_t i = 0; i < 8; i++) { // Use the constant directly to save memory
        Container* container = &runtime->containers[i];
        if(container->config.name != NULL && strcmp(container->config.name, name) == 0) {
            return container;
        }
    }
    
    return NULL;
}

static void cli_command_kubectl_start(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(context);

    // Lazy initialization of runtime
    if(!container_runtime) {
        container_runtime = furi_get_container_runtime();
        if(!container_runtime) {
            printf("Runtime not initialized\r\n");
            return;
        }
    }
    
    FuriString* name = furi_string_alloc();
    FuriString* image = furi_string_alloc();
    
    // Parse name and image
    bool parsed = cli_args_read_string_and_trim(args, name);
    if(parsed) {
        parsed = cli_args_read_string_and_trim(args, image);
    }
    
    if(!parsed || furi_string_empty(name) || furi_string_empty(image)) {
        printf("Usage: kubectl start <name> <image> [args]\r\n");
        printf("Example: kubectl start myapp /ext/apps/Games/snake_game.fap\r\n");
        furi_string_free(name);
        furi_string_free(image);
        return;
    }
    
    // Check if container with this name already exists
    Container* existing = find_container_by_name(container_runtime, furi_string_get_cstr(name));
    if(existing) {
        printf("Container '%s' already exists\r\n", furi_string_get_cstr(name));
        furi_string_free(name);
        furi_string_free(image);
        return;
    }
    
    // Check if FAP file exists
    if(strstr(furi_string_get_cstr(image), ".fap")) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        bool file_exists = storage_file_exists(storage, furi_string_get_cstr(image));
        furi_record_close(RECORD_STORAGE);
        
        if(!file_exists) {
            printf("FAP file not found: %s\r\n", furi_string_get_cstr(image));
            furi_string_free(name);
            furi_string_free(image);
            return;
        }
    }
    
    // Create ultra-minimal container config for extreme resource constraints
    ContainerConfig config = {
        .name = furi_string_get_cstr(name),
        .image = furi_string_get_cstr(image),
        .args = (void*)furi_string_get_cstr(args), // Pass remaining args
        .restart_on_crash = true,
        .system_container = false,
        .resource_limits = {
            .max_memory = 4 * 1024,  // 4KB - absolute minimum to function
            .cpu_time_share = 5,     // 5% CPU share - extremely minimal
            .max_threads = 1,        // Single thread only - most minimal
        }
    };
    
    // Create and start container
    Container* container = container_create(container_runtime, &config);
    if(!container) {
        printf("Failed to create container '%s'\r\n", config.name);
        furi_string_free(name);
        furi_string_free(image);
        return;
    }
    
    if(container_start(container)) {
        printf("Container '%s' started successfully\r\n", config.name);
    } else {
        printf("Failed to start container '%s'\r\n", config.name);
    }
    
    furi_string_free(name);
    furi_string_free(image);
}

static void cli_command_kubectl_stop(Cli* cli, FuriString* args) {
    UNUSED(cli);
    
    if(!container_runtime) {
        container_runtime = furi_get_container_runtime();
        if(!container_runtime) {
            printf("Runtime not initialized\r\n");
            return;
        }
    }
    
    if(furi_string_empty(args)) {
        printf("Usage: kubectl stop <name>\r\n");
        return;
    }
    
    const char* container_name = furi_string_get_cstr(args);
    Container* container = find_container_by_name(container_runtime, container_name);
    
    if(!container) {
        printf("Container '%s' not found\r\n", container_name);
        return;
    }
    
    printf("Stopping container '%s'...\r\n", container_name);
    container_stop(container, false);
    printf("Container stopped\r\n");
}

// Ultra-minimal list implementation - just show count to save memory
static void cli_command_kubectl_list(Cli* cli) {
    UNUSED(cli);
    
    if(!container_runtime) {
        container_runtime = furi_get_container_runtime();
        if(!container_runtime) {
            printf("Runtime not available\r\n");
            return;
        }
    }
    
    uint8_t count = container_runtime_get_count(container_runtime);
    printf("CONTAINERS: %u/8\r\n", count);
    
    if(count == 0) {
        printf("No containers running\r\n");
        return;
    }
    
    // Ultra-minimal implementation to save memory
    printf("%u container(s) running\r\n", count);
    printf("Use kubectl health for status\r\n");
}

// Apply a pod manifest
static void cli_command_kubectl_apply(Cli* cli, FuriString* args) {
    UNUSED(cli);
    
    if(!container_runtime) {
        container_runtime = furi_get_container_runtime();
        if(!container_runtime) {
            printf("Runtime not initialized\r\n");
            return;
        }
    }
    
    FuriString* manifest_path = furi_string_alloc();
    if(!cli_args_read_string_and_trim(args, manifest_path) || furi_string_empty(manifest_path)) {
        printf("Usage: kubectl apply <manifest>\r\n");
        furi_string_free(manifest_path);
        return;
    }
    
    // Check if manifest exists
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool exists = storage_file_exists(storage, furi_string_get_cstr(manifest_path));
    furi_record_close(RECORD_STORAGE);
    
    if(!exists) {
        printf("Manifest not found: %s\r\n", furi_string_get_cstr(manifest_path));
        furi_string_free(manifest_path);
        return;
    }
    
    // Load and apply pod manifest
    printf("Loading manifest...\r\n");
    PodManifest* manifest = pod_manifest_load_from_file(furi_string_get_cstr(manifest_path));
    if(!manifest) {
        printf("Failed to load manifest\r\n");
        furi_string_free(manifest_path);
        return;
    }
    
    // Instantiate containers from manifest
    printf("Validating...\r\n");
    Container** containers = pod_manifest_instantiate(container_runtime, manifest);
    
    if(!containers) {
        printf("Failed: verify FAP files exist\r\n");
    } else {
        printf("Applied manifest: %s\r\n", furi_string_get_cstr(manifest_path));
        
        // Get more info about what was created
        const PodContainerSpec* specs;
        uint32_t count = pod_manifest_get_containers(manifest, &specs);
        printf("Created %lu container(s)\r\n", (unsigned long)count);
        
        // Free container array
        free(containers);
    }
    
    pod_manifest_free(manifest);
    furi_string_free(manifest_path);
}

// Enhanced health check with more detailed output
static void cli_command_kubectl_health(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(args);
    UNUSED(context);
    
    if(!container_runtime) {
        container_runtime = furi_get_container_runtime();
        if(!container_runtime) {
            printf("Runtime not available\r\n");
            return;
        }
    }
    
    // Get basic stats
    uint8_t count = container_runtime_get_count(container_runtime);
    uint32_t free = memmgr_get_free_heap();
    uint32_t max_block = memmgr_heap_get_max_free_block();
    uint32_t total = memmgr_get_total_heap();
    uint32_t used = total - free;
    
    // Ultra-compact format that uses very few bytes
    printf("Containers: %u/8\r\n", count);
    printf("Memory: %lu KB used, %lu KB free (total: %lu KB)\r\n", 
           used / 1024, free / 1024, total / 1024);
    printf("Largest block: %lu KB\r\n", max_block / 1024);
    
    // Simple status indicator
    if(count >= 8) printf("Status: MAX CONTAINERS REACHED\r\n");
    else if(free < 4096) printf("Status: CRITICAL - Low memory\r\n");
    else if(free < 8192) printf("Status: WARNING - Memory pressure\r\n");
    else printf("Status: OK - Healthy\r\n");
    
    // Simple guidance for users
    if(count > 0) {
        printf("\r\nUse 'kubectl list' for container details\r\n");
    }
}

// Debug function to dump container internals - extremely helpful for troubleshooting
static void cli_command_kubectl_debug(Cli* cli, FuriString* args) {
    UNUSED(cli);
    
    if(!container_runtime) {
        container_runtime = furi_get_container_runtime();
        if(!container_runtime) {
            printf("Runtime not initialized\r\n");
            return;
        }
    }
    
    if(furi_string_empty(args)) {
        printf("Usage: kubectl debug <name>\r\n");
        return;
    }
    
    const char* container_name = furi_string_get_cstr(args);
    Container* container = find_container_by_name(container_runtime, container_name);
    
    if(!container) {
        printf("Container '%s' not found\r\n", container_name);
        return;
    }
    
    // Get container status
    ContainerStatus status;
    container_get_status(container, &status);
    
    // Print detailed container info
    printf("Container Debug: %s\r\n", container->config.name);
    printf("-------------------\r\n");
    printf("Image: %s\r\n", container->config.image);
    
    const char* state_str = "Unknown";
    switch(status.state) {
        case ContainerStatePending: state_str = "Pending"; break;
        case ContainerStateRunning: state_str = "Running"; break;
        case ContainerStatePaused: state_str = "Paused"; break;
        case ContainerStateTerminated: state_str = "Terminated"; break;
    }
    
    printf("State: %s\r\n", state_str);
    printf("Uptime: %lus\r\n", (unsigned long)status.uptime);
    printf("Restarts: %lu\r\n", (unsigned long)status.restart_count);
    printf("Memory limit: %lu bytes\r\n", (unsigned long)container->config.resource_limits.max_memory);
    printf("CPU share: %lu%%\r\n", (unsigned long)container->config.resource_limits.cpu_time_share);
    printf("Max threads: %lu\r\n", (unsigned long)container->config.resource_limits.max_threads);
    printf("Restart policy: %s\r\n", container->config.restart_on_crash ? "Yes" : "No");
    printf("System privileges: %s\r\n", container->config.system_container ? "Yes" : "No");
}

// Main command handler
static void cli_command_kubectl_callback(Cli* cli, FuriString* args, void* context) {
    if(furi_string_empty(args)) {
        cli_command_kubectl_help(cli);
        return;
    }
    
    FuriString* cmd = furi_string_alloc();
    cli_args_read_string_and_trim(args, cmd);
    
    if(furi_string_cmp_str(cmd, "help") == 0) {
        cli_command_kubectl_help(cli);
    } else if(furi_string_cmp_str(cmd, "start") == 0) {
        cli_command_kubectl_start(cli, args, context);
    } else if(furi_string_cmp_str(cmd, "stop") == 0) {
        cli_command_kubectl_stop(cli, args);
    } else if(furi_string_cmp_str(cmd, "list") == 0) {
        cli_command_kubectl_list(cli);
    } else if(furi_string_cmp_str(cmd, "apply") == 0) {
        cli_command_kubectl_apply(cli, args);
    } else if(furi_string_cmp_str(cmd, "health") == 0) {
        cli_command_kubectl_health(cli, args, context);
    } else if(furi_string_cmp_str(cmd, "debug") == 0) {
        cli_command_kubectl_debug(cli, args);
    } else {
        printf("Unknown command: %s\r\n", furi_string_get_cstr(cmd));
        cli_command_kubectl_help(cli);
    }
    
    furi_string_free(cmd);
}

// Initialize CLI commands - add kubectl command
void cli_commands_containerization_init(Cli* cli) {
    // Don't initialize runtime here - do it lazily in commands to save memory
    cli_add_command(cli, "kubectl", CliCommandFlagDefault, cli_command_kubectl_callback, NULL);
    FURI_LOG_I(TAG, "Container CLI commands initialized");
}

// Remove CLI commands
void cli_commands_containerization_deinit(Cli* cli) {
    cli_delete_command(cli, "kubectl");
    // Don't free container_runtime as it's managed by furi
}
