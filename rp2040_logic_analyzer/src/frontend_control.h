#ifndef FRONTEND_CONTROL_H
#define FRONTEND_CONTROL_H

#include <Arduino.h>
#include "hardware_config.h"

// Hardware control GPIOs for front-end protection and prescaler
#define PIN_FRONTEND_PROT_EN   -1                   // No GPIO claimed (Safe Default)
#ifndef PIN_FRONTEND_PRESCALER
#define PIN_FRONTEND_PRESCALER PIN_PRESCALER_SEL    // Hardware prescaler select (GP14, header P1.19)
#endif

void frontend_init();

// Toggle active protection / level shifting
void frontend_set_protection(bool enable);
bool frontend_get_protection();

// Set 74HC74 prescaler division ratio (1, 2, or 4)
void frontend_set_prescaler(uint8_t ratio);
uint8_t frontend_get_prescaler();

// Compensate frequency based on active prescaler setting
float frontend_adjust_frequency(float raw_freq);

// Print status to stream
void frontend_print_status(Stream& out);

#endif // FRONTEND_CONTROL_H
