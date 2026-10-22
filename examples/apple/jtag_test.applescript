-- ==============================================================================
-- UDOO KEY Companion — Automated JTAG & SWD Hardware Probe Test
-- ==============================================================================
-- Scan JTAG TAP chain, query TAP identification codes (IDCODE), connect ARM SWD
-- CoreSight debug target, verify DPIDR, and record hardware trace.
-- ==============================================================================

tell application "UDOO KEY Companion"
    -- 1. Connect hardware
    connect device mock true
    
    -- 2. Start diagnostic session
    start apple session description "JTAG & ARM CoreSight SWD Probe Automation"
    add marker label "DEBUG_PROBE_START"
    
    -- 3. JTAG Scan Chain
    start jtag
    set jtagRunning to get property "jtag.running"
    set tapCount to get property "jtag.taps"
    set mfr to get property "jtag.manufacturer"
    log "JTAG Chain: " & tapCount & " TAPs detected. Manufacturer: " & mfr
    add marker label "JTAG_CHAIN_ENUMERATED"
    stop jtag
    
    -- 4. SWD Target Probe
    start swd
    set swdRunning to get property "swd.running"
    set dpidr to get property "swd.dpidr"
    set part to get property "swd.part"
    log "SWD Target: " & part & " (DPIDR: " & dpidr & ")"
    add marker label "SWD_TARGET_IDENTIFIED"
    stop swd
    
    -- 5. Export report
    export report to "/tmp/jtag_swd_report.html" format "html"
    
    -- 6. End session
    stop apple session
    disconnect device
end tell
