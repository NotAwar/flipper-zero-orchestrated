#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

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
    // Simple stub implementation
    (void)ms;
}
