#ifndef I2C_SNIFFER_H
#define I2C_SNIFFER_H

#include <Arduino.h>

void i2c_sniffer_start(uint8_t sda_pin, uint8_t scl_pin, Stream& port);

#endif // I2C_SNIFFER_H
