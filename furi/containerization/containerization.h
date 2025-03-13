/**
 * @file containerization.h
 * @brief Flipper Zero Lightweight Containerization System
 * 
 * This file provides a central include point for the containerization system.
 * The implementation is inspired by Kubernetes but highly optimized for
 * resource-constrained microcontroller environments.
 */

#pragma once

#include "container_runtime.h"
#include "service_registry.h"
#include "pod_manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the containerization system
 * 
 * This function initializes the container runtime and service registry.
 * It should be called during system startup.
 * 
 * @return true if initialization was successful
 */
bool containerization_init(void);

/**
 * @brief Start system containers
 * 
 * This function starts the system containers defined in the
 * system pod manifest.
 * 
 * @return true if all system containers started successfully
 */
bool containerization_start_system_containers(void);

/**
 * @brief Get container runtime statistics as a string
 * 
 * @param buffer Buffer to write statistics to
 * @param size Size of buffer
 * @return Number of bytes written to buffer
 */
size_t containerization_get_stats_string(char* buffer, size_t size);

#ifdef __cplusplus
}
#endif
