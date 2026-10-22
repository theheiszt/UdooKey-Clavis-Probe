-- ==============================================================================
-- UDOO KEY Companion — Automated Network & Wi-Fi Diagnostics Test
-- ==============================================================================
-- Inspect Wi-Fi interface health, measure Ping latency, perform DNS resolution,
-- correlate network events into the unified timeline alongside hardware activity.
-- ==============================================================================

tell application "UDOO KEY Companion"
    -- 1. Connect hardware
    connect device mock true
    
    -- 2. Start session
    start apple session description "Network Latency & Wi-Fi Health Correlation"
    add marker label "NETWORK_TEST_START"
    
    -- 3. Wi-Fi Scan & Interface Telemetry
    start wifi scan
    set curSsid to get property "wifi.ssid"
    set curRssi to get property "wifi.rssi"
    set curIp to get property "wifi.ip"
    log "Wi-Fi: " & curSsid & " (IP: " & curIp & ", RSSI: " & curRssi & " dBm)"
    
    -- 4. Latency Probing
    add marker label "PING_PROBE_BEGIN"
    run ping "apple.com"
    add marker label "PING_PROBE_COMPLETE"
    
    -- 5. DNS Resolution
    add marker label "DNS_QUERY_BEGIN"
    run dns test "apple.com"
    add marker label "DNS_QUERY_COMPLETE"
    
    -- 6. Stop Wi-Fi Scan
    stop wifi scan
    
    -- 7. Export diagnostics report
    export report to "/tmp/network_health_report.html" format "html"
    
    -- 8. End session
    stop apple session
    disconnect device
end tell
