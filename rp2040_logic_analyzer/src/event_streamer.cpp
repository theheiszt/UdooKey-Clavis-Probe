#include "event_streamer.h"
#include <Arduino.h>
#include "pico/time.h"

static InstrumentEventPayload s_queue[EVENT_STREAMER_QUEUE_SIZE];
static volatile size_t s_head = 0;
static volatile size_t s_tail = 0;
static volatile size_t s_count = 0;
static volatile uint32_t s_overruns = 0;

void event_streamer_init() {
    event_streamer_clear();
}

bool event_streamer_push(InstrumentEventType type, uint8_t ch, uint16_t value, uint32_t flags, const char* tag) {
    if (s_count >= EVENT_STREAMER_QUEUE_SIZE) {
        s_overruns++;
        return false;
    }

    InstrumentEventPayload& ev = s_queue[s_head];
    ev.timestamp_us   = device_timestamp_us();
    ev.event_type     = (uint8_t)type;
    ev.source_channel = ch;
    ev.raw_value      = value;
    ev.event_flags    = flags;
    if (tag) {
        strncpy(ev.tag, tag, sizeof(ev.tag) - 1);
        ev.tag[sizeof(ev.tag) - 1] = '\0';
    } else {
        ev.tag[0] = '\0';
    }

    s_head = (s_head + 1) % EVENT_STREAMER_QUEUE_SIZE;
    s_count++;
    return true;
}

bool event_streamer_pop(InstrumentEventPayload& out_event) {
    if (s_count == 0) return false;

    out_event = s_queue[s_tail];
    s_tail = (s_tail + 1) % EVENT_STREAMER_QUEUE_SIZE;
    s_count--;
    return true;
}

bool event_streamer_has_events() {
    return (s_count > 0);
}

size_t event_streamer_count() {
    return s_count;
}

uint32_t event_streamer_get_overrun_count() {
    return s_overruns;
}

void event_streamer_clear() {
    s_head = 0;
    s_tail = 0;
    s_count = 0;
    s_overruns = 0;
}
