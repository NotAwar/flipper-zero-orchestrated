#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct ContainerRuntime ContainerRuntime;
typedef struct ServiceRegistry ServiceRegistry;

/** Early initialization */
void furi_init();

/** Start execution */
void furi_run();

/** Clean up and exit */
void furi_exit();

// Containerization component accessors

/** Get container runtime */
ContainerRuntime* furi_get_container_runtime();

/** Get service registry */
ServiceRegistry* furi_get_service_registry();

// Include all core headers
#include <furi/core/log.h>
#include <furi/core/thread.h>
#include <furi/core/mutex.h>
#include "core/common_defines.h"
#include "core/check.h"
#include "core/event_loop.h"
#include "core/event_loop_timer.h"
#include "core/event_flag.h"
#include "core/kernel.h"
#include "core/log.h"
#include "core/memmgr.h"
#include "core/memmgr_heap.h"
#include "core/message_queue.h"
#include "core/mutex.h"
#include "core/pubsub.h"
#include "core/record.h"
#include "core/semaphore.h"
#include "core/thread.h"
#include "core/thread_list.h"
#include "core/timer.h"
#include "core/string.h"
#include "core/stream_buffer.h"

// Containerization system headers (include only when compiling with containers)
#ifdef FW_CFG_WITH_CONTAINERIZATION
#include <furi/containerization/containerization.h>
#include <furi/containerization/container_runtime.h>
#include <furi/containerization/service_registry.h>
#include <furi/containerization/pod_manifest.h>
#endif

#include <furi_hal_gpio.h>

// Workaround for math.h leaking through HAL in older versions
#include <math.h>

#ifdef __cplusplus
}
#endif
