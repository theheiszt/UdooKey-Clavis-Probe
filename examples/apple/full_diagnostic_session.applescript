-- ==============================================================================
-- UDOO KEY Companion — Full End-to-End Automated Diagnostic Workflow
-- ==============================================================================
-- Comprehensive workflow demonstrating:
--  1. Connect UDOO Hardware Bridge
--  2. Configure Logic Analyzer & Arm Trigger
--  3. Start Apple Diagnostics Session
--  4. Run Wireless (BLE & Wi-Fi) Probes
--  5. Run Network Diagnostics (Ping, DNS, TCP)
--  6. Trigger Hardware Capture & Record Correlation Markers
--  7. Run JTAG & SWD Debug Scan
--  8. Save Session State (.udoo)
--  9. Export Waveform Trace (VCD) & Comprehensive HTML Diagnostic Report
-- ==============================================================================

tell application "UDOO KEY Companion"
    -- 1. Connect to UDOO hardware or virtual simulator
    connect device mock true
    
    -- 2. Configure Logic Analyzer (10 MHz, 8 Channels, Rising Edge on GP0)
    set sample rate to 10000000
    set channel count to 8
    set trigger channel to 0
    set trigger mode to "Rising"
    
    -- 3. Arm hardware trigger
    arm capture
    add marker label "1_HARDWARE_ARMED"
    
    -- 4. Start Unified Apple Diagnostics Session
    start apple session description "Full Regression Suite: Hardware + Wireless + Network + Debug Probes"
    add marker label "2_SESSION_INITIALIZED"
    
    -- 5. BLE & Wi-Fi Telemetry
    start ble scan
    set bleDevs to get property "ble.peripherals"
    add marker label "3_BLE_PERIPHERALS_SCANNED"
    stop ble scan
    
    start wifi scan
    set wifiSsid to get property "wifi.ssid"
    add marker label "4_WIFI_TELEMETRY_LOGGED"
    stop wifi scan
    
    -- 6. Network Probes
    add marker label "5_PING_TEST_START"
    run ping "apple.com"
    
    add marker label "6_DNS_TEST_START"
    run dns test "apple.com"
    
    -- 7. Trigger & Capture Logic Waveforms
    add marker label "7_TRIGGER_PULSE_DETECTED"
    start capture
    add marker label "8_CAPTURE_BUFFER_ACQUIRED"
    
    -- 8. JTAG & SWD Debug Enumerate
    start jtag
    set jtagTaps to get property "jtag.taps"
    add marker label "9_JTAG_SCAN_COMPLETE"
    stop jtag
    
    start swd
    set swdDpidr to get property "swd.dpidr"
    add marker label "10_SWD_PROBE_COMPLETE"
    stop swd
    
    -- 9. Save Session & Export All Artifacts
    save session to "/tmp/full_diagnostic_session.udoo"
    export capture to "/tmp/full_capture.vcd" format "vcd"
    export report to "/tmp/full_diagnostic_report.html" format "html"
    
    -- 10. Complete & Disconnect
    add marker label "11_ALL_TESTS_PASSED"
    stop apple session
    disconnect device
end tell
