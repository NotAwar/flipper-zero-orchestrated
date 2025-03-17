#!/bin/bash

# Create protobuf version file to avoid git tag errors
echo "#pragma once" > build/f7-firmware-D/assets/compiled/protobuf_version.h
echo "#define PROTOBUF_VERSION \"0.1.0-minimal\"" >> build/f7-firmware-D/assets/compiled/protobuf_version.h

# Copy it to the standard location as well
mkdir -p assets/compiled
cp build/f7-firmware-D/assets/compiled/protobuf_version.h assets/compiled/

# Create a proper furi.h that doesn't conflict with log.h
cat > furi/furi.h << 'EOF'
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "core/check.h"

// Include log.h instead of redefining macros
#include "core/log.h"

// Define function prototypes needed by applications
void furi_delay_ms(uint32_t ms);

// Utility macros
#define UNUSED(x) (void)(x)
EOF

# Make the script executable
chmod +x ./fbt

echo "Minimal build setup complete - now try running ./fbt fap_container_test"
