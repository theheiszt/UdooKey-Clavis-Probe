-- ==============================================================================
-- UDOO KEY Companion — Automated Apple USB Diagnostics Test
-- ==============================================================================
-- Enumerate attached Apple USB devices, inspect descriptors, record USB transactions,
-- correlate with hardware logic analyzer markers, and export report.
-- ==============================================================================

tell application "UDOO KEY Companion"
    -- 1. Connect hardware
    connect device mock true
    
    -- 2. Begin Apple diagnostic session
    start apple session description "iPhone USB Enumeration & PHY Test"
    add marker label "USB_PROBE_START"
    
    -- 3. Query USB and device properties
    set devConnected to get property "device.connected"
    set devModel to get property "device.model"
    log "UDOO Status: " & devModel & " (Connected=" & devConnected & ")"
    
    -- 4. Arm logic analyzer to observe USB D+/D- transitions
    set sample rate to 20000000
    set channel count to 8
    arm capture
    
    -- 5. Complete capture and mark timeline
    start capture
    add marker label "USB_ENUMERATION_CONFIRMED"
    
    -- 6. Export HTML diagnostics report
    export report to "/tmp/apple_usb_report.html" format "html"
    
    -- 7. End session
    stop apple session
    disconnect device
end tell
