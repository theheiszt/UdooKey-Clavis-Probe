#ifndef ONEWIRE_SEARCH_H
#define ONEWIRE_SEARCH_H

#include <Arduino.h>

#include "hardware_shells.h"

void onewire_search_bus(uint8_t pin, Stream& port);
void onewire_read_all_temps(uint8_t pin, Stream& port);

#endif // ONEWIRE_SEARCH_H
