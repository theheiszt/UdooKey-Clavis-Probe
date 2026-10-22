-- ==============================================================================
-- UDOO KEY Companion — Automated Apple BLE & GATT Test
-- ==============================================================================
-- Scan for Apple Bluetooth Low Energy peripherals, inspect GATT characteristics,
-- correlate BLE discovery events with hardware timestamps, and export report.
-- ==============================================================================

tell application "UDOO KEY Companion"
    -- 1. Connect hardware
    connect device mock true
    
    -- 2. Start diagnostic session
    start apple session description "Apple BLE Peripheral & GATT Service Discovery"
    add marker label "BLE_SCAN_INITIATED"
    
    -- 3. Run BLE scan
    start ble scan
    
    -- 4. Query discovered BLE properties
    set bleScanning to get property "ble.scanning"
    set bleCount to get property "ble.peripherals"
    set bleDev to get property "ble.connecteddevice"
    set bleRssi to get property "ble.rssi"
    log "BLE Discovered: " & bleCount & " peripherals. Connected: " & bleDev & " (" & bleRssi & " dBm)"
    
    -- 5. Mark discovery event
    add marker label "BLE_PERIPHERAL_DISCOVERED"
    
    -- 6. Stop scan
    stop ble scan
    add marker label "BLE_SCAN_FINISHED"
    
    -- 7. Export diagnostics report
    export report to "/tmp/apple_ble_report.html" format "html"
    
    -- 8. End session
    stop apple session
    disconnect device
end tell
