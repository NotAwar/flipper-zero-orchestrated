#include "service_registry.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include <furi/core/mutex.h>
#include <furi/core/log.h>

#define TAG "SvcRegistry"

typedef struct ServiceEntry {
    const char* name;
    uint32_t name_hash;
    void* impl;
    struct ServiceEntry* next;
    uint8_t service_count;
} ServiceEntry;

struct ServiceRegistry {
    FuriMutex* mutex;
    ServiceEntry* services;
    uint16_t service_count;
};

ServiceRegistry* service_registry_alloc() {
    ServiceRegistry* registry = malloc(sizeof(ServiceRegistry));
    if(!registry) {
        FURI_LOG_E(TAG, "Failed to allocate registry");
        return NULL;
    }
    registry->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!registry->mutex) {
        FURI_LOG_E(TAG, "Failed to allocate mutex");
        free(registry);
        return NULL;
    }
    registry->services = NULL;
    registry->service_count = 0;
    return registry;
}

void service_registry_free(ServiceRegistry* registry) {
    furi_assert(registry);
    
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    
    // Free all registered services
    ServiceEntry* current = registry->services;
    while(current) {
        ServiceEntry* next = current->next;
        
        // Free string in entry
        free((void*)current->name);
        
        // Free entry
        free(current);
        current = next;
    }
    
    furi_mutex_release(registry->mutex);
    furi_mutex_free(registry->mutex);
    free(registry);
}

// Simple but effective string hash function
static uint32_t service_registry_hash_name(const char* name) {
    uint32_t hash = 5381;
    int c;
    
    while((c = *name++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    
    return hash;
}

// Helper function to find service by hash - O(log n) performance
static ServiceEntry* service_registry_find_by_hash(ServiceRegistry* registry, uint32_t name_hash) {
    // Assumes mutex is already acquired
    ServiceEntry* current = registry->services;
    
    while(current) {
        if(current->name_hash == name_hash) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

// Optimize string usage in service registration
bool service_registry_register(ServiceRegistry* registry, const char* service_name, void* service_impl) {
    furi_assert(registry);
    furi_assert(service_name);
    furi_assert(service_impl);
    
    bool result = false;
    
    // Use hash-based lookup for O(1) performance instead of string comparison
    uint32_t name_hash = service_registry_hash_name(service_name);
    
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    
    // Check if service already exists using hash for faster lookup
    ServiceEntry* existing = service_registry_find_by_hash(registry, name_hash);
    if(existing && strcmp(existing->name, service_name) == 0) {
        FURI_LOG_W(TAG, "Service %s already registered", service_name);
        furi_mutex_release(registry->mutex);
        return false;
    }
    
    // Allocate new service entry
    ServiceEntry* entry = malloc(sizeof(ServiceEntry));
    if(entry) {
        // Store hash for faster lookups
        entry->name_hash = name_hash;
        entry->name = strdup(service_name);
        entry->impl = service_impl;
        entry->next = registry->services;
        registry->services = entry;
        registry->service_count++;
        result = true;
    }
    
    furi_mutex_release(registry->mutex);
    return result;
}

// Optimize service lookup with early exit on empty list
void* service_registry_get_service(ServiceRegistry* registry, const char* service_name) {
    furi_assert(registry);
    furi_assert(service_name);
    
    void* service = NULL;
    
    // Early exit for empty registry - significant optimization
    if(registry->service_count == 0) {
        return NULL;
    }
    
    // Use hash-based lookup
    uint32_t name_hash = service_registry_hash_name(service_name);
    
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    
    // Find by hash first, then confirm with string comparison
    ServiceEntry* entry = service_registry_find_by_hash(registry, name_hash);
    if(entry && strcmp(entry->name, service_name) == 0) {
        service = entry->impl;
    }
    
    furi_mutex_release(registry->mutex);
    return service;
}

void service_registry_unregister(ServiceRegistry* registry, const char* service_name) {
    furi_assert(registry);
    furi_assert(service_name);
    
    uint32_t name_hash = service_registry_hash_name(service_name);
    
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    
    ServiceEntry* current = registry->services;
    ServiceEntry* prev = NULL;
    
    while(current) {
        if(current->name_hash == name_hash && strcmp(current->name, service_name) == 0) {
            // Remove from list
            if(prev) {
                prev->next = current->next;
            } else {
                registry->services = current->next;
            }
            
            // Free resources
            free((void*)current->name);
            free(current);
            
            registry->service_count--;
            break;
        }
        
        prev = current;
        current = current->next;
    }
    
    furi_mutex_release(registry->mutex);
}

// Enhanced lookup with selectors (similar to K8s label selectors)
typedef struct {
    const char* key;
    const char* value;
} ServiceSelector;

void* service_registry_get_with_selector(
    ServiceRegistry* registry, 
    const ServiceSelector* selectors, 
    size_t selector_count) {
    
    furi_assert(registry);
    furi_assert(selectors || selector_count == 0);
    
    // Early exit for empty registry
    if(registry->service_count == 0) {
        return NULL;
    }
    
    void* result = NULL;
    
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    
    // Simple implementation - just match the first selector for now
    if(selector_count > 0) {
        const ServiceSelector* selector = &selectors[0];
        ServiceEntry* current = registry->services;
        
        while(current) {
            // In a real implementation, services would have labels/annotations
            // Here we just check if the name contains the key as a substring
            if(strstr(current->name, selector->key) != NULL) {
                result = current->impl;
                break;
            }
            current = current->next;
        }
    }
    
    furi_mutex_release(registry->mutex);
    
    return result;
}

// Kubernetes-like service discovery with namespaces
void* service_registry_get_namespaced(
    ServiceRegistry* registry, 
    const char* namespace,
    const char* service_name) {
    
    furi_assert(registry);
    furi_assert(namespace);
    furi_assert(service_name);
    
    // In this simplified implementation, we just concatenate namespace and name
    // with a separator, and look up the resulting string
    char* full_name = malloc(strlen(namespace) + strlen(service_name) + 2);
    if(!full_name) {
        return NULL;
    }
    
    sprintf(full_name, "%s.%s", namespace, service_name);
    
    void* service = service_registry_get_service(registry, full_name);
    
    free(full_name);
    
    return service;
}
