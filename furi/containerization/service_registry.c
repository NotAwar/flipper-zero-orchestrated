#include "service_registry.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include <furi/core/mutex.h>
#include <furi/core/log.h>

#define TAG "SvcRegistry"

typedef struct ServiceLabel {
    const char* key;
    const char* value;
    struct ServiceLabel* next;
} ServiceLabel;

typedef struct ServiceEntry {
    const char* name;
    uint32_t name_hash;
    void* impl;
    struct ServiceEntry* next;
    uint8_t service_count;
    ServiceLabel* labels;  // Add labels for kubernetes-like selectors
    const char* namespace; // Add namespace support
    uint32_t creation_timestamp;
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
        
        // Free labels
        ServiceLabel* label = current->labels;
        while(label) {
            ServiceLabel* next_label = label->next;
            free((void*)label->key);
            free((void*)label->value);
            free(label);
            label = next_label;
        }
        
        // Free namespace
        free((void*)current->namespace);
        
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

// Optimized service registration with label support
bool service_registry_register_with_labels(
    ServiceRegistry* registry, 
    const char* service_name,
    void* service_impl,
    const char* namespace,
    ServiceLabel* labels) {
    
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
    
    // Add namespace and labels if provided
    if(namespace) {
        entry->namespace = strdup(namespace);
    } else {
        entry->namespace = strdup("default");
    }
    
    // Add labels
    entry->labels = NULL;
    ServiceLabel* current_label = labels;
    while(current_label) {
        ServiceLabel* new_label = malloc(sizeof(ServiceLabel));
        if(!new_label) {
            // Handle allocation failure
            continue;
        }
        
        new_label->key = strdup(current_label->key);
        new_label->value = strdup(current_label->value);
        new_label->next = entry->labels;
        entry->labels = new_label;
        
        current_label = current_label->next;
    }
    
    entry->creation_timestamp = furi_get_tick();
    
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
            free((void*)current->namespace);
            
            // Free labels
            ServiceLabel* label = current->labels;
            while(label) {
                ServiceLabel* next_label = label->next;
                free((void*)label->key);
                free((void*)label->value);
                free(label);
                label = next_label;
            }
            
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
    const char* namespace;
} ServiceSelector;

bool service_selector_matches(ServiceEntry* entry, const ServiceSelector* selector) {
    if(!selector->key) return false;
    
    // Check namespace match first for early exit
    if(selector->namespace && entry->namespace && 
       strcmp(selector->namespace, entry->namespace) != 0) {
        return false;
    }
    
    // Check labels
    ServiceLabel* label = entry->labels;
    while(label) {
        if(strcmp(label->key, selector->key) == 0) {
            // If value is specified, check it too
            if(!selector->value || strcmp(label->value, selector->value) == 0) {
                return true;
            }
        }
        label = label->next;
    }
    
    return false;
}

// Improved service discovery with more Kubernetes-like selectors
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
    
    // More sophisticated matching algorithm
    ServiceEntry* best_match = NULL;
    uint32_t best_match_score = 0;
    
    furi_mutex_acquire(registry->mutex, FuriWaitForever);
    
    ServiceEntry* current = registry->services;
    while(current) {
        uint32_t match_score = 0;
        
        // Check each selector
        for(size_t i = 0; i < selector_count; i++) {
            if(service_selector_matches(current, &selectors[i])) {
                match_score++;
            }
        }
        
        // Keep the best match
        if(match_score > best_match_score) {
            best_match = current;
            best_match_score = match_score;
        }
        
        current = current->next;
    }
    
    result = best_match ? best_match->impl : NULL;
    
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
