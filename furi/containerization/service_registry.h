#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Service Registry for inter-container communication
 * 
 * Provides Kubernetes-like service discovery capabilities
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ServiceRegistry ServiceRegistry;
typedef struct ServiceEndpoint ServiceEndpoint;

/** Service type */
typedef enum {
    ServiceTypeInternal, // Internal Flipper service using furi_record
    ServiceTypeExternal, // External service provided by a container
    ServiceTypeSystem,   // System-level service
} ServiceType;

/** Service descriptor */
typedef struct {
    const char* name;        // Service name
    const char* namespace;   // Service namespace
    ServiceType type;        // Service type
    uint32_t port;           // Service port (for protocols that need it)
    const char* protocol;    // Service protocol (e.g., "rpc", "serial")
    
    // Optimization flags - set these to true if strings are guaranteed to persist
    bool name_persistent;
    bool namespace_persistent;
    bool protocol_persistent;
} ServiceDescriptor;

/**
 * @brief Allocate a new service registry
 * 
 * @return ServiceRegistry* 
 */
ServiceRegistry* service_registry_alloc();

/**
 * @brief Free a service registry
 * 
 * @param registry 
 */
void service_registry_free(ServiceRegistry* registry);

/**
 * @brief Register a new service
 * 
 * @param registry 
 * @param descriptor 
 * @return ServiceEndpoint* 
 */
ServiceEndpoint* service_registry_register(
    ServiceRegistry* registry, 
    const ServiceDescriptor* descriptor);

/**
 * @brief Unregister a service
 * 
 * @param registry 
 * @param endpoint 
 */
void service_registry_unregister(ServiceRegistry* registry, ServiceEndpoint* endpoint);

/**
 * @brief Look up a service by name and namespace
 * 
 * @param registry 
 * @param name 
 * @param namespace 
 * @return ServiceEndpoint* or NULL if not found
 */
ServiceEndpoint* service_registry_lookup(
    ServiceRegistry* registry, 
    const char* name, 
    const char* namespace);

/**
 * @brief Get the service descriptor
 * 
 * @param endpoint 
 * @return const ServiceDescriptor* 
 */
const ServiceDescriptor* service_endpoint_get_descriptor(const ServiceEndpoint* endpoint);

/**
 * @brief Connect to a service endpoint
 * 
 * @param endpoint 
 * @return void* Handle to the service connection
 */
void* service_endpoint_connect(ServiceEndpoint* endpoint);

/**
 * @brief Disconnect from a service
 * 
 * @param endpoint 
 * @param handle 
 */
void service_endpoint_disconnect(ServiceEndpoint* endpoint, void* handle);

#ifdef __cplusplus
}
#endif
