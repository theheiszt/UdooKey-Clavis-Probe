#include "spiflash_full.h"

// Standard SPI Flash Opcode definitions
#define CMD_WRITE_ENABLE       0x06
#define CMD_WRITE_DISABLE      0x04
#define CMD_READ_STATUS_1      0x05
#define CMD_READ_STATUS_2      0x35
#define CMD_READ_STATUS_3      0x15
#define CMD_WRITE_STATUS_1     0x01
#define CMD_PAGE_PROGRAM       0x02
#define CMD_READ_DATA          0x03
#define CMD_FAST_READ          0x0B
#define CMD_SECTOR_ERASE_4K    0x20
#define CMD_BLOCK_ERASE_32K    0x52
#define CMD_BLOCK_ERASE_64K    0xD8
#define CMD_CHIP_ERASE         0xC7
#define CMD_JEDEC_ID           0x9F
#define CMD_READ_SFDP          0x5A

static void wait_wip_clear(uint8_t cs_pin, uint32_t timeout_ms = 5000) {
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        digitalWrite(cs_pin, LOW);
        SPI.transfer(CMD_READ_STATUS_1);
        uint8_t sr1 = SPI.transfer(0x00);
        digitalWrite(cs_pin, HIGH);
        if ((sr1 & 0x01) == 0) return; // WIP bit cleared
        delayMicroseconds(50);
    }
}

static void send_write_enable(uint8_t cs_pin) {
    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_WRITE_ENABLE);
    digitalWrite(cs_pin, HIGH);
}

void flash_full_probe(uint8_t cs_pin, Stream& port) {
    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_JEDEC_ID);
    uint8_t mfg = SPI.transfer(0x00);
    uint8_t mem_type = SPI.transfer(0x00);
    uint8_t cap_code = SPI.transfer(0x00);
    digitalWrite(cs_pin, HIGH);

    port.println("\r\n=== SPI Flash JEDEC Identification ===");
    port.printf("  Raw JEDEC ID: [0x%02X 0x%02X 0x%02X]\r\n", mfg, mem_type, cap_code);

    const char* mfg_str = "Unknown";
    switch (mfg) {
        case 0xEF: mfg_str = "Winbond Electronics"; break;
        case 0xC2: mfg_str = "Macronix (MXIC)"; break;
        case 0xC8: mfg_str = "GigaDevice"; break;
        case 0x20: mfg_str = "Micron / STMicroelectronics"; break;
        case 0xBF: mfg_str = "SST / Microchip"; break;
        case 0x1F: mfg_str = "Adesto Technologies"; break;
        case 0x9D: mfg_str = "ISSI"; break;
        case 0x01: mfg_str = "Spansion / Cypress / Infineon"; break;
        case 0x85: mfg_str = "Puya Semiconductor"; break;
        default: break;
    }
    port.printf("  Manufacturer : %s (0x%02X)\r\n", mfg_str, mfg);

    uint32_t capacity_bytes = 0;
    if (cap_code >= 0x10 && cap_code <= 0x22) {
        capacity_bytes = 1UL << cap_code;
    }

    if (capacity_bytes > 0) {
        if (capacity_bytes >= 1048576) {
            port.printf("  Capacity     : %lu MB (%lu KBytes)\r\n", capacity_bytes / 1048576, capacity_bytes / 1024);
        } else {
            port.printf("  Capacity     : %lu KBytes\r\n", capacity_bytes / 1024);
        }
    } else {
        port.println("  Capacity     : Undetermined");
    }

    flash_full_status(cs_pin, port);
}

void flash_full_status(uint8_t cs_pin, Stream& port) {
    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_READ_STATUS_1);
    uint8_t sr1 = SPI.transfer(0x00);
    digitalWrite(cs_pin, HIGH);

    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_READ_STATUS_2);
    uint8_t sr2 = SPI.transfer(0x00);
    digitalWrite(cs_pin, HIGH);

    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_READ_STATUS_3);
    uint8_t sr3 = SPI.transfer(0x00);
    digitalWrite(cs_pin, HIGH);

    port.println("\r\n--- Status Registers ---");
    port.printf("  SR1: 0x%02X [WIP=%d WEL=%d BP=%d SEC=%d TB=%d SRP0=%d]\r\n",
                sr1, (sr1 & 1), (sr1 >> 1) & 1, (sr1 >> 2) & 7,
                (sr1 >> 5) & 1, (sr1 >> 6) & 1, (sr1 >> 7) & 1);
    port.printf("  SR2: 0x%02X [SRP1=%d QE=%d LB=%d CMP=%d SUS=%d]\r\n",
                sr2, sr2 & 1, (sr2 >> 1) & 1, (sr2 >> 3) & 7,
                (sr2 >> 6) & 1, (sr2 >> 7) & 1);
    port.printf("  SR3: 0x%02X [WPS=%d DRV=%d HOLD/RST=%d]\r\n",
                sr3, (sr3 >> 2) & 1, (sr3 >> 5) & 3, (sr3 >> 7) & 1);
}

