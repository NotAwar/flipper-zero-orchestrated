#include "tm2004.h"

#include <core/kernel.h>

#define TM2004_CMD_READ_STATUS    0xAA
#define TM2004_CMD_READ_MEMORY    0xF0
#define TM2004_CMD_WRITE_ROM      0x3C
#define TM2004_CMD_FINALIZATION   0x35
#define TM2004_ANSWER_READ_MEMORY 0xF5

bool tm2004_write(OneWireHost* host, const uint8_t* data, size_t data_size) {
    onewire_host_reset(host);
    onewire_host_write(host, TM2004_CMD_WRITE_ROM);
    // Starting writing from address 0x0000
    onewire_host_write(host, 0x00);
    onewire_host_write(host, 0x00);

    size_t i;
    for(i = 0; i < data_size; ++i) {
        uint8_t answer;

        onewire_host_write(host, data[i]);
        answer = onewire_host_read(host);
        // Implement CRC check
        uint8_t crc = 0;
        uint8_t data[8];
        for(uint8_t i = 0; i < 8; i++) {
            data[i] = onewire_host_read(host);
            crc = onewire_crc8(crc, data[i]);
        }
        uint8_t received_crc = onewire_host_read(host);
        if(crc != received_crc) {
            FURI_LOG_E("TM2004", "CRC check failed: %02X != %02X", crc, received_crc);
            return false;
        }

        // pulse indicating that data is correct
        furi_delay_us(600);
        onewire_host_write_bit(host, true);
        furi_delay_us(50000);

        // read written key byte
        answer = onewire_host_read(host); //-V519

        // check that written and read are same
        if(data[i] != answer) {
            break;
        }
    }

    // TODO FL-3529: Better error handling
    return i == data_size;
}
