#include "rw1990.h"

#include <core/kernel.h>

#define RW1990_1_CMD_WRITE_RECORD_FLAG 0xD1
#define RW1990_1_CMD_READ_RECORD_FLAG  0xB5
#define RW1990_1_CMD_WRITE_ROM         0xD5

#define RW1990_2_CMD_WRITE_RECORD_FLAG 0x1D
#define RW1990_2_CMD_READ_RECORD_FLAG  0x1E
#define RW1990_2_CMD_WRITE_ROM         0xD5

#define DS1990_CMD_READ_ROM 0x33

static void rw1990_write_byte(OneWireHost* host, uint8_t value) {
    for(uint8_t bitMask = 0x01; bitMask; bitMask <<= 1) {
        onewire_host_write_bit(host, (bool)(bitMask & value));
        furi_delay_us(5000);
    }
}

static bool rw1990_read_and_compare(OneWireHost* host, const uint8_t* data, size_t data_size) {
    bool success = false;

    if(onewire_host_reset(host)) {
        success = true;
        onewire_host_write(host, DS1990_CMD_READ_ROM);

        for(size_t i = 0; i < data_size; ++i) {
            if(data[i] != onewire_host_read(host)) {
                success = false;
                break;
            }
        }
    }

    return success;
}

bool rw1990_write_v1(OneWireHost* host, const uint8_t* data, size_t data_size) {
    // Unlock sequence
    onewire_host_reset(host);
    onewire_host_write(host, RW1990_1_CMD_WRITE_RECORD_FLAG);
    furi_delay_us(10);

    onewire_host_write_bit(host, false);
    furi_delay_us(5000);

    // Write data
    onewire_host_reset(host);
    onewire_host_write(host, RW1990_1_CMD_WRITE_ROM);

    for(size_t i = 0; i < data_size; ++i) {
        // inverted key for RW1990.1
        rw1990_write_byte(host, ~(data[i]));
        furi_delay_us(30000);
    }

    // Lock sequence
    onewire_host_write(host, RW1990_1_CMD_WRITE_RECORD_FLAG);

    onewire_host_write_bit(host, true);
    furi_delay_us(10000);

    // Enhanced error handling
    int retry_count = 0;
    bool check = false;
    while(!check) {
        if(!onewire_host_reset(host)) {
            return false;
        }
        onewire_host_write(host, DS1990_CMD_READ_ROM);
        uint8_t first_rb = 0;
        for(size_t i = 0; i < data_size; ++i) {
            first_rb = onewire_host_read(host);
            if(first_rb != data[i]) {
                break;
            }
        }
        check = (first_rb == data[data_size - 1]);
        if(!check) {
            FURI_LOG_E("RW1990", "Verification failed on write attempt %d", ++retry_count);
            if(retry_count >= 3) {
                FURI_LOG_E("RW1990", "Maximum retries exceeded");
                return false;
            }
            furi_delay_ms(10);
        }
    }

    return true;
}

bool rw1990_write_v2(OneWireHost* host, const uint8_t* data, size_t data_size) {
    // Unlock sequence
    onewire_host_reset(host);
    onewire_host_write(host, RW1990_2_CMD_WRITE_RECORD_FLAG);
    furi_delay_us(10);

    onewire_host_write_bit(host, true);
    furi_delay_us(5000);

    // Write data
    onewire_host_reset(host);
    onewire_host_write(host, RW1990_2_CMD_WRITE_ROM);

    for(size_t i = 0; i < data_size; ++i) {
        rw1990_write_byte(host, data[i]);
        furi_delay_us(30000);
    }

    // Lock sequence
    onewire_host_write(host, RW1990_2_CMD_WRITE_RECORD_FLAG);

    onewire_host_write_bit(host, false);
    furi_delay_us(10000);

    // Enhanced error handling
    int retry_count = 0;
    bool check = false;
    while(!check) {
        if(!onewire_host_reset(host)) {
            return false;
        }
        onewire_host_write(host, DS1990_CMD_READ_ROM);
        uint8_t first_rb = 0;
        for(size_t i = 0; i < data_size; ++i) {
            first_rb = onewire_host_read(host);
            if(first_rb != data[i]) {
                break;
            }
        }
        check = (first_rb == data[data_size - 1]);
        if(!check) {
            FURI_LOG_E("RW1990", "Verification failed on write attempt %d", ++retry_count);
            if(retry_count >= 3) {
                FURI_LOG_E("RW1990", "Maximum retries exceeded");
                return false;
            }
            furi_delay_ms(10);
        }
    }

    return true;
}
