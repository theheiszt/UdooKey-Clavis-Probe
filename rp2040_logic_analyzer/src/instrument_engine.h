#ifndef INSTRUMENT_ENGINE_H
#define INSTRUMENT_ENGINE_H

#include <Arduino.h>
#include "instrument_protocol.h"

void instrument_engine_init();

// Process incoming InstrumentProtocol packet from given stream
bool instrument_engine_process_stream(Stream& stream, bool& start_req, bool& stop_req);

// Populate device capabilities according to active profile & hardware state
void instrument_get_capabilities(InstrumentCapabilitiesPayload& caps);

// Execute diagnostic self-test suite
void instrument_run_self_test(InstrumentSelfTestPayload& res);

// Send a packet over the given stream
bool instrument_send_packet(Stream& stream, InstrumentPacketType type, uint16_t seq, const void* payload, uint16_t len);

// Periodic task to flush queued events to the host if stream is active
void instrument_engine_poll_events(Stream& stream);

#endif // INSTRUMENT_ENGINE_H
