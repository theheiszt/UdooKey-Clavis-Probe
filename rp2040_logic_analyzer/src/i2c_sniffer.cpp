#include "i2c_sniffer.h"

void i2c_sniffer_start(uint8_t sda_pin, uint8_t scl_pin, Stream& port) {
    port.println("\r\n=== I2C Bus Sniffer Active ===");
    port.printf("  Monitoring SDA on GPIO %d, SCL on GPIO %d\r\n", sda_pin, scl_pin);
    port.println("  Format: [ (START), ] (STOP), + (ACK), - (NACK)");
    port.println("  Press any key or type 'x' to exit.\r\n");

    pinMode(sda_pin, INPUT_PULLUP);
    pinMode(scl_pin, INPUT_PULLUP);

    bool in_packet = false;
    bool expect_addr = false;
    uint32_t last_activity = millis();

    // Fast bitmask access
    uint32_t sda_mask = 1UL << sda_pin;
    uint32_t scl_mask = 1UL << scl_pin;

    uint32_t last_sio = sio_hw->gpio_in;
    bool last_sda = (last_sio & sda_mask) != 0;
    bool last_scl = (last_sio & scl_mask) != 0;

    while (true) {
        if (port.available()) {
            while (port.available()) port.read();
            break;
        }

        uint32_t cur_sio = sio_hw->gpio_in;
        bool sda = (cur_sio & sda_mask) != 0;
        bool scl = (cur_sio & scl_mask) != 0;

        // Detect START Condition: SDA falling edge while SCL is HIGH
        if (last_sda && !sda && scl) {
            port.print("[");
            in_packet = true;
            expect_addr = true;
            last_activity = millis();
        }
        // Detect STOP Condition: SDA rising edge while SCL is HIGH
        else if (!last_sda && sda && scl) {
            port.println("]");
            in_packet = false;
            expect_addr = false;
            last_activity = millis();
        }

        // Detect SCL Rising Edge -> Sample Data Bit
        if (!last_scl && scl && in_packet) {
            // Read 8 data bits
            uint8_t byte_val = 0;
            byte_val |= (sda ? 1 : 0) << 7;

            for (int bit = 6; bit >= 0; bit--) {
                // Wait for SCL to fall then rise
                while ((sio_hw->gpio_in & scl_mask) != 0) {
                    if (port.available()) goto exit_sniff;
                }
                while ((sio_hw->gpio_in & scl_mask) == 0) {
                    if (port.available()) goto exit_sniff;
                }
                if ((sio_hw->gpio_in & sda_mask) != 0) {
                    byte_val |= (1 << bit);
                }
            }

            // Wait for 9th clock pulse (ACK bit)
            while ((sio_hw->gpio_in & scl_mask) != 0) {
                if (port.available()) goto exit_sniff;
            }
            while ((sio_hw->gpio_in & scl_mask) == 0) {
                if (port.available()) goto exit_sniff;
            }
            bool ack = ((sio_hw->gpio_in & sda_mask) == 0);

            if (expect_addr) {
                uint8_t addr = byte_val >> 1;
                bool read_op = (byte_val & 1) != 0;
                port.printf(" 0x%02X %s%c", addr, read_op ? "R" : "W", ack ? '+' : '-');
                expect_addr = false;
            } else {
                port.printf(" 0x%02X%c", byte_val, ack ? '+' : '-');
            }
            last_activity = millis();
        }

        last_sda = sda;
        last_scl = scl;
    }

exit_sniff:
    port.println("\r\n[I2C Sniffer Stopped]");
}
