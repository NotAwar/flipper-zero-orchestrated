#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// Simple assertions that don't depend on m-core.h
#define furi_assert(expr) ((void)((expr) || (abort(), 0)))
#define furi_check(expr) ((void)((expr) || (abort(), 0)))

