#ifndef LOGIC_SCOPE_UI_H
#define LOGIC_SCOPE_UI_H

#include <pgmspace.h>

// =============================================================================
// UDOO KEY Dual-MCU Diagnostic Instrument — Logic Scope Web Application UI
// Embedded Production Web UI (HTML5, Canvas, Modern CSS, WebSockets)
// =============================================================================

const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>UDOO KEY Logic Scope & Diagnostic Instrument</title>
  <style>
    :root {
      --bg-base: #0b1120;
      --bg-card: #131d31;
      --bg-card-alt: #1e293b;
      --bg-canvas: #060b14;
      --border: #22324e;
      --border-accent: #3b82f6;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --accent: #38bdf8;
      --accent-green: #10b981;
      --accent-red: #ef4444;
      --accent-yellow: #f59e0b;
      --accent-purple: #8b5cf6;
      --ch0: #10b981;
      --ch1: #06b6d4;
      --ch2: #eab308;
      --ch3: #f97316;
      --ch4: #ec4899;
      --ch5: #a855f7;
      --ch6: #38bdf8;
      --ch7: #f1f5f9;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", monospace, sans-serif;
      background: var(--bg-base);
      color: var(--text-main);
      padding: 8px;
      user-select: none;
      -webkit-user-select: none;
    }
    header {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      gap: 8px;
      padding: 8px 12px;
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: 8px;
      margin-bottom: 8px;
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 8px;
      font-weight: 800;
      font-size: 1.05rem;
      letter-spacing: 0.5px;
      color: var(--text-main);
    }
    .brand-icon {
      color: var(--accent);
      font-size: 1.25rem;
    }
    .badge-bar {
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
      align-items: center;
    }
    .pill {
      font-size: 0.72rem;
      font-weight: 700;
      padding: 3px 8px;
      border-radius: 12px;
      display: inline-flex;
      align-items: center;
      gap: 4px;
      background: #1e293b;
      color: var(--text-muted);
      border: 1px solid var(--border);
      text-transform: uppercase;
      letter-spacing: 0.3px;
    }
    .pill.online  { background: rgba(16, 185, 129, 0.15); color: #34d399; border-color: rgba(16, 185, 129, 0.4); }
    .pill.warn    { background: rgba(245, 158, 11, 0.15); color: #fbbf24; border-color: rgba(245, 158, 11, 0.4); }
    .pill.offline { background: rgba(239, 68, 68, 0.15); color: #f87171; border-color: rgba(239, 68, 68, 0.4); }
    .pill.active  { background: rgba(56, 189, 248, 0.15); color: #38bdf8; border-color: rgba(56, 189, 248, 0.4); }
    .dot {
      width: 6px;
      height: 6px;
      border-radius: 50%;
      background: currentColor;
    }

    /* Tabs Navigation */
    .nav-tabs {
      display: flex;
      gap: 4px;
      margin-bottom: 8px;
      overflow-x: auto;
      scrollbar-width: none;
      border-bottom: 2px solid var(--border);
    }
    .nav-tabs::-webkit-scrollbar { display: none; }
    .tab-btn {
      padding: 8px 14px;
      background: var(--bg-card);
      color: var(--text-muted);
      border: none;
      font-weight: 700;
      border-radius: 6px 6px 0 0;
      cursor: pointer;
      font-size: 0.8rem;
      white-space: nowrap;
      transition: all 0.15s ease;
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .tab-btn.active {
      background: var(--border-accent);
      color: #ffffff;
      box-shadow: 0 2px 8px rgba(59, 130, 246, 0.4);
    }
    .tab-btn:hover:not(.active) {
      background: #1e293b;
      color: var(--text-main);
    }

    .tab-content { display: none; }
    .tab-content.active { display: block; }

    /* Cards & Controls */
    .card {
      background: var(--bg-card);
      padding: 10px 12px;
      border-radius: 8px;
      margin-bottom: 8px;
      border: 1px solid var(--border);
    }
    label {
      font-size: 0.7rem;
      color: var(--text-muted);
      display: block;
      margin-bottom: 3px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 0.4px;
    }
    select, input, button {
      width: 100%;
      padding: 7px 10px;
      background: #090f1d;
      border: 1px solid var(--border);
      color: var(--text-main);
      border-radius: 6px;
      font-size: 0.82rem;
      font-family: inherit;
      outline: none;
      transition: border 0.15s ease;
    }
    select:focus, input:focus {
      border-color: var(--accent);
    }
    button {
      cursor: pointer;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 5px;
      transition: all 0.12s ease;
    }
    button:active { transform: scale(0.98); }
    .btn-arm     { background: #059669; color: #fff; border: 1px solid #10b981; }
    .btn-arm:hover { background: #10b981; }
    .btn-single  { background: #d97706; color: #fff; border: 1px solid #f59e0b; }
    .btn-single:hover { background: #f59e0b; }
    .btn-stop    { background: #dc2626; color: #fff; border: 1px solid #ef4444; }
    .btn-stop:hover { background: #ef4444; }
    .btn-action  { background: #1e40af; color: #fff; border: 1px solid #3b82f6; }
    .btn-action:hover { background: #2563eb; }
    .btn-export  { background: #374151; color: #e5e7eb; border: 1px solid #4b5563; }
    .btn-export:hover { background: #4b5563; color: #fff; }
    .btn-clear   { background: #475569; color: #e2e8f0; border: 1px solid #64748b; }

    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
    .grid-3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 8px; }
    .grid-4 { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
    .grid-5 { display: grid; grid-template-columns: repeat(5, 1fr); gap: 8px; }
    .grid-6 { display: grid; grid-template-columns: repeat(6, 1fr); gap: 6px; }

    @media (max-width: 768px) {
      .grid-4, .grid-5, .grid-6 { grid-template-columns: 1fr 1fr; }
    }

    /* Waveform Canvas Area */
    .scope-viewport {
      background: var(--bg-canvas);
      border-radius: 8px;
      border: 1px solid var(--border);
      overflow: hidden;
      margin-bottom: 8px;
      position: relative;
    }
    .canvas-wrapper {
      position: relative;
      width: 100%;
      height: 380px;
      cursor: crosshair;
      touch-action: none;
    }
    canvas {
      display: block;
      width: 100%;
      height: 100%;
    }

    /* Scope Top Timing Toolbar */
    .scope-toolbar {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      gap: 6px;
      background: #090f1d;
      border-bottom: 1px solid var(--border);
      padding: 6px 10px;
      font-size: 0.78rem;
      font-family: monospace;
    }
    .cursor-readouts {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: 12px;
      color: var(--accent);
    }
    .cursor-val { color: var(--text-main); font-weight: 700; }

    /* Zoom / Pan Bar */
    .zoom-bar {
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .btn-tool {
      width: auto;
      padding: 4px 8px;
      font-size: 0.72rem;
      margin: 0;
      text-transform: none;
    }

    /* Channel Controls Bar */
    .channel-legend {
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
      padding: 6px 10px;
      background: var(--bg-card);
      border-top: 1px solid var(--border);
    }
    .ch-badge {
      display: inline-flex;
      align-items: center;
      gap: 5px;
      font-size: 0.72rem;
      font-weight: 700;
      padding: 3px 8px;
      border-radius: 4px;
      background: #090f1d;
      border: 1px solid var(--border);
      cursor: pointer;
      transition: opacity 0.15s ease;
    }
    .ch-badge.disabled {
      opacity: 0.35;
      text-decoration: line-through;
    }
    .ch-dot {
      width: 8px;
      height: 8px;
      border-radius: 2px;
    }

    /* Decoders & Measurements Table */
    .table-container {
      max-height: 280px;
      overflow-y: auto;
      border-radius: 6px;
      border: 1px solid var(--border);
      background: #090f1d;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      font-size: 0.78rem;
      font-family: monospace;
      text-align: left;
    }
    th {
      background: #131d31;
      padding: 8px 10px;
      color: var(--accent);
      font-weight: 700;
      border-bottom: 1px solid var(--border);
      position: sticky;
      top: 0;
      z-index: 2;
    }
    td {
      padding: 6px 10px;
      border-bottom: 1px solid #162032;
      color: var(--text-main);
    }
    tr:hover td { background: rgba(56, 189, 248, 0.05); }

    /* Bit Pirate Terminal */
    #terminal-out {
      background: #040711;
      color: #38bdf8;
      font-family: "Cascadia Code", Consolas, "Courier New", monospace;
      font-size: 0.8rem;
      padding: 10px;
      height: 240px;
      overflow-y: auto;
      border-radius: 6px;
      border: 1px solid var(--border);
      margin-bottom: 8px;
      white-space: pre-wrap;
      line-height: 1.4;
    }
    .term-input-row {
      display: flex;
      gap: 6px;
    }

    /* Analog Gauges */
    .gauge-box {
      background: #090f1d;
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 12px;
      text-align: center;
    }
    .gauge-val {
      font-size: 1.4rem;
      font-weight: 800;
      color: var(--accent);
      margin-top: 4px;
      font-family: monospace;
    }

    /* Notifications & Alert Banners */
    .alert-banner {
      padding: 8px 12px;
      border-radius: 6px;
      font-size: 0.78rem;
      margin-bottom: 8px;
      display: none;
      align-items: center;
      gap: 6px;
    }
    .alert-banner.info { background: rgba(56, 189, 248, 0.15); color: #38bdf8; border: 1px solid rgba(56, 189, 248, 0.3); display: flex; }
    .alert-banner.err  { background: rgba(239, 68, 68, 0.15); color: #f87171; border: 1px solid rgba(239, 68, 68, 0.3); display: flex; }
  </style>
</head>
<body>

  <!-- Top Header with Status Badges -->
  <header>
    <div class="brand">
      <span class="brand-icon">&#x26A1;</span>
      <span>UDOO KEY LOGIC SCOPE</span>
    </div>
    <div class="badge-bar">
      <div id="pill-wifi" class="pill online"><span class="dot"></span>AP: 192.168.4.1</div>
      <div id="pill-ws" class="pill offline"><span class="dot"></span>WS: DISCONNECTED</div>
      <div id="pill-rp" class="pill warn"><span class="dot"></span>RP2040: CONNECTING</div>
      <div id="pill-state" class="pill active"><span class="dot"></span>STATE: IDLE</div>
    </div>
  </header>

  <!-- Navigation Tabs -->
  <div class="nav-tabs">
    <button class="tab-btn active" onclick="switchTab('scope')">&#x1F4CA; Logic Scope</button>
    <button class="tab-btn" onclick="switchTab('decoder')">&#x1F50D; Decoders</button>
    <button class="tab-btn" onclick="switchTab('measurements')">&#x1F4CF; Measurements</button>
    <button class="tab-btn" onclick="switchTab('pirate')">&#x1F4BB; Bit Pirate CLI</button>
    <button class="tab-btn" onclick="switchTab('pwm')">&#x2699; PWM Gen</button>
    <button class="tab-btn" onclick="switchTab('telemetry')">&#x1F4DF; Telemetry & ADC</button>
    <button class="tab-btn" onclick="switchTab('system')">&#x2139; System Info</button>
  </div>

  <!-- ========================================================================= -->
  <!-- TAB 1: LOGIC SCOPE                                                        -->
  <!-- ========================================================================= -->
  <div id="tab-scope" class="tab-content active">

    <!-- Primary Scope Control Panel -->
    <div class="card grid-6">
      <div>
        <label>Sample Rate</label>
        <select id="sel-rate">
          <option value="100000">100 kHz (10 &mu;s)</option>
          <option value="500000">500 kHz (2 &mu;s)</option>
          <option value="1000000">1 MHz (1 &mu;s)</option>
          <option value="2000000">2 MHz (500 ns)</option>
          <option value="5000000">5 MHz (200 ns)</option>
          <option value="10000000" selected>10 MHz (100 ns)</option>
          <option value="25000000">25 MHz (40 ns)</option>
          <option value="50000000">50 MHz (20 ns)</option>
          <option value="100000000">100 MHz (10 ns)</option>
        </select>
      </div>

      <div>
        <label>Sample Count</label>
        <select id="sel-count">
          <option value="1024">1,024</option>
          <option value="2048">2,048</option>
          <option value="4096">4,096</option>
          <option value="8192" selected>8,192</option>
          <option value="16384">16,384</option>
          <option value="32768">32,768</option>
          <option value="65536">65,536 (Max DMA)</option>
        </select>
      </div>

      <div>
        <label>Trigger Channel</label>
        <select id="sel-trig-ch">
          <option value="0" selected>CH0 (GP2)</option>
          <option value="1">CH1 (GP3)</option>
          <option value="2">CH2 (GP4)</option>
          <option value="3">CH3 (GP5)</option>
          <option value="4">CH4 (GP6)</option>
          <option value="5">CH5 (GP7)</option>
          <option value="6">CH6 (GP8)</option>
          <option value="7">CH7 (GP9)</option>
          <option value="10">EXT TRIG (GP10)</option>
        </select>
      </div>

      <div>
        <label>Trigger Condition</label>
        <select id="sel-trig-mode">
          <option value="0" selected>Auto / Immediate</option>
          <option value="1">Rising Edge (&uarr;)</option>
          <option value="2">Falling Edge (&darr;)</option>
          <option value="3">Any Edge (&updownarrow;)</option>
          <option value="4">High Level (1)</option>
          <option value="5">Low Level (0)</option>
        </select>
      </div>

      <div>
        <label>Pre-Trigger</label>
        <select id="sel-pre-trig">
          <option value="0">0% (Post only)</option>
          <option value="25">25% Pre-trigger</option>
          <option value="50">50% Center</option>
          <option value="70" selected>70% (Default)</option>
          <option value="90">90% Pre-trigger</option>
        </select>
      </div>

      <div>
        <label>Acquisition Mode</label>
        <select id="sel-acq-mode">
          <option value="single" selected>Single Shot</option>
          <option value="continuous">Continuous Run</option>
        </select>
      </div>
    </div>

    <!-- Main Action Buttons Row -->
    <div class="card grid-5" style="padding: 8px 12px; margin-bottom: 8px;">
      <button class="btn-arm" id="btn-arm" onclick="cmdArm()"><span style="font-size:1.1rem;">&#x25B6;</span> ARM / RUN</button>
      <button class="btn-single" onclick="cmdSingle()"><span style="font-size:1.1rem;">&#x26A1;</span> SINGLE</button>
      <button class="btn-stop" onclick="cmdStop()"><span style="font-size:1.1rem;">&#x25A0;</span> STOP</button>
      <button class="btn-clear" onclick="cmdClear()"><span style="font-size:1.1rem;">&#x21BA;</span> CLEAR</button>
      <div style="display:flex; gap:4px;">
        <button class="btn-export" onclick="exportData('vcd')">VCD</button>
        <button class="btn-export" onclick="exportData('csv')">CSV</button>
        <button class="btn-export" onclick="exportData('json')">JSON</button>
      </div>
    </div>

    <!-- Waveform Viewport Area -->
    <div class="scope-viewport">
      <div class="scope-toolbar">
        <div class="cursor-readouts">
          <div><span style="color:#eab308;">Cursor A:</span> <span id="lbl-cursor-a" class="cursor-val">--</span></div>
          <div><span style="color:#38bdf8;">Cursor B:</span> <span id="lbl-cursor-b" class="cursor-val">--</span></div>
          <div><span>&Delta;t:</span> <span id="lbl-delta-t" class="cursor-val">--</span></div>
          <div><span>&Delta;S:</span> <span id="lbl-delta-s" class="cursor-val">--</span></div>
          <div><span>Freq:</span> <span id="lbl-cursor-freq" class="cursor-val">--</span></div>
        </div>
        <div class="zoom-bar">
          <button class="btn-tool btn-clear" onclick="clearCursors()">Reset Cursors</button>
          <button class="btn-tool btn-export" onclick="zoomIn()">Zoom +</button>
          <button class="btn-tool btn-export" onclick="zoomOut()">Zoom -</button>
          <button class="btn-tool btn-export" onclick="zoomFit()">Fit All</button>
        </div>
      </div>

      <div class="canvas-wrapper" id="canvas-wrap">
        <canvas id="scope-canvas"></canvas>
      </div>

      <!-- Channel Legend & Visibility Toggles -->
      <div class="channel-legend" id="channel-legend">
        <!-- Rendered dynamically by JavaScript -->
      </div>
    </div>

    <!-- Quick Channel Measurements Bar -->
    <div class="card grid-4" id="quick-meas-bar">
      <div class="gauge-box" style="padding: 6px;">
        <label>Active Channel</label>
        <select id="sel-meas-ch" onchange="updateMeasurementsUI()" style="margin: 0; padding: 4px;">
          <option value="0">CH0 (GP2)</option>
          <option value="1">CH1 (GP3)</option>
          <option value="2">CH2 (GP4)</option>
          <option value="3">CH3 (GP5)</option>
          <option value="4">CH4 (GP6)</option>
          <option value="5">CH5 (GP7)</option>
          <option value="6">CH6 (GP8)</option>
          <option value="7">CH7 (GP9)</option>
        </select>
      </div>
      <div class="gauge-box" style="padding: 6px;">
        <label>Frequency</label>
        <div id="qm-freq" class="gauge-val" style="font-size:1.05rem;">-- Hz</div>
      </div>
      <div class="gauge-box" style="padding: 6px;">
        <label>Period / Duty</label>
        <div id="qm-period-duty" class="gauge-val" style="font-size:1.05rem;">-- / --%</div>
      </div>
      <div class="gauge-box" style="padding: 6px;">
        <label>Transitions</label>
        <div id="qm-edges" class="gauge-val" style="font-size:1.05rem;">-- edges</div>
      </div>
    </div>

  </div>

  <!-- ========================================================================= -->
  <!-- TAB 2: PROTOCOL DECODERS                                                  -->
  <!-- ========================================================================= -->
  <div id="tab-decoder" class="tab-content">
    <div class="card grid-4">
      <div>
        <label>Protocol</label>
        <select id="dec-proto" onchange="onDecoderConfigChange()">
          <option value="uart" selected>UART / Async Serial</option>
          <option value="spi">SPI Master/Slave</option>
          <option value="i2c">I2C Two-Wire Bus</option>
        </select>
      </div>

      <div id="dec-uart-cfg" style="display:contents;">
        <div>
          <label>TX/RX Channel</label>
          <select id="dec-uart-ch" onchange="runDecoders()">
            <option value="0" selected>CH0 (GP2)</option>
            <option value="1">CH1 (GP3)</option>
            <option value="2">CH2 (GP4)</option>
            <option value="3">CH3 (GP5)</option>
            <option value="4">CH4 (GP6)</option>
            <option value="5">CH5 (GP7)</option>
            <option value="6">CH6 (GP8)</option>
            <option value="7">CH7 (GP9)</option>
          </select>
        </div>
        <div>
          <label>Baud Rate</label>
          <select id="dec-uart-baud" onchange="runDecoders()">
            <option value="9600">9,600</option>
            <option value="19200">19,200</option>
            <option value="38400">38,400</option>
            <option value="57600">57,600</option>
            <option value="115200" selected>115,200</option>
            <option value="230400">230,400</option>
            <option value="460800">460,800</option>
            <option value="921600">921,600</option>
            <option value="1000000">1,000,000 (Native)</option>
          </select>
        </div>
        <div>
          <label>Format</label>
          <select id="dec-uart-format" onchange="runDecoders()">
            <option value="8N1" selected>8-N-1 (8 bits, No parity, 1 stop)</option>
            <option value="8E1">8-E-1 (Even parity)</option>
            <option value="8O1">8-O-1 (Odd parity)</option>
          </select>
        </div>
      </div>

      <div id="dec-spi-cfg" style="display:none;">
        <div>
          <label>SCK / MOSI / MISO / CS</label>
          <select id="dec-spi-map" onchange="runDecoders()">
            <option value="0_1_2_3" selected>SCK=CH0, MOSI=CH1, MISO=CH2, CS=CH3</option>
            <option value="2_3_4_5">SCK=CH2, MOSI=CH3, MISO=CH4, CS=CH5</option>
          </select>
        </div>
        <div>
          <label>Mode / Clock Phase</label>
          <select id="dec-spi-mode" onchange="runDecoders()">
            <option value="0" selected>Mode 0 (CPOL=0, CPHA=0)</option>
            <option value="1">Mode 1 (CPOL=0, CPHA=1)</option>
            <option value="2">Mode 2 (CPOL=1, CPHA=0)</option>
            <option value="3">Mode 3 (CPOL=1, CPHA=1)</option>
          </select>
        </div>
        <div>
          <label>Bit Order</label>
          <select id="dec-spi-endian" onchange="runDecoders()">
            <option value="msb" selected>MSB First</option>
            <option value="lsb">LSB First</option>
          </select>
        </div>
      </div>

      <div id="dec-i2c-cfg" style="display:none;">
        <div>
          <label>SCL / SDA Channel</label>
          <select id="dec-i2c-map" onchange="runDecoders()">
            <option value="0_1" selected>SCL=CH0 (GP2), SDA=CH1 (GP3)</option>
            <option value="2_3">SCL=CH2 (GP4), SDA=CH3 (GP5)</option>
            <option value="4_5">SCL=CH4 (GP6), SDA=CH5 (GP7)</option>
          </select>
        </div>
      </div>
    </div>

    <!-- Decoded Packets Table -->
    <div class="card">
      <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom: 6px;">
        <label style="margin: 0;">Decoded Packets & Stream</label>
        <span id="dec-count" style="font-size:0.75rem; color:var(--accent);">0 tokens</span>
      </div>
      <div class="table-container">
        <table>
          <thead>
            <tr>
              <th style="width: 50px;">#</th>
              <th style="width: 100px;">Time (&mu;s)</th>
              <th style="width: 100px;">Hex</th>
              <th style="width: 100px;">ASCII / Symbol</th>
              <th>Details</th>
            </tr>
          </thead>
          <tbody id="dec-table-body">
            <tr><td colspan="5" style="text-align:center; color:var(--text-muted);">Arm and capture data to decode packets</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- ========================================================================= -->
  <!-- TAB 3: MEASUREMENTS & TIMING                                              -->
  <!-- ========================================================================= -->
  <div id="tab-measurements" class="tab-content">
    <div class="card">
      <label>Complete 8-Channel Measurement Matrix</label>
      <div class="table-container" style="max-height: 360px;">
        <table>
          <thead>
            <tr>
              <th>Channel</th>
              <th>Frequency (Hz)</th>
              <th>Period (&mu;s)</th>
              <th>Duty (%)</th>
              <th>Pulse High (&mu;s)</th>
              <th>Pulse Low (&mu;s)</th>
              <th>Total Edges</th>
              <th>State</th>
            </tr>
          </thead>
          <tbody id="meas-table-body">
            <!-- Dynamically populated -->
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- ========================================================================= -->
  <!-- TAB 4: BIT PIRATE TERMINAL                                                -->
  <!-- ========================================================================= -->
  <div id="tab-pirate" class="tab-content">
    <div class="card grid-4">
      <button class="btn-action" onclick="sendCmd('test self')">Self Test 17/17</button>
      <button class="btn-action" onclick="sendCmd('scan')">I2C Scan</button>
      <button class="btn-action" onclick="sendCmd('flash probe')">SPI Flash Probe</button>
      <button class="btn-action" onclick="sendCmd('swd')">SWD Scan</button>
      <button class="btn-action" onclick="sendCmd('v')">Voltages (v)</button>
      <button class="btn-action" onclick="sendCmd('meas')">Frequency (meas)</button>
      <button class="btn-action" onclick="sendCmd('test synth 8192')">Synth 8K Test</button>
      <button class="btn-action" onclick="sendCmd('mode hiz')">Safe Hi-Z Mode</button>
    </div>
    <div class="card">
      <div id="terminal-out">UDOO KEY Dual-MCU Diagnostic Instrument Ready.
Type commands below or use quick actions above.
&gt; </div>
      <div class="term-input-row">
        <input type="text" id="term-input" placeholder="Enter command (e.g. scan, v, meas, rate, trig, help)..." onkeydown="if(event.key==='Enter') sendTermInput()">
        <button class="btn-action" style="width: 80px;" onclick="sendTermInput()">Send</button>
      </div>
    </div>
  </div>

  <!-- ========================================================================= -->
  <!-- TAB 5: PWM SIGNAL GENERATOR                                               -->
  <!-- ========================================================================= -->
  <div id="tab-pwm" class="tab-content">
    <div class="card grid-2">
      <div>
        <label>Frequency (Hz)</label>
        <input type="number" id="pwm-freq" value="1000" min="1" max="40000000">
      </div>
      <div>
        <label>Duty Cycle (%)</label>
        <input type="number" id="pwm-duty" value="50" min="0" max="100">
      </div>
      <div style="grid-column: span 2;">
        <button class="btn-arm" onclick="updatePWM()">Apply PWM Generator Settings (IO25)</button>
      </div>
    </div>
    <div class="card grid-4">
      <button class="btn-export" onclick="setPWMPreset(1000, 50)">1 kHz @ 50%</button>
      <button class="btn-export" onclick="setPWMPreset(10000, 25)">10 kHz @ 25%</button>
      <button class="btn-export" onclick="setPWMPreset(100000, 50)">100 kHz @ 50%</button>
      <button class="btn-export" onclick="setPWMPreset(1000000, 50)">1 MHz @ 50%</button>
    </div>
  </div>

  <!-- ========================================================================= -->
  <!-- TAB 6: TELEMETRY & ANALOG GAUGES                                          -->
  <!-- ========================================================================= -->
  <div id="tab-telemetry" class="tab-content">
    <div class="card grid-4">
      <div class="gauge-box">
        <label>ADC0 (LDR Sensor)</label>
        <div id="val-adc0" class="gauge-val">-- V</div>
      </div>
      <div class="gauge-box">
        <label>ADC1 (GP27 Header)</label>
        <div id="val-adc1" class="gauge-val">-- V</div>
      </div>
      <div class="gauge-box">
        <label>ADC2 (GP28 Header)</label>
        <div id="val-adc2" class="gauge-val">-- V</div>
      </div>
      <div class="gauge-box">
        <label>VSYS Rail</label>
        <div id="val-vsys" class="gauge-val">-- V</div>
      </div>
    </div>
    <div class="card grid-2">
      <button class="btn-action" onclick="sendCmd('v')">Poll ADC Voltages</button>
      <button class="btn-action" onclick="sendCmd('meas')">Poll Hardware Frequency</button>
    </div>
  </div>

  <!-- ========================================================================= -->
  <!-- TAB 7: SYSTEM INFO                                                        -->
  <!-- ========================================================================= -->
  <div id="tab-system" class="tab-content">
    <div class="card grid-2">
      <div class="gauge-box" style="text-align: left;">
        <label>ESP32 Coprocessor</label>
        <div style="font-family: monospace; font-size: 0.85rem; line-height: 1.6; margin-top: 6px;">
          <div>Chip: ESP32-D0WDQ6 (240 MHz)</div>
          <div>Wi-Fi SoftAP: UDOO-BIT-PIRATE-SCOPE</div>
          <div>IP Address: 192.168.4.1 (CH 1)</div>
          <div>Free Heap: <span id="sys-heap">-- KB</span></div>
          <div>Uptime: <span id="sys-uptime">-- s</span></div>
        </div>
      </div>
      <div class="gauge-box" style="text-align: left;">
        <label>RP2040 Primary Engine</label>
        <div style="font-family: monospace; font-size: 0.85rem; line-height: 1.6; margin-top: 6px;">
          <div>Architecture: Dual Cortex-M0+ (133 MHz)</div>
          <div>Logic Engine: PIO0 + Dual-DMA Engine</div>
          <div>Channels: GP2..GP9 (8 digital probes)</div>
          <div>Trigger: GP10 (EXT_TRIG) / Adv. Edge/Pattern</div>
          <div>Transport: 1,000,000 Baud Factory UART</div>
        </div>
      </div>
    </div>
  </div>

  <!-- ========================================================================= -->
  <!-- JAVASCRIPT APPLICATION CORE                                               -->
  <!-- ========================================================================= -->
  <script>
    // ─── Constants & State ───────────────────────────────────────────────────
    const COLORS = [
      '#10b981', // CH0 (GP2): Green
      '#06b6d4', // CH1 (GP3): Cyan
      '#eab308', // CH2 (GP4): Yellow
      '#f97316', // CH3 (GP5): Orange
      '#ec4899', // CH4 (GP6): Pink
      '#a855f7', // CH5 (GP7): Purple
      '#38bdf8', // CH6 (GP8): Sky Blue
      '#f1f5f9'  // CH7 (GP9): Silver/White
    ];

    const PIN_NAMES = ['GP2', 'GP3', 'GP4', 'GP5', 'GP6', 'GP7', 'GP8', 'GP9'];
    let chEnabled = [true, true, true, true, true, true, true, true];
    let chInverted = [false, false, false, false, false, false, false, false];

    let ws = null;
    let canvas, ctx;
    let rawBuffer = null;      // Uint8Array of samples (1 byte = 8 channels)
    let sampleRate = 10000000; // Hz
    let totalSamples = 8192;
    let triggerOffset = 0;
    let captureId = 0;
    let isArmed = false;

    // Viewport & Zoom Navigation
    let zoomLevel = 1.0;       // 1.0 = fit to canvas width
    let panOffset = 0;         // Left sample index
    let isDragging = false;
    let dragStartX = 0;
    let dragStartPan = 0;

    // Timing Cursors
    let cursorA = null;        // Sample index
    let cursorB = null;

    // Hover Crosshair
    let hoverSample = null;

    // Decoders Cache
    let decodedTokens = [];

    // ─── Startup & Initialization ───────────────────────────────────────────
    window.onload = () => {
      canvas = document.getElementById('scope-canvas');
      ctx = canvas.getContext('2d');
      setupCanvasResolution();
      renderChannelLegend();
      initEvents();
      connectWebSocket();
      drawScope();

      // Poll system info periodically
      setInterval(pollSystemStats, 5000);
    };

    function setupCanvasResolution() {
      const wrap = document.getElementById('canvas-wrap');
      const dpr = window.devicePixelRatio || 1;
      const w = wrap.clientWidth;
      const h = wrap.clientHeight;
      canvas.width = w * dpr;
      canvas.height = h * dpr;
      ctx.resetTransform && ctx.resetTransform();
      ctx.scale(dpr, dpr);
    }

    window.onresize = () => {
      setupCanvasResolution();
      drawScope();
    };

    function renderChannelLegend() {
      const container = document.getElementById('channel-legend');
      container.innerHTML = '';
      for (let ch = 0; ch < 8; ch++) {
        const badge = document.createElement('div');
        badge.className = 'ch-badge' + (chEnabled[ch] ? '' : ' disabled');
        badge.id = 'ch-badge-' + ch;
        badge.innerHTML = `<span class="ch-dot" style="background:${COLORS[ch]};"></span>CH${ch} [${PIN_NAMES[ch]}]`;
        badge.onclick = () => toggleChannel(ch);
        container.appendChild(badge);
      }
    }

    function toggleChannel(ch) {
      chEnabled[ch] = !chEnabled[ch];
      const badge = document.getElementById('ch-badge-' + ch);
      if (chEnabled[ch]) {
        badge.classList.remove('disabled');
      } else {
        badge.classList.add('disabled');
      }
      drawScope();
    }

    // ─── WebSocket Link Management ───────────────────────────────────────────
    function connectWebSocket() {
      const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = proto + '//' + window.location.host + '/ws';
      ws = new WebSocket(wsUrl);
      ws.binaryType = 'arraybuffer';

      ws.onopen = () => {
        updateBadge('pill-ws', 'online', 'WS: CONNECTED');
        updateBadge('pill-rp', 'online', 'RP2040: ONLINE');
        // Query initial status
        ws.send(JSON.stringify({ cmd: 'status' }));
      };

      ws.onclose = () => {
        updateBadge('pill-ws', 'offline', 'WS: DISCONNECTED');
        updateBadge('pill-rp', 'offline', 'RP2040: OFFLINE');
        setTimeout(connectWebSocket, 2000);
      };

      ws.onerror = () => {
        updateBadge('pill-ws', 'offline', 'WS: ERROR');
      };

      ws.onmessage = (evt) => {
        if (evt.data instanceof ArrayBuffer) {
          handleBinaryCapture(evt.data);
        } else {
          try {
            const msg = JSON.parse(evt.data);
            handleJsonMessage(msg);
          } catch(e) {
            appendTerminal(evt.data);
          }
        }
      };
    }

    function handleBinaryCapture(buf) {
      const dataView = new DataView(buf);
      let samplesStart = 0;

      // Check for WebLogicCaptureHeader (Magic: 'SCOP' = 0x53434F50)
      if (buf.byteLength >= 32 && dataView.getUint32(0, true) === 0x53434F50) {
        sampleRate    = dataView.getUint32(8, true);
        totalSamples  = dataView.getUint32(12, true);
        triggerOffset = dataView.getUint32(16, true);
        captureId     = dataView.getUint32(28, true);
        samplesStart  = 32;
        rawBuffer     = new Uint8Array(buf, samplesStart);
      } else {
        // Fallback: Raw binary sample buffer
        rawBuffer     = new Uint8Array(buf);
        totalSamples  = rawBuffer.length;
        sampleRate    = parseInt(document.getElementById('sel-rate').value) || 10000000;
        triggerOffset = Math.floor(totalSamples * (parseInt(document.getElementById('sel-pre-trig').value) / 100.0));
      }

      isArmed = false;
      updateBadge('pill-state', 'online', `READY (#${captureId})`);
      zoomFit();
      computeAllMeasurements();
      runDecoders();
      drawScope();

      // Check if continuous mode is active
      if (document.getElementById('sel-acq-mode').value === 'continuous') {
        setTimeout(cmdArm, 80);
      }
    }

    function handleJsonMessage(msg) {
      if (msg.type === 'term') {
        appendTerminal(msg.text);
        parseTelemetryFromText(msg.text);
      } else if (msg.type === 'capture_ready') {
        captureId = msg.capture_id || captureId;
      } else if (msg.type === 'trigger_fired' || msg.type === 'trigger_event') {
        updateBadge('pill-state', 'warn', 'TRIGGERED');
      } else if (msg.type === 'status_resp') {
        updateBadge('pill-state', 'active', 'STATE: ' + msg.state);
        if (msg.adc0 !== undefined) document.getElementById('val-adc0').innerText = (msg.adc0 / 1000.0).toFixed(2) + ' V';
        if (msg.adc1 !== undefined) document.getElementById('val-adc1').innerText = (msg.adc1 / 1000.0).toFixed(2) + ' V';
        if (msg.adc2 !== undefined) document.getElementById('val-adc2').innerText = (msg.adc2 / 1000.0).toFixed(2) + ' V';
      }
    }

    function updateBadge(id, cls, text) {
      const el = document.getElementById(id);
      if (!el) return;
      el.className = 'pill ' + cls;
      el.innerHTML = `<span class="dot"></span>${text}`;
    }

    // ─── Scope Commands ──────────────────────────────────────────────────────
    function cmdArm() {
      const rate  = parseInt(document.getElementById('sel-rate').value);
      const count = parseInt(document.getElementById('sel-count').value);
      const trigCh = parseInt(document.getElementById('sel-trig-ch').value);
      const trigMode = parseInt(document.getElementById('sel-trig-mode').value);
      const preTrig = parseInt(document.getElementById('sel-pre-trig').value);

      const pkt = {
        cmd: 'arm',
        rate: rate,
        count: count,
        trigger_channel: trigCh,
        trigger_mode: trigMode,
        pre_trigger_pct: preTrig
      };

      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(pkt));
      }
      isArmed = true;
      updateBadge('pill-state', 'warn', 'ARMED');
    }

    function cmdSingle() {
      document.getElementById('sel-acq-mode').value = 'single';
      cmdArm();
    }

    function cmdStop() {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ cmd: 'stop' }));
      }
      isArmed = false;
      updateBadge('pill-state', 'offline', 'STOPPED');
    }

    function cmdClear() {
      rawBuffer = null;
      cursorA = null;
      cursorB = null;
      decodedTokens = [];
      updateCursorReadouts();
      drawScope();
    }

    // ─── Canvas Interaction & Rendering ──────────────────────────────────────
    function initEvents() {
      const wrap = document.getElementById('canvas-wrap');

      wrap.addEventListener('mousedown', (e) => {
        isDragging = true;
        dragStartX = e.clientX;
        dragStartPan = panOffset;
      });

      window.addEventListener('mousemove', (e) => {
        if (!rawBuffer || rawBuffer.length === 0) return;
        const rect = canvas.getBoundingClientRect();
        const clientX = e.clientX - rect.left;
        const labelW = 75;
        const traceW = rect.width - labelW;

        if (clientX >= labelW && clientX <= rect.width) {
          const visibleSamples = Math.max(16, Math.floor(rawBuffer.length / zoomLevel));
          const sampleUnderMouse = Math.floor(panOffset + ((clientX - labelW) / traceW) * visibleSamples);
          hoverSample = Math.max(0, Math.min(rawBuffer.length - 1, sampleUnderMouse));
        } else {
          hoverSample = null;
        }

        if (isDragging) {
          const deltaX = e.clientX - dragStartX;
          const visibleSamples = Math.max(16, Math.floor(rawBuffer.length / zoomLevel));
          const samplesPerPixel = visibleSamples / traceW;
          panOffset = Math.max(0, Math.min(rawBuffer.length - visibleSamples, Math.floor(dragStartPan - deltaX * samplesPerPixel)));
          drawScope();
        } else if (hoverSample !== null) {
          drawScope();
        }
      });

      window.addEventListener('mouseup', () => {
        isDragging = false;
      });

      wrap.addEventListener('click', (e) => {
        if (!rawBuffer || rawBuffer.length === 0) return;
        const rect = canvas.getBoundingClientRect();
        const clickX = e.clientX - rect.left;
        const labelW = 75;
        const traceW = rect.width - labelW;
        if (clickX < labelW) return;

        const visibleSamples = Math.max(16, Math.floor(rawBuffer.length / zoomLevel));
        const sampleIdx = Math.floor(panOffset + ((clickX - labelW) / traceW) * visibleSamples);

        if (cursorA === null) {
          cursorA = sampleIdx;
        } else if (cursorB === null) {
          cursorB = sampleIdx;
        } else {
          cursorA = sampleIdx;
          cursorB = null;
        }
        updateCursorReadouts();
        drawScope();
      });

      wrap.addEventListener('wheel', (e) => {
        e.preventDefault();
        if (!rawBuffer || rawBuffer.length === 0) return;
        if (e.deltaY < 0) {
          zoomIn();
        } else {
          zoomOut();
        }
      }, { passive: false });
    }

    function zoomIn() {
      zoomLevel = Math.min(128.0, zoomLevel * 1.5);
      clampPan();
      drawScope();
    }

    function zoomOut() {
      zoomLevel = Math.max(1.0, zoomLevel / 1.5);
      clampPan();
      drawScope();
    }

    function zoomFit() {
      zoomLevel = 1.0;
      panOffset = 0;
      drawScope();
    }

    function clampPan() {
      if (!rawBuffer) return;
      const visibleSamples = Math.max(16, Math.floor(rawBuffer.length / zoomLevel));
      panOffset = Math.max(0, Math.min(rawBuffer.length - visibleSamples, panOffset));
    }

    function clearCursors() {
      cursorA = null;
      cursorB = null;
      updateCursorReadouts();
      drawScope();
    }

    function updateCursorReadouts() {
      const aEl = document.getElementById('lbl-cursor-a');
      const bEl = document.getElementById('lbl-cursor-b');
      const dtEl = document.getElementById('lbl-delta-t');
      const dsEl = document.getElementById('lbl-delta-s');
      const fEl = document.getElementById('lbl-cursor-freq');

      if (!rawBuffer || (cursorA === null && cursorB === null)) {
        aEl.innerText = '--';
        bEl.innerText = '--';
        dtEl.innerText = '--';
        dsEl.innerText = '--';
        fEl.innerText = '--';
        return;
      }

      const sampleTimeUs = (1.0 / sampleRate) * 1e6;

      if (cursorA !== null) {
        const tA = (cursorA - triggerOffset) * sampleTimeUs;
        aEl.innerText = `#${cursorA} (${formatTime(tA)})`;
      } else {
        aEl.innerText = '--';
      }

      if (cursorB !== null) {
        const tB = (cursorB - triggerOffset) * sampleTimeUs;
        bEl.innerText = `#${cursorB} (${formatTime(tB)})`;
      } else {
        bEl.innerText = '--';
      }

      if (cursorA !== null && cursorB !== null) {
        const deltaSamples = Math.abs(cursorB - cursorA);
        const deltaSec = deltaSamples / sampleRate;
        const deltaUs = deltaSec * 1e6;
        const freqHz = deltaSec > 0 ? (1.0 / deltaSec) : 0;

        dsEl.innerText = deltaSamples.toLocaleString();
        dtEl.innerText = formatTime(deltaUs);
        fEl.innerText = formatFreq(freqHz);
      } else {
        dtEl.innerText = '--';
        dsEl.innerText = '--';
        fEl.innerText = '--';
      }
    }

    function formatTime(us) {
      const absUs = Math.abs(us);
      const sign = us < 0 ? '-' : '';
      if (absUs >= 1000) return sign + (absUs / 1000.0).toFixed(3) + ' ms';
      if (absUs >= 1.0)  return sign + absUs.toFixed(3) + ' \u03bcs';
      return sign + (absUs * 1000.0).toFixed(1) + ' ns';
    }

    function formatFreq(hz) {
      if (hz >= 1e6) return (hz / 1e6).toFixed(3) + ' MHz';
      if (hz >= 1e3) return (hz / 1e3).toFixed(2) + ' kHz';
      return hz.toFixed(1) + ' Hz';
    }

    // ─── Waveform Rendering Engine ───────────────────────────────────────────
    function drawScope() {
      const wrap = document.getElementById('canvas-wrap');
      const width = wrap.clientWidth;
      const height = wrap.clientHeight;

      ctx.fillStyle = '#060b14';
      ctx.fillRect(0, 0, width, height);

      const labelW = 75;
      const traceW = width - labelW;
      const numActiveCh = 8;
      const laneH = height / numActiveCh;

      // Draw Top Time Ruler & Grid Lines
      drawTimeRuler(labelW, traceW, width);

      if (!rawBuffer || rawBuffer.length === 0) {
        ctx.fillStyle = '#475569';
        ctx.font = '13px monospace';
        ctx.textAlign = 'center';
        ctx.fillText('NO CAPTURE DATA — CLICK [ARM / RUN] TO SAMPLE REAL HARDWARE SIGNALS', width / 2, height / 2);
        return;
      }

      const visibleSamples = Math.max(16, Math.floor(rawBuffer.length / zoomLevel));
      const stepX = traceW / visibleSamples;

      // Draw 8 Channel Traces
      for (let ch = 0; ch < 8; ch++) {
        const topY = ch * laneH;
        const baseY = topY + laneH - 6;
        const waveH = laneH - 12;

        // Channel dividing line
        ctx.strokeStyle = '#162238';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(0, topY);
        ctx.lineTo(width, topY);
        ctx.stroke();

        // Left Label Tag
        ctx.fillStyle = chEnabled[ch] ? COLORS[ch] : '#475569';
        ctx.font = 'bold 11px monospace';
        ctx.textAlign = 'left';
        ctx.fillText(`CH${ch}`, 8, topY + (laneH / 2) + 4);
        ctx.fillStyle = '#64748b';
        ctx.font = '9px monospace';
        ctx.fillText(PIN_NAMES[ch], 38, topY + (laneH / 2) + 4);

        if (!chEnabled[ch]) continue;

        // Draw Digital Waveform
        ctx.strokeStyle = COLORS[ch];
        ctx.lineWidth = 1.5;
        ctx.beginPath();

        const startIdx = panOffset;
        const endIdx = Math.min(rawBuffer.length, panOffset + visibleSamples);

        let prevBit = (rawBuffer[startIdx] >> ch) & 1;
        if (chInverted[ch]) prevBit ^= 1;
        let prevY = baseY - (prevBit * waveH);

        ctx.moveTo(labelW, prevY);

        for (let i = startIdx + 1; i < endIdx; i++) {
          let bit = (rawBuffer[i] >> ch) & 1;
          if (chInverted[ch]) bit ^= 1;
          const x = labelW + ((i - panOffset) * stepX);
          const y = baseY - (bit * waveH);

          if (bit !== prevBit) {
            ctx.lineTo(x, prevY);
          }
          ctx.lineTo(x, y);
          prevBit = bit;
          prevY = y;
        }
        ctx.stroke();

        // Subtle gradient fill under HIGH pulses
        ctx.lineTo(labelW + ((endIdx - 1 - panOffset) * stepX), baseY);
        ctx.lineTo(labelW, baseY);
        ctx.closePath();
        ctx.fillStyle = COLORS[ch] + '12';
        ctx.fill();
      }

      // Draw Trigger Marker ($T_0$)
      if (triggerOffset >= panOffset && triggerOffset < panOffset + visibleSamples) {
        const trigX = labelW + ((triggerOffset - panOffset) * stepX);
        ctx.strokeStyle = '#ef4444';
        ctx.lineWidth = 1.5;
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(trigX, 0);
        ctx.lineTo(trigX, height);
        ctx.stroke();
        ctx.setLineDash([]);

        ctx.fillStyle = '#ef4444';
        ctx.font = 'bold 10px monospace';
        ctx.textAlign = 'center';
        ctx.fillText('T0', trigX, 12);
      }

      // Draw Timing Cursors A & B
      drawCursor(cursorA, '#eab308', 'Cursor A', labelW, stepX, visibleSamples, height);
      drawCursor(cursorB, '#38bdf8', 'Cursor B', labelW, stepX, visibleSamples, height);

      // Draw Decoded Protocol Bubbles Overlay
      drawDecodedOverlay(labelW, stepX, visibleSamples, laneH);
    }

    function drawTimeRuler(labelW, traceW, totalW) {
      ctx.strokeStyle = '#0f172a';
      ctx.lineWidth = 1;
      const numDivs = 10;
      const divW = traceW / numDivs;

      ctx.fillStyle = '#64748b';
      ctx.font = '9px monospace';
      ctx.textAlign = 'center';

      for (let i = 0; i <= numDivs; i++) {
        const x = labelW + (i * divW);
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, canvas.height);
        ctx.stroke();

        if (rawBuffer && rawBuffer.length > 0) {
          const visibleSamples = Math.max(16, Math.floor(rawBuffer.length / zoomLevel));
          const sampleAtLine = panOffset + Math.floor((i / numDivs) * visibleSamples);
          const timeUs = (sampleAtLine - triggerOffset) * ((1.0 / sampleRate) * 1e6);
          ctx.fillText(formatTime(timeUs), x, 11);
        }
      }
    }

    function drawCursor(idx, color, label, labelW, stepX, visibleSamples, height) {
      if (idx === null || idx < panOffset || idx >= panOffset + visibleSamples) return;
      const curX = labelW + ((idx - panOffset) * stepX);

      ctx.strokeStyle = color;
      ctx.lineWidth = 1.5;
      ctx.setLineDash([3, 3]);
      ctx.beginPath();
      ctx.moveTo(curX, 14);
      ctx.lineTo(curX, height);
      ctx.stroke();
      ctx.setLineDash([]);

      ctx.fillStyle = color;
      ctx.font = 'bold 10px monospace';
      ctx.textAlign = 'center';
      ctx.fillText(label, curX, 12);
    }

    function drawDecodedOverlay(labelW, stepX, visibleSamples, laneH) {
      if (!decodedTokens || decodedTokens.length === 0) return;
      ctx.font = 'bold 9px monospace';
      ctx.textAlign = 'center';

      for (const tok of decodedTokens) {
        if (tok.endSample < panOffset || tok.startSample > panOffset + visibleSamples) continue;
        const x1 = Math.max(labelW, labelW + ((tok.startSample - panOffset) * stepX));
        const x2 = Math.min(canvas.width, labelW + ((tok.endSample - panOffset) * stepX));
        const w = Math.max(16, x2 - x1);
        const y = (tok.channel * laneH) + (laneH - 10);

        ctx.fillStyle = 'rgba(56, 189, 248, 0.25)';
        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.roundRect ? ctx.roundRect(x1, y - 11, w, 12, 3) : ctx.rect(x1, y - 11, w, 12);
        ctx.fill();
        ctx.stroke();

        ctx.fillStyle = '#ffffff';
        ctx.fillText(tok.text, x1 + (w / 2), y - 2);
      }
    }

    // ─── Real-Time Hardware Measurements Engine ──────────────────────────────
    function computeAllMeasurements() {
      if (!rawBuffer || rawBuffer.length < 2) return;
      const tbody = document.getElementById('meas-table-body');
      tbody.innerHTML = '';

      for (let ch = 0; ch < 8; ch++) {
        const meas = measureChannel(ch);
        const row = document.createElement('tr');
        row.innerHTML = `
          <td><span style="color:${COLORS[ch]}; font-weight:bold;">CH${ch}</span> (${PIN_NAMES[ch]})</td>
          <td>${meas.freq > 0 ? formatFreq(meas.freq) : '--'}</td>
          <td>${meas.periodUs > 0 ? meas.periodUs.toFixed(3) : '--'}</td>
          <td>${meas.duty > 0 ? meas.duty.toFixed(1) + '%' : '--'}</td>
          <td>${meas.highUs > 0 ? meas.highUs.toFixed(3) : '--'}</td>
          <td>${meas.lowUs > 0 ? meas.lowUs.toFixed(3) : '--'}</td>
          <td>${meas.edges.toLocaleString()}</td>
          <td>${meas.edges > 0 ? '<span style="color:#10b981;">ACTIVE</span>' : '<span style="color:#64748b;">STATIC</span>'}</td>
        `;
        tbody.appendChild(row);
      }

      updateMeasurementsUI();
    }

    function measureChannel(ch) {
      let edges = 0;
      let firstEdge = -1;
      let lastEdge = -1;
      let totalHighSamples = 0;
      let prevBit = (rawBuffer[0] >> ch) & 1;

      for (let i = 1; i < rawBuffer.length; i++) {
        const bit = (rawBuffer[i] >> ch) & 1;
        if (bit === 1) totalHighSamples++;
        if (bit !== prevBit) {
          edges++;
          if (firstEdge === -1) firstEdge = i;
          lastEdge = i;
          prevBit = bit;
        }
      }

      const sampleTimeUs = (1.0 / sampleRate) * 1e6;
      let freq = 0, periodUs = 0, duty = 0, highUs = 0, lowUs = 0;

      if (edges >= 2 && lastEdge > firstEdge) {
        // Compute fundamental frequency based on transitions
        const cycles = edges / 2.0;
        const timeWindowSec = (lastEdge - firstEdge) / sampleRate;
        freq = cycles / timeWindowSec;
        periodUs = (1.0 / freq) * 1e6;
        duty = (totalHighSamples / rawBuffer.length) * 100.0;
        highUs = (periodUs * (duty / 100.0));
        lowUs = periodUs - highUs;
      }

      return { edges, freq, periodUs, duty, highUs, lowUs };
    }

    function updateMeasurementsUI() {
      const ch = parseInt(document.getElementById('sel-meas-ch').value);
      if (!rawBuffer) return;
      const m = measureChannel(ch);
      document.getElementById('qm-freq').innerText = m.freq > 0 ? formatFreq(m.freq) : '-- Hz';
      document.getElementById('qm-period-duty').innerText = (m.periodUs > 0 ? m.periodUs.toFixed(2) + ' \u03bcs' : '--') + ' / ' + (m.duty > 0 ? m.duty.toFixed(1) + '%' : '--%');
      document.getElementById('qm-edges').innerText = m.edges.toLocaleString() + ' edges';
    }

    // ─── Protocol Decoders Engine ────────────────────────────────────────────
    function onDecoderConfigChange() {
      const proto = document.getElementById('dec-proto').value;
      document.getElementById('dec-uart-cfg').style.display = proto === 'uart' ? 'contents' : 'none';
      document.getElementById('dec-spi-cfg').style.display  = proto === 'spi'  ? 'contents' : 'none';
      document.getElementById('dec-i2c-cfg').style.display  = proto === 'i2c'  ? 'contents' : 'none';
      runDecoders();
    }

    function runDecoders() {
      decodedTokens = [];
      const tbody = document.getElementById('dec-table-body');
      tbody.innerHTML = '';
      if (!rawBuffer || rawBuffer.length === 0) return;

      const proto = document.getElementById('dec-proto').value;
      if (proto === 'uart') {
        decodeUART();
      } else if (proto === 'spi') {
        decodeSPI();
      } else if (proto === 'i2c') {
        decodeI2C();
      }

      document.getElementById('dec-count').innerText = `${decodedTokens.length} tokens decoded`;

      if (decodedTokens.length === 0) {
        tbody.innerHTML = `<tr><td colspan="5" style="text-align:center; color:var(--text-muted);">No valid ${proto.toUpperCase()} packets detected in current capture buffer</td></tr>`;
      } else {
        decodedTokens.slice(0, 200).forEach((tok, idx) => {
          const row = document.createElement('tr');
          row.innerHTML = `
            <td>${idx + 1}</td>
            <td>${tok.timeUs.toFixed(2)}</td>
            <td style="color:#eab308; font-weight:bold;">${tok.hex}</td>
            <td style="color:#38bdf8;">${tok.ascii}</td>
            <td>${tok.details}</td>
          `;
          tbody.appendChild(row);
        });
      }
      drawScope();
    }

    function decodeUART() {
      const ch   = parseInt(document.getElementById('dec-uart-ch').value);
      const baud = parseInt(document.getElementById('dec-uart-baud').value);
      const samplesPerBit = sampleRate / baud;
      if (samplesPerBit < 2) return; // Nyquist limit

      let i = 0;
      while (i < rawBuffer.length - Math.floor(samplesPerBit * 10)) {
        // Look for start bit falling edge (1 -> 0)
        const bit = (rawBuffer[i] >> ch) & 1;
        if (bit === 0 && i > 0 && ((rawBuffer[i - 1] >> ch) & 1) === 1) {
          // Midpoint of start bit
          const midStart = i + Math.floor(samplesPerBit * 0.5);
          if (midStart >= rawBuffer.length || ((rawBuffer[midStart] >> ch) & 1) !== 0) {
            i++;
            continue; // False glitch
          }

          // Sample 8 data bits
          let byteVal = 0;
          for (let b = 0; b < 8; b++) {
            const sampleIdx = i + Math.floor(samplesPerBit * (1.5 + b));
            if (sampleIdx >= rawBuffer.length) break;
            const bitVal = (rawBuffer[sampleIdx] >> ch) & 1;
            byteVal |= (bitVal << b);
          }

          const endIdx = i + Math.floor(samplesPerBit * 9.5);
          const timeUs = (i - triggerOffset) * ((1.0 / sampleRate) * 1e6);
          const hexStr = '0x' + byteVal.toString(16).toUpperCase().padStart(2, '0');
          const asciiChar = (byteVal >= 32 && byteVal <= 126) ? `'${String.fromCharCode(byteVal)}'` : '.';

          decodedTokens.push({
            channel: ch,
            startSample: i,
            endSample: endIdx,
            timeUs: timeUs,
            hex: hexStr,
            ascii: asciiChar,
            text: hexStr,
            details: `Baud: ${baud}, Byte: ${byteVal}`
          });

          i = endIdx;
        } else {
          i++;
        }
      }
    }

    function decodeSPI() {
      // Decode SPI on configured channel mapping
      const parts = document.getElementById('dec-spi-map').value.split('_').map(Number);
      const sckCh = parts[0], mosiCh = parts[1], misoCh = parts[2], csCh = parts[3];

      let byteMOSI = 0, byteMISO = 0, bitCount = 0;
      let startSample = 0;
      let prevSCK = (rawBuffer[0] >> sckCh) & 1;

      for (let i = 1; i < rawBuffer.length; i++) {
        const cs = (rawBuffer[i] >> csCh) & 1;
        if (cs === 1) { // CS inactive (HIGH)
          bitCount = 0;
          prevSCK = (rawBuffer[i] >> sckCh) & 1;
          continue;
        }

        const sck = (rawBuffer[i] >> sckCh) & 1;
        if (sck === 1 && prevSCK === 0) { // Rising edge
          if (bitCount === 0) startSample = i;
          const mosi = (rawBuffer[i] >> mosiCh) & 1;
          const miso = (rawBuffer[i] >> misoCh) & 1;
          byteMOSI = (byteMOSI << 1) | mosi;
          byteMISO = (byteMISO << 1) | miso;
          bitCount++;

          if (bitCount === 8) {
            const timeUs = (startSample - triggerOffset) * ((1.0 / sampleRate) * 1e6);
            const hexMOSI = '0x' + byteMOSI.toString(16).toUpperCase().padStart(2, '0');
            const hexMISO = '0x' + byteMISO.toString(16).toUpperCase().padStart(2, '0');

            decodedTokens.push({
              channel: mosiCh,
              startSample: startSample,
              endSample: i,
              timeUs: timeUs,
              hex: `MOSI:${hexMOSI}`,
              ascii: `MISO:${hexMISO}`,
              text: hexMOSI,
              details: `MOSI: ${hexMOSI}, MISO: ${hexMISO}`
            });
            byteMOSI = 0;
            byteMISO = 0;
            bitCount = 0;
          }
        }
        prevSCK = sck;
      }
    }

    function decodeI2C() {
      const parts = document.getElementById('dec-i2c-map').value.split('_').map(Number);
      const sclCh = parts[0], sdaCh = parts[1];

      let inTransaction = false;
      let byteVal = 0, bitCount = 0, startSample = 0;

      for (let i = 1; i < rawBuffer.length; i++) {
        const sclPrev = (rawBuffer[i - 1] >> sclCh) & 1;
        const sclCurr = (rawBuffer[i] >> sclCh) & 1;
        const sdaPrev = (rawBuffer[i - 1] >> sdaCh) & 1;
        const sdaCurr = (rawBuffer[i] >> sdaCh) & 1;

        // START Condition: SDA falls while SCL is HIGH
        if (sclCurr === 1 && sdaPrev === 1 && sdaCurr === 0) {
          inTransaction = true;
          bitCount = 0;
          byteVal = 0;
          startSample = i;
          decodedTokens.push({
            channel: sdaCh,
            startSample: i,
            endSample: i + 10,
            timeUs: (i - triggerOffset) * ((1.0 / sampleRate) * 1e6),
            hex: 'START',
            ascii: 'S',
            text: 'START',
            details: 'I2C START Condition'
          });
          continue;
        }

        // STOP Condition: SDA rises while SCL is HIGH
        if (sclCurr === 1 && sdaPrev === 0 && sdaCurr === 1) {
          inTransaction = false;
          decodedTokens.push({
            channel: sdaCh,
            startSample: i,
            endSample: i + 10,
            timeUs: (i - triggerOffset) * ((1.0 / sampleRate) * 1e6),
            hex: 'STOP',
            ascii: 'P',
            text: 'STOP',
            details: 'I2C STOP Condition'
          });
          continue;
        }

        // Clock data bit on SCL rising edge
        if (inTransaction && sclPrev === 0 && sclCurr === 1) {
          if (bitCount < 8) {
            byteVal = (byteVal << 1) | sdaCurr;
            bitCount++;
          } else if (bitCount === 8) { // 9th bit: ACK/NACK
            const ack = sdaCurr === 0;
            const timeUs = (startSample - triggerOffset) * ((1.0 / sampleRate) * 1e6);
            const hexStr = '0x' + byteVal.toString(16).toUpperCase().padStart(2, '0');

            decodedTokens.push({
              channel: sdaCh,
              startSample: startSample,
              endSample: i,
              timeUs: timeUs,
              hex: hexStr,
              ascii: ack ? 'ACK' : 'NACK',
              text: hexStr,
              details: `Data: ${hexStr} (${ack ? 'ACK' : 'NACK'})`
            });

            byteVal = 0;
            bitCount = 0;
            startSample = i;
          }
        }
      }
    }

    // ─── Export Engine (VCD / CSV / JSON) ────────────────────────────────────
    function exportData(type) {
      if (!rawBuffer || rawBuffer.length === 0) {
        alert('No capture data available to export! Please ARM and capture first.');
        return;
      }

      let content = '', filename = '', mime = '';

      if (type === 'vcd') {
        filename = `udoo_logic_capture_${captureId}.vcd`;
        mime = 'text/plain';
        content = generateVCD();
      } else if (type === 'csv') {
        filename = `udoo_logic_capture_${captureId}.csv`;
        mime = 'text/csv';
        content = generateCSV();
      } else if (type === 'json') {
        filename = `udoo_logic_capture_${captureId}.json`;
        mime = 'application/json';
        content = generateJSON();
      }

      const blob = new Blob([content], { type: mime });
      const link = document.createElement('a');
      link.href = URL.createObjectURL(blob);
      link.download = filename;
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      URL.revokeObjectURL(link.href);
    }

    function generateVCD() {
      const dtNs = Math.round((1.0 / sampleRate) * 1e9);
      let vcd = `$date\n   ${new Date().toISOString()}\n$end\n`;
      vcd += `$version\n   UDOO KEY Logic Scope v1.0\n$end\n`;
      vcd += `$timescale 1ns $end\n`;
      vcd += `$scope module instrument $end\n`;
      for (let ch = 0; ch < 8; ch++) {
        vcd += `$var wire 1 ${String.fromCharCode(33 + ch)} CH${ch}_${PIN_NAMES[ch]} $end\n`;
      }
      vcd += `$upscope $end\n$enddefinitions $end\n`;
      vcd += `#0\n$dumpvars\n`;

      let prevVal = rawBuffer[0];
      for (let ch = 0; ch < 8; ch++) {
        vcd += `${(prevVal >> ch) & 1}${String.fromCharCode(33 + ch)}\n`;
      }
      vcd += `$end\n`;

      let curNs = 0;
      for (let i = 1; i < rawBuffer.length; i++) {
        const val = rawBuffer[i];
        curNs += dtNs;
        if (val !== prevVal) {
          vcd += `#${curNs}\n`;
          for (let ch = 0; ch < 8; ch++) {
            const bPrev = (prevVal >> ch) & 1;
            const bCurr = (val >> ch) & 1;
            if (bPrev !== bCurr) {
              vcd += `${bCurr}${String.fromCharCode(33 + ch)}\n`;
            }
          }
          prevVal = val;
        }
      }
      return vcd;
    }

    function generateCSV() {
      const sampleTimeUs = (1.0 / sampleRate) * 1e6;
      let csv = 'Sample,Time_us,CH0,CH1,CH2,CH3,CH4,CH5,CH6,CH7\n';
      for (let i = 0; i < rawBuffer.length; i++) {
        const b = rawBuffer[i];
        const t = (i - triggerOffset) * sampleTimeUs;
        csv += `${i},${t.toFixed(4)},${(b>>0)&1},${(b>>1)&1},${(b>>2)&1},${(b>>3)&1},${(b>>4)&1},${(b>>5)&1},${(b>>6)&1},${(b>>7)&1}\n`;
      }
      return csv;
    }

    function generateJSON() {
      return JSON.stringify({
        device: 'UDOO KEY Dual-MCU',
        sample_rate_hz: sampleRate,
        total_samples: rawBuffer.length,
        trigger_offset: triggerOffset,
        capture_id: captureId,
        channels: PIN_NAMES,
        samples_base64: btoa(String.fromCharCode.apply(null, rawBuffer))
      }, null, 2);
    }

    // ─── Bit Pirate Terminal & Quick Commands ────────────────────────────────
    function sendCmd(c) {
      appendTerminal('\n> ' + c + '\n');
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ cmd: 'term', text: c }));
      }
    }

    function sendTermInput() {
      const inp = document.getElementById('term-input');
      const val = inp.value.trim();
      if (!val) return;
      sendCmd(val);
      inp.value = '';
    }

    function appendTerminal(txt) {
      const term = document.getElementById('terminal-out');
      term.innerText += txt;
      term.scrollTop = term.scrollHeight;
    }

    function parseTelemetryFromText(txt) {
      const ldr = txt.match(/LDR\s*:\s*([\d\.]+)\s*V/i) || txt.match(/ADC0\s*:\s*([\d\.]+)\s*V/i);
      const a1  = txt.match(/A1\s*:\s*([\d\.]+)\s*V/i)  || txt.match(/ADC1\s*:\s*([\d\.]+)\s*V/i);
      const a2  = txt.match(/A2\s*:\s*([\d\.]+)\s*V/i)  || txt.match(/ADC2\s*:\s*([\d\.]+)\s*V/i);
      const sys = txt.match(/SYS\s*:\s*([\d\.]+)\s*V/i) || txt.match(/VSYS\s*:\s*([\d\.]+)\s*V/i);
      if (ldr) document.getElementById('val-adc0').innerText = ldr[1] + ' V';
      if (a1)  document.getElementById('val-adc1').innerText = a1[1] + ' V';
      if (a2)  document.getElementById('val-adc2').innerText = a2[1] + ' V';
      if (sys) document.getElementById('val-vsys').innerText = sys[1] + ' V';
    }

    // ─── PWM Generator & System Stats ────────────────────────────────────────
    function updatePWM() {
      const freq = parseInt(document.getElementById('pwm-freq').value);
      const duty = parseInt(document.getElementById('pwm-duty').value);
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ cmd: 'set_pwm', freq: freq, duty: duty }));
      }
    }

    function setPWMPreset(f, d) {
      document.getElementById('pwm-freq').value = f;
      document.getElementById('pwm-duty').value = d;
      updatePWM();
    }

    function pollSystemStats() {
      fetch('/api/system')
        .then(r => r.json())
        .then(d => {
          if (d.free_heap) document.getElementById('sys-heap').innerText = Math.round(d.free_heap / 1024) + ' KB';
          if (d.uptime_ms) document.getElementById('sys-uptime').innerText = Math.round(d.uptime_ms / 1000) + ' s';
        }).catch(() => {});
    }

    function switchTab(tabId) {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      event.currentTarget.classList.add('active');
      document.getElementById('tab-' + tabId).classList.add('active');
      if (tabId === 'scope') {
        setupCanvasResolution();
        drawScope();
      }
    }
  </script>
</body>
</html>
)rawliteral";

#endif // LOGIC_SCOPE_UI_H
