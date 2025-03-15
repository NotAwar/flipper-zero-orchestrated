#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "core/check.h"

// Import log functions rather than defining them here
#include "core/log.h"

// Define function prototypes needed by applications
void furi_delay_ms(uint32_t ms);

// Utility macros
#define UNUSED(x) (void)(x)