void flash_full_sfdp(uint8_t cs_pin, Stream& port) {
    uint8_t buf[24];
    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_READ_SFDP);
    SPI.transfer(0x00); // 24-bit address 0x000000
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    SPI.transfer(0x00); // Dummy byte
    for (int i = 0; i < 24; i++) {
        buf[i] = SPI.transfer(0x00);
    }
    digitalWrite(cs_pin, HIGH);

    port.println("\r\n=== SFDP Header (JEDEC JESD216) ===");
    port.printf("  Signature: %c%c%c%c\r\n", buf[0], buf[1], buf[2], buf[3]);
    if (buf[0] == 'S' && buf[1] == 'F' && buf[2] == 'D' && buf[3] == 'P') {
        port.printf("  SFDP Revision: Minor 0x%02X, Major 0x%02X\r\n", buf[4], buf[5]);
        port.printf("  Param Headers: %d\r\n", buf[6] + 1);
        port.printf("  Basic SPI Param Table Offset: 0x%02X\r\n", buf[12]);
    } else {
        port.println("  [Notice] Chip does not support standard SFDP or not detected.");
    }
}

void flash_full_read_hex(uint8_t cs_pin, uint32_t addr, uint32_t len, Stream& port) {
    if (len > 1024) len = 1024;
    port.printf("\r\n=== SPI Flash Read: 0x%06lX (%lu bytes) ===\r\n", addr, len);

    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_READ_DATA);
    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);

    uint8_t line[16];
    for (uint32_t i = 0; i < len; i += 16) {
        uint32_t chunk = (len - i < 16) ? (len - i) : 16;
        for (uint32_t j = 0; j < chunk; j++) {
            line[j] = SPI.transfer(0x00);
        }
        port.printf("  %06lX: ", addr + i);
        for (uint32_t j = 0; j < 16; j++) {
            if (j < chunk) port.printf("%02X ", line[j]);
            else port.print("   ");
            if (j == 7) port.print(" ");
        }
        port.print(" |");
        for (uint32_t j = 0; j < chunk; j++) {
            char c = (line[j] >= 32 && line[j] <= 126) ? (char)line[j] : '.';
            port.print(c);
        }
        port.println("|");
    }
    digitalWrite(cs_pin, HIGH);
}

void flash_full_dump(uint8_t cs_pin, uint32_t pages, Stream& port) {
    if (pages == 0) pages = 1;
    if (pages > 256) pages = 256; // Limit to 64KB per command
    uint32_t total_bytes = pages * 256;
    port.printf("\r\n=== Starting Flash Dump: %lu pages (%lu bytes) ===\r\n", pages, total_bytes);

    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_READ_DATA);
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    SPI.transfer(0x00);

    uint8_t page_buf[256];
    for (uint32_t p = 0; p < pages; p++) {
        for (int b = 0; b < 256; b++) {
            page_buf[b] = SPI.transfer(0x00);
        }
        // Write raw binary packet
        port.write(page_buf, 256);
    }
    digitalWrite(cs_pin, HIGH);
    port.println("\r\n[Dump Complete]");
}

void flash_full_write_byte(uint8_t cs_pin, uint32_t addr, uint8_t data, Stream& port) {
    send_write_enable(cs_pin);

    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_PAGE_PROGRAM);
    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);
    SPI.transfer(data);
    digitalWrite(cs_pin, HIGH);

    wait_wip_clear(cs_pin, 100);
    port.printf("Flash byte 0x%02X written to address 0x%06lX.\r\n", data, addr);
}

void flash_full_erase_sector(uint8_t cs_pin, uint32_t addr, Stream& port) {
    send_write_enable(cs_pin);

    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_SECTOR_ERASE_4K);
    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);
    digitalWrite(cs_pin, HIGH);

    port.printf("Erasing 4KB sector at 0x%06lX... ", addr);
    wait_wip_clear(cs_pin, 2000);
    port.println("DONE.");
}

void flash_full_erase_block(uint8_t cs_pin, uint32_t addr, Stream& port) {
    send_write_enable(cs_pin);

    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_BLOCK_ERASE_64K);
    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);
    digitalWrite(cs_pin, HIGH);

    port.printf("Erasing 64KB block at 0x%06lX... ", addr);
    wait_wip_clear(cs_pin, 5000);
    port.println("DONE.");
}

void flash_full_erase_chip(uint8_t cs_pin, Stream& port) {
    send_write_enable(cs_pin);

    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_CHIP_ERASE);
    digitalWrite(cs_pin, HIGH);

    port.print("Erasing full chip (this may take up to 30 seconds)... ");
    wait_wip_clear(cs_pin, 60000);
    port.println("DONE.");
}

void flash_full_protect(uint8_t cs_pin, bool enable, Stream& port) {
    send_write_enable(cs_pin);

    uint8_t new_sr1 = enable ? 0x1C : 0x00; // BP0..BP2 bits
    digitalWrite(cs_pin, LOW);
    SPI.transfer(CMD_WRITE_STATUS_1);
    SPI.transfer(new_sr1);
    digitalWrite(cs_pin, HIGH);

    wait_wip_clear(cs_pin, 100);
    port.printf("Flash write protection %s (SR1=0x%02X).\r\n", enable ? "ENABLED" : "DISABLED", new_sr1);
}
