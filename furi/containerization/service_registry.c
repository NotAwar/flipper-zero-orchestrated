#include "include/service_registry.h"
#include <stdlib.h>

typedef struct ServiceRegistry {
    void* placeholder;
} ServiceRegistry;

ServiceRegistry* service_registry_alloc(void) {
    return calloc(1, sizeof(ServiceRegistry));
}

void service_registry_free(ServiceRegistry* registry) {
    free(registry);
}

bool service_registry_register(ServiceRegistry* registry, const char* service_name, void* service_impl) {
    (void)registry;
    (void)service_name;
    (void)service_impl;
    return false;
}

void* service_registry_get_service(ServiceRegistry* registry, const char* service_name) {
    (void)registry;
    (void)service_name;
    return NULL;
}

void service_registry_unregister(ServiceRegistry* registry, const char* service_name) {
    (void)registry;
    (void)service_name;
}
