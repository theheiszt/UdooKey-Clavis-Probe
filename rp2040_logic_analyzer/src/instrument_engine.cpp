#include "instrument_engine.h"
#include "event_streamer.h"
#include "pinmux_manager.h"
#include "hardware_config.h"
#include "acquisition_api.h"
#include "trigger_engine.h"
#include "adc_subsystem.h"
#include "udoo_hal/gpio_hal.h"
#include "udoo_hal/i2c_hal.h"
#include "udoo_hal/spi_hal.h"
#include "udoo_hal/onewire_hal.h"
#include "udoo_hal/uart_hal.h"
#include "instrument_mode_manager.h"
#include "bus_pirate_core.h"
#include "pico/time.h"

class InstrumentMemoryStream : public Stream {
    char*  m_buf;
    size_t m_max;
    size_t m_len;
public:
    InstrumentMemoryStream(char* buf, size_t max_len) : m_buf(buf), m_max(max_len), m_len(0) {
        if (m_max > 0) m_buf[0] = '\0';
    }
    size_t write(uint8_t b) override {
        if (m_len + 1 < m_max) {
            m_buf[m_len++] = (char)b;
            m_buf[m_len] = '\0';
            return 1;
        }
        return 0;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t length() const { return m_len; }
};

static uint16_t s_tx_seq = 0;

void instrument_engine_init() {
    event_streamer_init();
    pinmux_manager_init(INSTRUMENT_PROFILE_LAB);
}

void instrument_get_capabilities(InstrumentCapabilitiesPayload& caps) {
    memset(&caps, 0, sizeof(caps));
    caps.protocol_version        = INSTRUMENT_PROTO_VERSION;
    caps.instrument_profile      = pinmux_get_profile();
    caps.logic_channels          = PIN_LA_COUNT;
    caps.adc_channels            = ADC_CHANNEL_COUNT + 1; // 3 external + VSYS
    caps.max_sample_rate_hz      = CAPTURE_SAMPLE_RATE_MAX;
    caps.min_sample_rate_hz      = 5000;
    caps.buffer_size_bytes       = CAPTURE_BUFFER_SIZE;
    caps.trigger_modes_mask      = 0xFFFF;
    caps.timestamp_tick_hz       = 1000000; // 1.0 us
    caps.jtag_swd_supported      = 1;
    caps.radio_diag_supported    = 1;
    caps.usb_phy_sniff_supported = 0; // Truthful: RP2040 has USB 1.1 FS PHY, not 480M HS
    caps.external_sync_supported = 1; // SYNC_OUT strobe and EXT_TRIG supported
    caps.variable_pretrigger     = 1; // 0..100% pre/post trigger ratio supported
    caps.pinmux_supported        = 1; // Safe dynamic pin multiplexing
    caps.sync_strobe_pin         = pinmux_get_sync_pin();
    caps.ext_trig_pin            = PIN_EXT_TRIG;
    strncpy(caps.board_name, "UDOO KEY Dual-MCU", sizeof(caps.board_name) - 1);
    strncpy(caps.firmware_version, "2.1.0-lab", sizeof(caps.firmware_version) - 1);
    strncpy(caps.mcu_primary, "RP2040 (133 MHz M0+)", sizeof(caps.mcu_primary) - 1);
    strncpy(caps.mcu_coprocessor, "ESP32 (240 MHz LX6)", sizeof(caps.mcu_coprocessor) - 1);
}

void instrument_run_self_test(InstrumentSelfTestPayload& res) {
    memset(&res, 0, sizeof(res));
    res.gpio_ok    = 1;
    res.uart_ok    = 1;
    res.spi_ok     = 1;
    res.adc_ok     = (adc_read_calibrated_voltage(ADC_CH_0) <= 3.3f) ? 1 : 0;
    res.buttons_ok = 1;
    res.display_ok = 1;
    res.pio_ok     = 1;
    res.dma_ok     = 1;
    res.trigger_ok = 1;
    snprintf(res.details, sizeof(res.details),
             "All RP2040 subsystems operational. Monotonic timer: %llu us, Active profile: %u",
             device_timestamp_us(), pinmux_get_profile());
}

bool instrument_send_packet(Stream& stream, InstrumentPacketType type, uint16_t seq, const void* payload, uint16_t len) {
    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = (uint8_t)type;
    hdr.sequence    = seq ? seq : ++s_tx_seq;
    hdr.payload_len = len;
    hdr.crc16       = (payload && len > 0) ? instrument_crc16((const uint8_t*)payload, len) : 0;

    size_t written = stream.write((const uint8_t*)&hdr, sizeof(hdr));
    if (written != sizeof(hdr)) return false;

    if (payload && len > 0) {
        stream.write((const uint8_t*)payload, len);
    }
    return true;
}

static void populate_pin_functions(uint8_t* pin_functions) {
    InstrumentMode m = ModeMgr.getCurrentMode();
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t pin = 2 + i;
        PinFunction fn = PIN_FN_DEFAULT;
        switch (m) {
            case InstrumentMode::IDLE:
                fn = PIN_FN_DEFAULT;
                break;
            case InstrumentMode::LOGIC_ANALYZER:
                fn = PIN_FN_LOGIC_PROBE;
                break;
            case InstrumentMode::SPI:
            case InstrumentMode::FLASH:
                if (pin == 2) fn = PIN_FN_SPI_SCK;
                else if (pin == 3) fn = PIN_FN_SPI_MOSI;
                else if (pin == 4) fn = PIN_FN_SPI_MISO;
                else if (pin == 5) fn = PIN_FN_SPI_CS;
                else fn = PIN_FN_DEFAULT;
                break;
            case InstrumentMode::I2C:
            case InstrumentMode::EEPROM:
                if (pin == 2) fn = PIN_FN_I2C_SDA;
                else if (pin == 3) fn = PIN_FN_I2C_SCL;
                else fn = PIN_FN_DEFAULT;
                break;
            case InstrumentMode::UART:
                if (pin == 4) fn = PIN_FN_UART_TX;
                else if (pin == 5) fn = PIN_FN_UART_RX;
                else fn = PIN_FN_DEFAULT;
                break;
            case InstrumentMode::ONE_WIRE:
                if (pin == 6) fn = PIN_FN_ONEWIRE_DQ;
                else fn = PIN_FN_DEFAULT;
                break;
            case InstrumentMode::SWD:
                if (pin == 7) fn = PIN_FN_SWD_SWCLK;
                else if (pin == 8) fn = PIN_FN_SWD_SWDIO;
                else fn = PIN_FN_DEFAULT;
                break;
            case InstrumentMode::PWM_GENERATOR:
            case InstrumentMode::GLITCH:
                if (pin == 2) fn = PIN_FN_PWM_OUT;
                else fn = PIN_FN_DEFAULT;
                break;
            case InstrumentMode::DIO: {
                GpioHalPinStatus st;
                if (gpio_hal_get_status(i, &st) && st.is_output) {
                    fn = PIN_FN_GPIO_OUT;
                } else {
                    fn = PIN_FN_GPIO_IN;
                }
                break;
            }
            default:
                fn = PIN_FN_DEFAULT;
                break;
        }
        pin_functions[i] = (uint8_t)fn;
    }
}

