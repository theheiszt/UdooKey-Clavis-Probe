-- ==============================================================================
-- UDOO KEY Companion — Automated Logic Analyzer Capture Test
-- ==============================================================================
-- This script configures the RP2040 logic analyzer, arms the hardware trigger,
-- creates host timeline markers, captures digital waveforms, and exports to VCD.
-- ==============================================================================

tell application "UDOO KEY Companion"
    -- 1. Connect to virtual or physical hardware
    connect device mock true
    
    -- 2. Configure capture parameters: 20 MHz sample rate, 8 channels
    set sample rate to 20000000
    set channel count to 8
    set trigger channel to 0
    set trigger mode to "Rising"
    
    -- 3. Insert host synchronization marker
    add marker label "ARM_CAPTURE_TEST"
    
    -- 4. Arm the hardware
    arm capture
    
    -- 5. Trigger and complete capture (2048 samples)
    start capture
    
    -- 6. Verify capture completion
    set capState to get property "capture.state"
    log "Capture state: " & capState
    
    -- 7. Export waveform trace to VCD
    export capture to "/tmp/logic_capture_20mhz.vcd" format "vcd"
    
    -- 8. Add completion marker
    add marker label "CAPTURE_TEST_COMPLETE"
    
    -- 9. Disconnect
    disconnect device
end tell
