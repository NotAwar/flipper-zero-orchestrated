#pragma once
#include <stddef.h>
#include <stdbool.h>

typedef struct ServiceRegistry ServiceRegistry;

ServiceRegistry* service_registry_alloc(void);
void service_registry_free(ServiceRegistry* registry);
bool service_registry_register(ServiceRegistry* registry, const char* service_name, void* service_impl);
void* service_registry_get_service(ServiceRegistry* registry, const char* service_name);
void service_registry_unregister(ServiceRegistry* registry, const char* service_name);
