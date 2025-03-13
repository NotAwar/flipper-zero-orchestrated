#include "service_registry.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include <furi/core/mutex.h>
#include <furi/core/log.h>

#define TAG "SvcRegistry"

typedef struct ServiceEndpoint {
    ServiceDescriptor descriptor;
    void* provider_context;
    struct ServiceEndpoint* next;
} ServiceEndpoint;

struct ServiceRegistry {
    FuriMutex* mutex;
    ServiceEndpoint* services;
};

ServiceRegistry* service_registry_alloc() {
    ServiceRegistry* registry = malloc(sizeof(ServiceRegistry));
    registry->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    registry->services = NULL;
    return registry;
}

void service_registry_free(ServiceRegistry* registry) {
    furi_assert(registry);
    
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    
    // Free all registered services
    ServiceEndpoint* current = registry->services;
    while(current) {
        ServiceEndpoint* next = current->next;
        
        // Free strings in descriptor
        free((void*)current->descriptor.name);
        free((void*)current->descriptor.namespace);
        free((void*)current->descriptor.protocol);
        
        // Free endpoint
        free(current);
        current = next;
    }
    
    furi_mutex_release(registry->mutex);
    furi_mutex_free(registry->mutex);
    free(registry);
}

// Optimize string usage in service registration
ServiceEndpoint* service_registry_register(
    ServiceRegistry* registry, 
    const ServiceDescriptor* descriptor) {
    furi_assert(registry);
    furi_assert(descriptor);
    
    ServiceEndpoint* endpoint = malloc(sizeof(ServiceEndpoint));
    if (!endpoint) return NULL;
    
    // Skip copying strings if they're guaranteed to be persistent
    endpoint->descriptor.name = descriptor->name_persistent ? 
        descriptor->name : strdup(descriptor->name);
    
    endpoint->descriptor.namespace = descriptor->namespace_persistent ? 
        descriptor->namespace : strdup(descriptor->namespace);
    
    endpoint->descriptor.protocol = descriptor->protocol_persistent ? 
        descriptor->protocol : strdup(descriptor->protocol);
    
    endpoint->descriptor.type = descriptor->type;
    endpoint->descriptor.port = descriptor->port;
    
    // Lockless update for single writer case
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    endpoint->next = registry->services;
    registry->services = endpoint;
    furi_mutex_release(registry->mutex);
    
    return endpoint;
}

void service_registry_unregister(ServiceRegistry* registry, ServiceEndpoint* endpoint) {
    furi_assert(registry);
    furi_assert(endpoint);
    
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    
    if(registry->services == endpoint) {
        // First element in the list
        registry->services = endpoint->next;
    } else {
        // Find the endpoint in the list
        ServiceEndpoint* current = registry->services;
        while(current && current->next != endpoint) {
            current = current->next;
        }
        
        if(current) {
            current->next = endpoint->next;
        }
    }
    
    furi_mutex_release(registry->mutex);
    
    // Log unregistration
    FURI_LOG_I(
        TAG, 
        "Service %s/%s unregistered", 
        endpoint->descriptor.namespace, 
        endpoint->descriptor.name);
    
    // Free descriptor strings
    free((void*)endpoint->descriptor.name);
    free((void*)endpoint->descriptor.namespace);
    free((void*)endpoint->descriptor.protocol);
    
    // Free endpoint
    free(endpoint);
}

// Optimize service lookup with early exit on empty list
ServiceEndpoint* service_registry_lookup(
    ServiceRegistry* registry, 
    const char* name, 
    const char* namespace) {
    furi_assert(registry);
    furi_assert(name);
    furi_assert(namespace);
    
    ServiceEndpoint* result = NULL;
    
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    
    // Early exit if no services to save cycles
    if(!registry->services) {
        furi_mutex_release(registry->mutex);
        return NULL;
    }
    
    // Fast path optimization for common case - system services
    if(strcmp(namespace, "system") == 0) {
        // Check for common system services by direct pointer comparison first
        static const char* common_services[] = {
            "storage", "gui", "notification", "loader"
        };
        for(uint8_t i = 0; i < sizeof(common_services)/sizeof(char*); i++) {
            if(strcmp(name, common_services[i]) == 0) {
                // Search for exact match by direct pointer comparison
                ServiceEndpoint* current = registry->services;
                while(current) {
                    if(current->descriptor.namespace == namespace || 
                       strcmp(current->descriptor.namespace, namespace) == 0) {
                        if(current->descriptor.name == name ||
                           strcmp(current->descriptor.name, name) == 0) {
                            result = current;
                            break;
                        }
                    }
                    current = current->next;
                }
                break;
            }
        }
    }
    
    // If not found with fast path, do regular search
    if(!result) {
        ServiceEndpoint* current = registry->services;
        while(current) {
            if((current->descriptor.name == name || strcmp(current->descriptor.name, name) == 0) &&
               (current->descriptor.namespace == namespace || strcmp(current->descriptor.namespace, namespace) == 0)) {
                result = current;
                break;
            }
            current = current->next;
        }
    }
    
    furi_mutex_release(registry->mutex);
    
    return result;
}

const ServiceDescriptor* service_endpoint_get_descriptor(const ServiceEndpoint* endpoint) {
    furi_assert(endpoint);
    return &endpoint->descriptor;
}

void* service_endpoint_connect(ServiceEndpoint* endpoint) {
    furi_assert(endpoint);
    
    // This is a simplified implementation
    // In a real implementation, we would create a connection to the service
    // based on its protocol, port, etc.
    
    if(endpoint->descriptor.type == ServiceTypeInternal) {
        // For internal services, just return the record
        return furi_record_open(endpoint->descriptor.name);
    }
    
    // For other service types, would implement appropriate connection logic
    FURI_LOG_W(TAG, "Connection to external/system services not implemented");
    return NULL;
}

void service_endpoint_disconnect(ServiceEndpoint* endpoint, void* handle) {
    furi_assert(endpoint);
    
    // This is a simplified implementation
    if(endpoint->descriptor.type == ServiceTypeInternal) {
        // For internal services, close the record
        furi_record_close(endpoint->descriptor.name);
    }
    
    // For other service types, would implement appropriate disconnection logic
    // e.g., closing sockets, pipes, etc.
}
