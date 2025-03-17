#!/usr/bin/env fish

# Create required core files
echo "#include <stdint.h>
#include <stdlib.h>

// Basic core API implementation
void* furi_alloc(size_t size) {
    return malloc(size);
}

void furi_free(void* ptr) {
    free(ptr);
}

uint32_t furi_get_tick(void) {
    return 0;
}

void furi_delay_ms(uint32_t ms) {
    // Just a stub
}" > furi/core/core_api.c

# Create a minimal check.h that doesn't depend on m-core.h
mkdir -p furi/core
echo "#pragma once
#include <stdint.h>
#include <stdlib.h>

// Simple assertions that don't require m-core.h
#define furi_assert(expr) ((void)0)
#define furi_check(expr) ((expr) ? (void)0 : abort())
" > furi/core/check.h

# Create minimal furi.h
echo "#pragma once
#include <stdint.h>
#include <stdbool.h>
#include \"furi/core/check.h\"

// Minimal furi.h that just includes what we need
#define FURI_LOG_E(tag, fmt, ...) ((void)0)
#define FURI_LOG_W(tag, fmt, ...) ((void)0)
#define FURI_LOG_I(tag, fmt, ...) ((void)0)
#define FURI_LOG_D(tag, fmt, ...) ((void)0)
#define UNUSED(x) (void)(x)
" > furi/furi.h

# Create empty protobuf Changelog
mkdir -p assets/protobuf
echo "Initial version" > assets/protobuf/Changelog

# Create the missing directories for build
mkdir -p build/f7-firmware-D/assets/compiled

echo "Setup complete. Now try running ./fbt fap_container_test"
