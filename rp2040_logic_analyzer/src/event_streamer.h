#ifndef EVENT_STREAMER_H
#define EVENT_STREAMER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "instrument_protocol.h"

#define EVENT_STREAMER_QUEUE_SIZE 64

// Initializes the hardware event queue and timestamp engine
void event_streamer_init();

// Pushes a structured instrument event with 64-bit microsecond timestamp
bool event_streamer_push(InstrumentEventType type, uint8_t ch, uint16_t value, uint32_t flags, const char* tag);

// Pops the oldest event from the queue
bool event_streamer_pop(InstrumentEventPayload& out_event);

// Checks if any events are waiting in the queue
bool event_streamer_has_events();

// Returns number of queued events
size_t event_streamer_count();

// Returns total number of overrun events dropped due to full queue
uint32_t event_streamer_get_overrun_count();

// Clears event queue and resets counters
void event_streamer_clear();

#endif // EVENT_STREAMER_H