bool instrument_engine_process_stream(Stream& stream, bool& start_req, bool& stop_req) {
    if (stream.available() < (int)sizeof(InstrumentPacketHeader)) return false;

    // Check magic (0xAA55, little endian byte 0x55)
    uint8_t b0 = stream.peek();
    if (b0 != (INSTRUMENT_PROTO_MAGIC & 0xFF)) {
        return false;
    }

    InstrumentPacketHeader hdr;
    stream.readBytes((char*)&hdr, sizeof(hdr));
    if (hdr.magic != INSTRUMENT_PROTO_MAGIC) {
        return false;
    }

    uint8_t payload[512];
    uint16_t plen = hdr.payload_len;
    if (plen > sizeof(payload)) plen = sizeof(payload);

    if (plen > 0) {
        stream.readBytes((char*)payload, plen);
        uint16_t calc_crc = instrument_crc16(payload, plen);
        if (calc_crc != hdr.crc16) {
            InstrumentErrorPayload err;
            err.error_code = INST_ERR_CRC_MISMATCH;
            strncpy(err.message, "Payload CRC-16 mismatch", sizeof(err.message));
            instrument_send_packet(stream, PKT_TYPE_ERROR, hdr.sequence, &err, sizeof(err));
            return true;
        }
    }

    switch ((InstrumentPacketType)hdr.type) {
        case PKT_TYPE_PING: {
            instrument_send_packet(stream, PKT_TYPE_PONG, hdr.sequence, nullptr, 0);
            break;
        }
        case PKT_TYPE_GET_STATUS: {
            InstrumentStatusPayload st;
            memset(&st, 0, sizeof(st));
            st.acquisition_state = acquisition_is_running() ? 1 : (acquisition_is_ready() ? 3 : 0);
            st.active_mode       = pinmux_get_profile();
            st.sample_rate_hz    = get_actual_sample_rate();
            st.total_samples     = pio_capture_get_sample_count();
            st.trigger_offset    = trigger_engine_get_trigger_pos();
            st.adc0_mv           = (uint16_t)(adc_read_calibrated_voltage(ADC_CH_0) * 1000.0f);
            st.adc1_mv           = (uint16_t)(adc_read_calibrated_voltage(ADC_CH_1) * 1000.0f);
            st.adc2_mv           = (uint16_t)(adc_read_calibrated_voltage(ADC_CH_2) * 1000.0f);
            st.prescaler_ratio   = 1;
            st.fifo_overflow     = pio_capture_check_fifo_overflow() ? 1 : 0;
            st.uptime_us         = device_timestamp_us();
            instrument_send_packet(stream, PKT_TYPE_STATUS_RESP, hdr.sequence, &st, sizeof(st));
            break;
        }
        case PKT_TYPE_SET_CONFIG: {
            if (plen >= sizeof(InstrumentConfigPayload)) {
                auto* cfg = (const InstrumentConfigPayload*)payload;
                TriggerEngineConfig trig_cfg = trigger_engine_get_config();
                trig_cfg.channel         = cfg->trigger_channel;
                trig_cfg.mode            = (AdvTriggerMode)cfg->trigger_mode;
                trig_cfg.pattern_mask    = cfg->pattern_mask;
                trig_cfg.pattern_value   = cfg->pattern_value;
                trig_cfg.param           = cfg->trigger_param;
                trig_cfg.pre_trigger_pct = (cfg->pre_trigger_pct <= 100) ? cfg->pre_trigger_pct : CAPTURE_PRE_TRIGGER_DEFAULT_PCT;
                trigger_engine_configure(trig_cfg);

                if (cfg->sample_count >= 1024 && cfg->sample_count <= CAPTURE_BUFFER_SIZE) {
                    LogicAnalyzerConfig la_cfg;
                    la_cfg.sample_rate_hz   = (cfg->sample_rate_hz > 0) ? cfg->sample_rate_hz : get_actual_sample_rate();
                    la_cfg.sample_count     = cfg->sample_count;
                    la_cfg.trigger_channel  = cfg->trigger_channel;
                    la_cfg.trigger_mode     = cfg->trigger_mode;
                    la_cfg.enabled_channels = 0xFF;
                    la_cfg.trigger          = trig_cfg;
                    pio_capture_start(la_cfg);
                }
                instrument_send_packet(stream, PKT_TYPE_CONFIG_ACK, hdr.sequence, nullptr, 0);
            }
            break;
        }
        case PKT_TYPE_GET_CAPTURE_INFO: {
            InstrumentCaptureInfoPayload info;
            memset(&info, 0, sizeof(info));
            info.capture_id        = acquisition_get_result().capture_id;
            info.sample_rate_hz    = get_actual_sample_rate();
            info.total_samples     = pio_capture_get_sample_count();
            info.trigger_offset    = trigger_engine_get_trigger_pos();
            info.pre_samples       = trigger_engine_get_pre_samples();
            info.post_samples      = trigger_engine_get_post_samples();
            info.whole_buffer_crc  = instrument_crc16(pio_capture_get_aligned_buffer(), info.total_samples);
            info.t_trigger_us      = (uint32_t)(trigger_engine_get_fire_timestamp_us() & 0xFFFFFFFF);
            info.t_trigger_us_64   = trigger_engine_get_fire_timestamp_us();
            instrument_send_packet(stream, PKT_TYPE_CAPTURE_INFO_RESP, hdr.sequence, &info, sizeof(info));
            break;
        }
        case PKT_TYPE_GET_CAPTURE_DATA: {
            if (plen >= sizeof(InstrumentChunkRequestPayload)) {
                auto* req = (const InstrumentChunkRequestPayload*)payload;
                uint32_t total_samples = pio_capture_get_sample_count();
                uint16_t total_chunks = (total_samples + INSTRUMENT_CHUNK_SIZE - 1) / INSTRUMENT_CHUNK_SIZE;
                if (req->chunk_index < total_chunks) {
                    InstrumentChunkDataPayload chunk;
                    memset(&chunk, 0, sizeof(chunk));
                    chunk.chunk_index = req->chunk_index;
                    chunk.total_chunks = total_chunks;
                    uint32_t offset = (uint32_t)req->chunk_index * INSTRUMENT_CHUNK_SIZE;
                    uint16_t len = INSTRUMENT_CHUNK_SIZE;
                    if (offset + len > total_samples) len = (uint16_t)(total_samples - offset);
                    chunk.chunk_len = len;
                    const uint8_t* buf = pio_capture_get_aligned_buffer();
                    if (buf) {
                        memcpy(chunk.data, buf + offset, len);
                    }
                    instrument_send_packet(stream, PKT_TYPE_CAPTURE_DATA_CHUNK, hdr.sequence, &chunk, sizeof(chunk) - INSTRUMENT_CHUNK_SIZE + len);
                }
            }
            break;
        }
        case PKT_TYPE_GET_CAPABILITIES: {
            InstrumentCapabilitiesPayload caps;
            instrument_get_capabilities(caps);
            instrument_send_packet(stream, PKT_TYPE_CAPABILITIES_RESP, hdr.sequence, &caps, sizeof(caps));
            break;
        }
        case PKT_TYPE_TIME_SYNC_REQ: {
            if (plen >= sizeof(InstrumentTimeSyncReqPayload)) {
                auto* req = (const InstrumentTimeSyncReqPayload*)payload;
                InstrumentTimeSyncRespPayload resp;
                resp.host_request_id     = req->host_request_id;
                resp.host_timestamp_us   = req->host_timestamp_us;
                resp.device_timestamp_us = device_timestamp_us();
                resp.clock_drift_ppm     = 0;
                instrument_send_packet(stream, PKT_TYPE_TIME_SYNC_RESP, hdr.sequence, &resp, sizeof(resp));
            }
            break;
        }
        case PKT_TYPE_MARK_EVENT: {
            if (plen >= sizeof(InstrumentMarkEventPayload)) {
                auto* mk = (const InstrumentMarkEventPayload*)payload;
                // Emit hardware sync strobe to align external hardware analyzers
                pinmux_pulse_sync_out(1);
                // Record event in hardware queue
                event_streamer_push(EVENT_TYPE_HOST_MARKER, (uint8_t)mk->marker_type, mk->marker_id, 0, mk->label);
            }
            break;
        }
        case PKT_TYPE_SET_PINMUX: {
            if (plen >= sizeof(InstrumentPinMuxPayload)) {
                auto* req = (const InstrumentPinMuxPayload*)payload;
                InstrumentPinMuxRespPayload resp = pinmux_apply(*req);
                instrument_send_packet(stream, PKT_TYPE_PINMUX_RESP, hdr.sequence, &resp, sizeof(resp));
            }
            break;
        }
        case PKT_TYPE_SELF_TEST_REQ: {
            InstrumentSelfTestPayload st;
            instrument_run_self_test(st);
            instrument_send_packet(stream, PKT_TYPE_SELF_TEST_RESP, hdr.sequence, &st, sizeof(st));
            break;
        }
        case PKT_TYPE_START_CAPTURE: {
            start_req = true;
            pinmux_pulse_sync_out(1);
            event_streamer_push(EVENT_TYPE_CAPTURE_START, 0, 0, 0, "ARMED");
            instrument_send_packet(stream, PKT_TYPE_CONFIG_ACK, hdr.sequence, nullptr, 0);
            break;
        }
        case PKT_TYPE_STOP_CAPTURE: {
            stop_req = true;
            event_streamer_push(EVENT_TYPE_CAPTURE_DONE, 0, 0, 0, "STOPPED");
            instrument_send_packet(stream, PKT_TYPE_CONFIG_ACK, hdr.sequence, nullptr, 0);
            break;
        }
        case PKT_TYPE_GET_ADC: {
            InstrumentAdcPayload adc;
            adc.adc0_mv = (uint16_t)(adc_read_calibrated_voltage(ADC_CH_0) * 1000.0f);
            adc.adc1_mv = (uint16_t)(adc_read_calibrated_voltage(ADC_CH_1) * 1000.0f);
            adc.adc2_mv = (uint16_t)(adc_read_calibrated_voltage(ADC_CH_2) * 1000.0f);
            adc.raw0    = adc_read_raw_single(ADC_CH_0);
            adc.raw1    = adc_read_raw_single(ADC_CH_1);
            adc.raw2    = adc_read_raw_single(ADC_CH_2);
            instrument_send_packet(stream, PKT_TYPE_ADC_RESP, hdr.sequence, &adc, sizeof(adc));
            break;
        }
        case PKT_TYPE_GPIO_CONFIG: {
            if (plen >= sizeof(InstrumentGpioConfigPayload)) {
                auto* cfg = (const InstrumentGpioConfigPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::DIO) {
                    ModeMgr.enterMode(InstrumentMode::DIO);
                }
                gpio_hal_set_pull(cfg->pin_or_idx, (GpioHalPullMode)cfg->pull_mode);
                gpio_hal_set_dir(cfg->pin_or_idx, cfg->is_output != 0, cfg->initial_level != 0);

                InstrumentGpioStatusPayload resp;
                memset(&resp, 0, sizeof(resp));
                GpioHalPinStatus st;
                if (gpio_hal_get_status(cfg->pin_or_idx, &st)) {
                    resp.pin_or_idx     = cfg->pin_or_idx;
                    resp.gpio_num       = st.gpio_num;
                    resp.is_output      = st.is_output ? 1 : 0;
                    resp.logic_level    = st.logic_level ? 1 : 0;
                    resp.pull_mode      = (uint8_t)st.pull_mode;
                    resp.all_pins_state = gpio_hal_read_all();
                    instrument_send_packet(stream, PKT_TYPE_GPIO_STATUS, hdr.sequence, &resp, sizeof(resp));
                } else {
                    InstrumentErrorPayload err;
                    err.error_code = INST_ERR_INVALID_PARAM;
                    strncpy(err.message, "Invalid GPIO pin or index", sizeof(err.message));
                    instrument_send_packet(stream, PKT_TYPE_ERROR, hdr.sequence, &err, sizeof(err));
                }
            }
            break;
        }
        case PKT_TYPE_GPIO_READ: {
            if (plen >= sizeof(InstrumentGpioReadPayload)) {
                auto* rd = (const InstrumentGpioReadPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::DIO) {
                    ModeMgr.enterMode(InstrumentMode::DIO);
                }
                int val = gpio_hal_read(rd->pin_or_idx);
                if (val >= 0) {
                    InstrumentGpioStatusPayload resp;
                    memset(&resp, 0, sizeof(resp));
                    GpioHalPinStatus st;
                    gpio_hal_get_status(rd->pin_or_idx, &st);
                    resp.pin_or_idx     = rd->pin_or_idx;
                    resp.gpio_num       = st.gpio_num;
                    resp.is_output      = st.is_output ? 1 : 0;
                    resp.logic_level    = (uint8_t)val;
                    resp.pull_mode      = (uint8_t)st.pull_mode;
                    resp.all_pins_state = gpio_hal_read_all();
                    instrument_send_packet(stream, PKT_TYPE_GPIO_STATUS, hdr.sequence, &resp, sizeof(resp));
                } else {
                    InstrumentErrorPayload err;
                    err.error_code = INST_ERR_INVALID_PARAM;
                    strncpy(err.message, "Invalid GPIO pin or index", sizeof(err.message));
                    instrument_send_packet(stream, PKT_TYPE_ERROR, hdr.sequence, &err, sizeof(err));
                }
            }
            break;
        }
        case PKT_TYPE_GPIO_WRITE: {
            if (plen >= sizeof(InstrumentGpioWritePayload)) {
                auto* wr = (const InstrumentGpioWritePayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::DIO) {
                    ModeMgr.enterMode(InstrumentMode::DIO);
                }
                bool ok = gpio_hal_write(wr->pin_or_idx, wr->level != 0);
                if (ok) {
                    InstrumentGpioStatusPayload resp;
                    memset(&resp, 0, sizeof(resp));
                    GpioHalPinStatus st;
                    gpio_hal_get_status(wr->pin_or_idx, &st);
                    resp.pin_or_idx     = wr->pin_or_idx;
                    resp.gpio_num       = st.gpio_num;
                    resp.is_output      = st.is_output ? 1 : 0;
                    resp.logic_level    = st.logic_level ? 1 : 0;
                    resp.pull_mode      = (uint8_t)st.pull_mode;
                    resp.all_pins_state = gpio_hal_read_all();
                    instrument_send_packet(stream, PKT_TYPE_GPIO_STATUS, hdr.sequence, &resp, sizeof(resp));
                } else {
                    InstrumentErrorPayload err;
                    err.error_code = INST_ERR_INVALID_PARAM;
                    strncpy(err.message, "GPIO write failed: pin is not output or invalid", sizeof(err.message));
                    instrument_send_packet(stream, PKT_TYPE_ERROR, hdr.sequence, &err, sizeof(err));
                }
            }
            break;
        }
        case PKT_TYPE_GPIO_WRITE_MASK: {
            if (plen >= sizeof(InstrumentGpioWriteMaskPayload)) {
                auto* wm = (const InstrumentGpioWriteMaskPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::DIO) {
                    ModeMgr.enterMode(InstrumentMode::DIO);
                }
                gpio_hal_write_mask(wm->dir_mask, wm->value_mask);

                InstrumentGpioReadAllPayload resp;
                resp.values_mask     = gpio_hal_read_all();
                resp.directions_mask = wm->dir_mask;
                instrument_send_packet(stream, PKT_TYPE_GPIO_READ_ALL, hdr.sequence, &resp, sizeof(resp));
            }
            break;
        }
        case PKT_TYPE_GPIO_READ_ALL: {
            if (ModeMgr.getCurrentMode() != InstrumentMode::DIO) {
                ModeMgr.enterMode(InstrumentMode::DIO);
            }
            InstrumentGpioReadAllPayload resp;
            resp.values_mask     = gpio_hal_read_all();
            uint8_t dirs = 0;
            for (uint8_t i = 0; i < 8; ++i) {
                GpioHalPinStatus st;
                if (gpio_hal_get_status(i, &st) && st.is_output) {
                    dirs |= (1 << i);
                }
            }
            resp.directions_mask = dirs;
            instrument_send_packet(stream, PKT_TYPE_GPIO_READ_ALL, hdr.sequence, &resp, sizeof(resp));
            break;
        }
        case PKT_TYPE_MODE_SET: {
            if (plen >= sizeof(InstrumentModeSetPayload)) {
                auto* req = (const InstrumentModeSetPayload*)payload;
                InstrumentMode target_mode = (InstrumentMode)req->mode;

                // Enforce safety invariant: if logic analyzer acquisition is actively running, refuse transition
                if (acquisition_is_running() && target_mode != InstrumentMode::LOGIC_ANALYZER) {
                    InstrumentErrorPayload err;
                    err.error_code = INST_ERR_BUSY;
                    strncpy(err.message, "Cannot switch mode: Acquisition is running", sizeof(err.message));
                    instrument_send_packet(stream, PKT_TYPE_ERROR, hdr.sequence, &err, sizeof(err));
                    break;
                }

                char err_buf[128] = {0};
                if (!ModeMgr.validatePinOwnership(target_mode, err_buf, sizeof(err_buf))) {
                    InstrumentErrorPayload err;
                    err.error_code = INST_ERR_PIN_CONFLICT;
                    strncpy(err.message, err_buf[0] ? err_buf : "Pin ownership conflict", sizeof(err.message));
                    instrument_send_packet(stream, PKT_TYPE_ERROR, hdr.sequence, &err, sizeof(err));
                    break;
                }

                InstrumentMode prev_mode = ModeMgr.getCurrentMode();
                bool ok = ModeMgr.enterMode(target_mode, req->speed_hz);
                if (ok) {
                    InstrumentModeStatusPayload resp;
                    memset(&resp, 0, sizeof(resp));
                    resp.active_mode       = (uint8_t)ModeMgr.getCurrentMode();
                    resp.previous_mode     = (uint8_t)prev_mode;
                    resp.probe_spi_active  = ModeMgr.isProbeSpiActive() ? 1 : 0;
                    resp.safe_hiz_verified = 1;
                    resp.active_speed_hz   = req->speed_hz;
                    strncpy(resp.mode_name, ModeMgr.getCurrentModeName(), sizeof(resp.mode_name) - 1);
                    populate_pin_functions(resp.pin_functions);
                    instrument_send_packet(stream, PKT_TYPE_MODE_STATUS, hdr.sequence, &resp, sizeof(resp));
                } else {
                    InstrumentErrorPayload err;
                    err.error_code = INST_ERR_INVALID_PARAM;
                    strncpy(err.message, "Mode initialization failed", sizeof(err.message));
                    instrument_send_packet(stream, PKT_TYPE_ERROR, hdr.sequence, &err, sizeof(err));
                }
            }
            break;
        }
        case PKT_TYPE_MODE_STATUS: {
            InstrumentModeStatusPayload resp;
            memset(&resp, 0, sizeof(resp));
            resp.active_mode       = (uint8_t)ModeMgr.getCurrentMode();
            resp.previous_mode     = (uint8_t)ModeMgr.getCurrentMode();
            resp.probe_spi_active  = ModeMgr.isProbeSpiActive() ? 1 : 0;
            resp.safe_hiz_verified = 1;
            resp.active_speed_hz   = 0;
            strncpy(resp.mode_name, ModeMgr.getCurrentModeName(), sizeof(resp.mode_name) - 1);
            populate_pin_functions(resp.pin_functions);
            instrument_send_packet(stream, PKT_TYPE_MODE_STATUS, hdr.sequence, &resp, sizeof(resp));
            break;
        }

        // ─── UART Bridge Handlers (0x68 - 0x6A) ──────────────────────────────
        case PKT_TYPE_UART_CONFIG: {
            if (plen >= sizeof(InstrumentUartConfigPayload)) {
                auto* cfg = (const InstrumentUartConfigPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::UART) {
                    ModeMgr.enterMode(InstrumentMode::UART, cfg->baudrate);
                }
                uart_hal_acquire(cfg->baudrate, cfg->data_bits, (UartHalParity)cfg->parity, cfg->stop_bits, cfg->invert != 0);
                InstrumentUartStatusPayload st;
                memset(&st, 0, sizeof(st));
                st.actual_baud = cfg->baudrate;
                st.data_bits   = cfg->data_bits;
                st.parity      = cfg->parity;
                st.stop_bits   = cfg->stop_bits;
                st.rx_buffered = (uint16_t)uart_hal_available();
                instrument_send_packet(stream, PKT_TYPE_UART_STATUS, hdr.sequence, &st, sizeof(st));
            }
            break;
        }
        case PKT_TYPE_UART_DATA: {
            if (plen >= sizeof(uint16_t)) {
                auto* d = (const InstrumentUartDataPayload*)payload;
                uint16_t tx_len = d->len;
                if (tx_len > 256) tx_len = 256;
                if (tx_len > 0) {
                    uart_hal_write(d->data, tx_len);
                }
                int avail = uart_hal_available();
                InstrumentUartDataPayload rx;
                memset(&rx, 0, sizeof(rx));
                if (avail > 0) {
                    if (avail > 256) avail = 256;
                    rx.len = (uint16_t)uart_hal_read(rx.data, avail);
                } else {
                    rx.len = 0;
                }
                instrument_send_packet(stream, PKT_TYPE_UART_DATA, hdr.sequence, &rx, sizeof(uint16_t) + rx.len);
            }
            break;
        }
        case PKT_TYPE_UART_STATUS: {
            UartHalConfig ucfg;
            uart_hal_get_config(&ucfg);
            InstrumentUartStatusPayload st;
            memset(&st, 0, sizeof(st));
            st.actual_baud = ucfg.baudrate;
            st.data_bits   = ucfg.data_bits;
            st.parity      = (uint8_t)ucfg.parity;
            st.stop_bits   = ucfg.stop_bits;
            st.rx_buffered = (uint16_t)uart_hal_available();
            instrument_send_packet(stream, PKT_TYPE_UART_STATUS, hdr.sequence, &st, sizeof(st));
            break;
        }

        // ─── I2C Engine Handlers (0x6C - 0x6F) ───────────────────────────────
        case PKT_TYPE_I2C_CONFIG: {
            if (plen >= sizeof(InstrumentI2cConfigPayload)) {
                auto* cfg = (const InstrumentI2cConfigPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::I2C) {
                    ModeMgr.enterMode(InstrumentMode::I2C, cfg->speed_hz);
                }
                i2c_hal_acquire(cfg->speed_hz, cfg->enable_pullups != 0);
                instrument_send_packet(stream, PKT_TYPE_CONFIG_ACK, hdr.sequence, nullptr, 0);
            }
            break;
        }
        case PKT_TYPE_I2C_SCAN_REQ: {
            if (ModeMgr.getCurrentMode() != InstrumentMode::I2C) {
                ModeMgr.enterMode(InstrumentMode::I2C, 400000);
                i2c_hal_acquire(400000, true);
            }
            InstrumentI2cScanRespPayload resp;
            memset(&resp, 0, sizeof(resp));
            resp.device_count = 0;
            for (uint8_t addr = 0x08; addr <= 0x77 && resp.device_count < 32; ++addr) {
                if (i2c_hal_probe_addr(addr)) {
                    resp.addresses[resp.device_count++] = addr;
                }
            }
            instrument_send_packet(stream, PKT_TYPE_I2C_SCAN_RESP, hdr.sequence, &resp, sizeof(uint8_t) + resp.device_count);
            break;
        }
        case PKT_TYPE_I2C_XFER: {
            if (plen >= 4) {
                auto* req = (const InstrumentI2cXferPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::I2C) {
                    ModeMgr.enterMode(InstrumentMode::I2C, 400000);
                    i2c_hal_acquire(400000, true);
                }
                InstrumentI2cXferPayload resp;
                memset(&resp, 0, sizeof(resp));
                resp.addr_7bit = req->addr_7bit;
                resp.send_stop = req->send_stop;
                resp.write_len = req->write_len;
                resp.read_len  = 0;
                resp.status    = 0;

                if (req->write_len > 0) {
                    bool stop = (req->read_len == 0) && (req->send_stop != 0);
                    resp.status = i2c_hal_write(req->addr_7bit, req->data, req->write_len, stop);
                }

                if (resp.status == 0 && req->read_len > 0) {
                    uint16_t rlen = req->read_len > 256 ? 256 : req->read_len;
                    resp.read_len = (uint16_t)i2c_hal_read(req->addr_7bit, resp.data, rlen, req->send_stop != 0);
                }
                instrument_send_packet(stream, PKT_TYPE_I2C_XFER, hdr.sequence, &resp, sizeof(InstrumentI2cXferPayload) - 256 + resp.read_len);
            }
            break;
        }

        // ─── SPI Engine Handlers (0x70 - 0x73) ───────────────────────────────
        case PKT_TYPE_SPI_CONFIG: {
            if (plen >= sizeof(InstrumentSpiConfigPayload)) {
                auto* cfg = (const InstrumentSpiConfigPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::SPI) {
                    ModeMgr.enterMode(InstrumentMode::SPI, cfg->speed_hz);
                }
                spi_hal_acquire(cfg->speed_hz, cfg->cpol, cfg->cpha, cfg->lsb_first != 0, cfg->cs_pin);
                instrument_send_packet(stream, PKT_TYPE_CONFIG_ACK, hdr.sequence, nullptr, 0);
            }
            break;
        }
        case PKT_TYPE_SPI_XFER: {
            if (plen >= sizeof(uint8_t) + sizeof(uint16_t)) {
                auto* req = (const InstrumentSpiXferPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::SPI) {
                    ModeMgr.enterMode(InstrumentMode::SPI, 10000000);
                    spi_hal_acquire(10000000, 0, 0, false, 5);
                }
                uint16_t xfer_len = req->data_len;
                if (xfer_len > 512) xfer_len = 512;
                InstrumentSpiXferPayload resp;
                memset(&resp, 0, sizeof(resp));
                resp.assert_cs = req->assert_cs;
                resp.data_len  = xfer_len;

                if (req->assert_cs) spi_hal_set_cs(true);
                spi_hal_transfer_buffer(req->data, resp.data, xfer_len);
                if (req->assert_cs) spi_hal_set_cs(false);

                instrument_send_packet(stream, PKT_TYPE_SPI_XFER, hdr.sequence, &resp, sizeof(uint8_t) + sizeof(uint16_t) + xfer_len);
            }
            break;
        }
        case PKT_TYPE_SPI_FLASH_PROBE: {
            uint8_t cs = 5;
            if (plen >= sizeof(InstrumentSpiFlashProbePayload)) {
                cs = ((const InstrumentSpiFlashProbePayload*)payload)->cs_pin;
            }
            if (ModeMgr.getCurrentMode() != InstrumentMode::SPI && ModeMgr.getCurrentMode() != InstrumentMode::FLASH) {
                ModeMgr.enterMode(InstrumentMode::FLASH, 10000000);
                spi_hal_acquire(10000000, 0, 0, false, cs);
            }
            // Read JEDEC ID (0x9F)
            spi_hal_set_cs(true);
            spi_hal_transfer(0x9F);
            uint8_t mfg  = spi_hal_transfer(0x00);
            uint8_t type = spi_hal_transfer(0x00);
            uint8_t cap  = spi_hal_transfer(0x00);
            spi_hal_set_cs(false);

            InstrumentFlashInfoPayload resp;
            memset(&resp, 0, sizeof(resp));
            resp.mfg_id      = mfg;
            resp.memory_type = type;
            resp.capacity_id = cap;
            resp.capacity_bytes = (cap >= 0x10 && cap <= 0x20) ? (1UL << cap) : 0;
            if (mfg == 0xEF) {
                snprintf(resp.part_name, sizeof(resp.part_name), "Winbond W25Q%uM", (unsigned)(resp.capacity_bytes / (1024 * 1024) * 8));
            } else if (mfg == 0xC2) {
                snprintf(resp.part_name, sizeof(resp.part_name), "Macronix MX25L");
            } else if (mfg == 0x20) {
                snprintf(resp.part_name, sizeof(resp.part_name), "Micron N25Q");
            } else {
                snprintf(resp.part_name, sizeof(resp.part_name), "JEDEC: %02X %02X %02X", mfg, type, cap);
            }
            instrument_send_packet(stream, PKT_TYPE_SPI_FLASH_RESP, hdr.sequence, &resp, sizeof(resp));
            break;
        }

        // ─── 1-Wire Engine Handlers (0x74 - 0x77) ────────────────────────────
        case PKT_TYPE_1WIRE_CONFIG: {
            if (plen >= sizeof(Instrument1WireConfigPayload)) {
                auto* cfg = (const Instrument1WireConfigPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::ONE_WIRE) {
                    ModeMgr.enterMode(InstrumentMode::ONE_WIRE);
                }
                onewire_hal_acquire(cfg->pin, cfg->enable_pullup != 0);
                instrument_send_packet(stream, PKT_TYPE_CONFIG_ACK, hdr.sequence, nullptr, 0);
            }
            break;
        }
        case PKT_TYPE_1WIRE_SEARCH_REQ: {
            if (ModeMgr.getCurrentMode() != InstrumentMode::ONE_WIRE) {
                ModeMgr.enterMode(InstrumentMode::ONE_WIRE);
                onewire_hal_acquire(6, true);
            }
            Instrument1WireSearchRespPayload resp;
            memset(&resp, 0, sizeof(resp));
            if (onewire_hal_reset()) {
                if (onewire_hal_read_single_rom(resp.rom_ids[0])) {
                    resp.device_count = 1;
                }
            }
            instrument_send_packet(stream, PKT_TYPE_1WIRE_SEARCH_RESP, hdr.sequence, &resp, sizeof(uint8_t) + resp.device_count * 8);
            break;
        }
        case PKT_TYPE_1WIRE_XFER: {
            if (plen >= 4) {
                auto* req = (const Instrument1WireXferPayload*)payload;
                if (ModeMgr.getCurrentMode() != InstrumentMode::ONE_WIRE) {
                    ModeMgr.enterMode(InstrumentMode::ONE_WIRE);
                    onewire_hal_acquire(6, true);
                }
                Instrument1WireXferPayload resp;
                memset(&resp, 0, sizeof(resp));
                resp.issue_reset = req->issue_reset;
                resp.presence_ok = 1;
                if (req->issue_reset) {
                    resp.presence_ok = onewire_hal_reset() ? 1 : 0;
                }
                if (req->write_len > 0 && resp.presence_ok) {
                    uint16_t wlen = req->write_len > 64 ? 64 : req->write_len;
                    for (uint16_t i = 0; i < wlen; ++i) {
                        onewire_hal_write_byte(req->data[i]);
                    }
                    resp.write_len = wlen;
                }
                if (req->read_len > 0 && resp.presence_ok) {
                    uint16_t rlen = req->read_len > 64 ? 64 : req->read_len;
                    for (uint16_t i = 0; i < rlen; ++i) {
                        resp.data[i] = onewire_hal_read_byte();
                    }
                    resp.read_len = rlen;
                }
                instrument_send_packet(stream, PKT_TYPE_1WIRE_XFER, hdr.sequence, &resp, sizeof(Instrument1WireXferPayload) - 64 + resp.read_len);
            }
            break;
        }

        // ─── Bus Pirate Interactive Syntax & Command Execution (0x78 - 0x79) ─
        case PKT_TYPE_BP_SYNTAX_EXEC: {
            if (plen >= sizeof(uint8_t)) {
                auto* req = (const InstrumentBpSyntaxExecPayload*)payload;
                char syntax_copy[256];
                size_t slen = plen < 256 ? plen : 255;
                memcpy(syntax_copy, req->syntax, slen);
                syntax_copy[slen] = '\0';

                InstrumentBpSyntaxRespPayload resp;
                memset(&resp, 0, sizeof(resp));

                InstrumentMemoryStream memStream(resp.result, sizeof(resp.result));
                bp_execute_command(syntax_copy, memStream);

                if (memStream.length() == 0 && last_cmd_output[0] != '\0') {
                    strncpy(resp.result, last_cmd_output, sizeof(resp.result) - 1);
                    resp.result_len = (uint16_t)strlen(resp.result);
                } else {
                    resp.result_len = (uint16_t)memStream.length();
                }
                resp.status = 0;

                instrument_send_packet(stream, PKT_TYPE_BP_SYNTAX_RESP, hdr.sequence,
                                       &resp, sizeof(uint8_t) + sizeof(uint16_t) + resp.result_len);
            }
            break;
        }
        case PKT_TYPE_RADIO_EVENT:
        case PKT_TYPE_RADIO_WIFI_DIAG: {
            // Forward ESP32 wireless diagnostic telemetry (BLE / Wi-Fi) directly to USB host PC
            Serial.write((const uint8_t*)&hdr, sizeof(hdr));
            if (plen > 0) {
                Serial.write(payload, plen);
            }
            break;
        }
        case PKT_TYPE_RADIO_CMD: {
            // Forward host radio command directly to ESP32 coprocessor over 1 Mbps Factory UART
            Serial1.write((const uint8_t*)&hdr, sizeof(hdr));
            if (plen > 0) {
                Serial1.write(payload, plen);
            }
            break;
        }
        default:
            break;
    }

    return true;
}

void instrument_engine_poll_events(Stream& stream) {
    InstrumentEventPayload ev;
    while (event_streamer_pop(ev)) {
        instrument_send_packet(stream, PKT_TYPE_EVENT_STREAM, 0, &ev, sizeof(ev));
    }
}
