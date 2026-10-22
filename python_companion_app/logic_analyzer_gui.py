import sys
import csv
import numpy as np
import pyqtgraph as pg
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QComboBox, QPushButton, QLabel, QSpinBox, QTableWidget, QTableWidgetItem,
    QFileDialog, QHeaderView, QGroupBox, QSplitter, QTextEdit, QLineEdit, QTabWidget,
    QStackedWidget
)
from PyQt6.QtCore import Qt, pyqtSignal, QObject
from serial_driver import LogicAnalyzerDriver
from protocol_decoders import ProtocolDecoders

CHANNEL_COLORS = [
    '#00FF66', '#00FFFF', '#FFFF00', '#FF9900',
    '#FF00FF', '#9900FF', '#0099FF', '#FFFFFF'
]

class SignalBridge(QObject):
    data_received = pyqtSignal(bytes, int)
    text_received = pyqtSignal(str)
    disconnected = pyqtSignal()

class LogicAnalyzerGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("UDOO KEY RP2040 Logic Analyzer & Bus Pirate Console")
        self.resize(1350, 900)

        self.driver = LogicAnalyzerDriver()
        self.bridge = SignalBridge()

        self.bridge.data_received.connect(self.on_data_received)
        self.bridge.text_received.connect(self.on_text_received)
        self.bridge.disconnected.connect(self.on_disconnected)

        self.driver.on_data_callback = lambda data, sr: self.bridge.data_received.emit(data, sr)
        self.driver.on_terminal_text_callback = lambda txt: self.bridge.text_received.emit(txt)
        self.driver.on_disconnect_callback = lambda: self.bridge.disconnected.emit()

        self.current_buffer = None
        self.current_sample_rate = 10000000

        self.init_ui()

    def init_ui(self):
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QVBoxLayout(main_widget)
        main_layout.setContentsMargins(8, 8, 8, 8)
        main_layout.setSpacing(6)

        # ─── Top Control Toolbar ────────────────────────────────────────────────
        toolbar_group = QGroupBox("Device Connection & Quick Actions")
        toolbar_layout = QHBoxLayout(toolbar_group)
        toolbar_layout.setContentsMargins(8, 6, 8, 6)

        # Port Selector
        toolbar_layout.addWidget(QLabel("Port:"))
        self.port_combo = QComboBox()
        self.refresh_ports()
        toolbar_layout.addWidget(self.port_combo)

        btn_refresh = QPushButton("Refresh")
        btn_refresh.clicked.connect(self.refresh_ports)
        toolbar_layout.addWidget(btn_refresh)

        self.btn_connect = QPushButton("Connect")
        self.btn_connect.setStyleSheet("background-color: #065f46; color: #34d399; font-weight: bold;")
        self.btn_connect.clicked.connect(self.toggle_connection)
        toolbar_layout.addWidget(self.btn_connect)

        toolbar_layout.addSpacing(10)

        # Quick Shell Buttons
        btn_scan = QPushButton("I2C Scan")
        btn_scan.clicked.connect(lambda: self.send_bp_cmd("scan"))
        toolbar_layout.addWidget(btn_scan)

        btn_flash = QPushButton("SPI Flash")
        btn_flash.clicked.connect(lambda: self.send_bp_cmd("flash probe"))
        toolbar_layout.addWidget(btn_flash)

        btn_swd = QPushButton("SWD")
        btn_swd.clicked.connect(lambda: self.send_bp_cmd("swd"))
        toolbar_layout.addWidget(btn_swd)

        btn_volt = QPushButton("Voltages")
        btn_volt.clicked.connect(lambda: self.send_bp_cmd("v"))
        toolbar_layout.addWidget(btn_volt)

        # Built-in Hardware Validation & Test Suite
        btn_self = QPushButton("Self Test")
        btn_self.setStyleSheet("background-color: #0f766e; color: #5eead4; font-weight: bold;")
        btn_self.clicked.connect(lambda: self.send_bp_cmd("test self"))
        toolbar_layout.addWidget(btn_self)

        btn_freq = QPushButton("Freq Sweep")
        btn_freq.setStyleSheet("background-color: #0f766e; color: #5eead4; font-weight: bold;")
        btn_freq.clicked.connect(lambda: self.send_bp_cmd("test freq"))
        toolbar_layout.addWidget(btn_freq)

        btn_100m = QPushButton("100 MSPS")
        btn_100m.setStyleSheet("background-color: #0f766e; color: #5eead4; font-weight: bold;")
        btn_100m.clicked.connect(lambda: self.send_bp_cmd("test 100msps"))
        toolbar_layout.addWidget(btn_100m)

        btn_disp = QPushButton("Displays")
        btn_disp.setStyleSheet("background-color: #7c2d12; color: #fdba74; font-weight: bold;")
        btn_disp.clicked.connect(lambda: self.send_bp_cmd("disp test"))
        toolbar_layout.addWidget(btn_disp)

        toolbar_layout.addSpacing(10)

        # Capture Settings
        toolbar_layout.addWidget(QLabel("Rate:"))
        self.rate_combo = QComboBox()
        self.rate_combo.addItems(["100 kHz", "1 MHz", "5 MHz", "10 MHz", "25 MHz", "50 MHz", "100 MHz"])
        self.rate_combo.setCurrentIndex(3)
        toolbar_layout.addWidget(self.rate_combo)

        toolbar_layout.addWidget(QLabel("Samples:"))
        self.samples_spin = QSpinBox()
        self.samples_spin.setRange(256, 65536)
        self.samples_spin.setValue(8192)
        self.samples_spin.setSingleStep(1024)
        toolbar_layout.addWidget(self.samples_spin)

        self.btn_run = QPushButton("START SCOPE")
        self.btn_run.setStyleSheet("background-color: #2563eb; color: white; font-weight: bold;")
        self.btn_run.clicked.connect(self.start_capture)
        toolbar_layout.addWidget(self.btn_run)

        self.btn_stop = QPushButton("STOP")
        self.btn_stop.setStyleSheet("background-color: #dc2626; color: white; font-weight: bold;")
        self.btn_stop.clicked.connect(lambda: self.send_bp_cmd("stop"))
        toolbar_layout.addWidget(self.btn_stop)

        btn_export_vcd = QPushButton("Export VCD")
        btn_export_vcd.clicked.connect(self.export_vcd)
        toolbar_layout.addWidget(btn_export_vcd)

        btn_export_csv = QPushButton("Export CSV")
        btn_export_csv.clicked.connect(self.export_csv)
        toolbar_layout.addWidget(btn_export_csv)

        main_layout.addWidget(toolbar_group)

        # ─── Central Splitter: Scope Waveforms (Top) & Tabs (Bottom) ────────────
        splitter = QSplitter(Qt.Orientation.Vertical)

        # Top Waveform Area with Timing Cursors Banner
        waveform_container = QWidget()
        waveform_layout = QVBoxLayout(waveform_container)
        waveform_layout.setContentsMargins(0, 0, 0, 0)
        waveform_layout.setSpacing(2)

        # Dual Cursor Timing Readout Bar
        cursor_bar = QWidget()
        cursor_layout = QHBoxLayout(cursor_bar)
        cursor_layout.setContentsMargins(4, 2, 4, 2)
        cursor_bar.setStyleSheet("background-color: #0b1120; border: 1px solid #1e293b; border-radius: 4px;")

        self.lbl_cursor_a = QLabel("Cursor A: 0.000 µs")
        self.lbl_cursor_a.setStyleSheet("color: #10b981; font-weight: bold; font-family: monospace;")
        cursor_layout.addWidget(self.lbl_cursor_a)

        cursor_layout.addWidget(QLabel("│"))

        self.lbl_cursor_b = QLabel("Cursor B: 0.000 µs")
        self.lbl_cursor_b.setStyleSheet("color: #06b6d4; font-weight: bold; font-family: monospace;")
        cursor_layout.addWidget(self.lbl_cursor_b)

        cursor_layout.addWidget(QLabel("│"))

        self.lbl_cursor_delta = QLabel("Δt: 0.000 µs (Freq: ---)")
        self.lbl_cursor_delta.setStyleSheet("color: #facc15; font-weight: bold; font-family: monospace;")
        cursor_layout.addWidget(self.lbl_cursor_delta)

        cursor_layout.addStretch()
        btn_reset_cursors = QPushButton("Reset Cursors")
        btn_reset_cursors.setStyleSheet("padding: 2px 8px; font-size: 10px;")
        btn_reset_cursors.clicked.connect(self.reset_cursors)
        cursor_layout.addWidget(btn_reset_cursors)

        waveform_layout.addWidget(cursor_bar)

        # Waveform Plot Widget (PyQtGraph) - antialias=False with peak downsampling
        pg.setConfigOptions(antialias=False)
        self.plot_widget = pg.PlotWidget(title="Logic Scope Channels (Ch0 - Ch7)")
        self.plot_widget.setBackground('#0f172a')
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)
        self.plot_widget.setLabel('bottom', "Time", units='s')
        self.plot_widget.setLabel('left', "Channels")
        plot_item = self.plot_widget.getPlotItem()
        plot_item.setDownsampling(auto=True, mode='peak')
        plot_item.setClipToView(True)

        # Interactive Cursors
        self.cursor_line_a = pg.InfiniteLine(pos=0.0, angle=90, movable=True, pen=pg.mkPen('#10b981', width=1.5, style=Qt.PenStyle.DashLine), label='A', labelOpts={'position': 0.95, 'color': '#10b981'})
        self.cursor_line_b = pg.InfiniteLine(pos=0.0, angle=90, movable=True, pen=pg.mkPen('#06b6d4', width=1.5, style=Qt.PenStyle.DashLine), label='B', labelOpts={'position': 0.95, 'color': '#06b6d4'})
        self.cursor_line_a.sigPositionChanged.connect(self.update_cursor_readout)
        self.cursor_line_b.sigPositionChanged.connect(self.update_cursor_readout)
        self.plot_widget.addItem(self.cursor_line_a)
        self.plot_widget.addItem(self.cursor_line_b)

        waveform_layout.addWidget(self.plot_widget)
        splitter.addWidget(waveform_container)

        # Bottom Tabbed Area: Protocol Decoders + Interactive Bus Pirate Console
        bottom_tabs = QTabWidget()
        bottom_tabs.setStyleSheet("QTabBar::tab { background: #1e293b; color: #94a3b8; padding: 6px 12px; } QTabBar::tab:selected { background: #2563eb; color: white; }")

        # Tab 1: Protocol Decoders
        decoder_tab = QWidget()
        dec_layout = QVBoxLayout(decoder_tab)
        dec_layout.setContentsMargins(6, 6, 6, 6)

        # Decoder Options Bar
        dec_opts_bar = QHBoxLayout()
        dec_opts_bar.addWidget(QLabel("Protocol:"))
        self.dec_proto_combo = QComboBox()
        self.dec_proto_combo.addItems(["UART", "I2C", "SPI"])
        self.dec_proto_combo.currentIndexChanged.connect(self.on_proto_selection_changed)
        dec_opts_bar.addWidget(self.dec_proto_combo)

        # Stacked Widget for Protocol Specific Settings
        self.dec_stack = QStackedWidget()

        # Page 0: UART Settings
        uart_widget = QWidget()
        uart_layout = QHBoxLayout(uart_widget)
        uart_layout.setContentsMargins(0, 0, 0, 0)
        uart_layout.addWidget(QLabel("RX Ch:"))
        self.uart_ch_combo = QComboBox()
        self.uart_ch_combo.addItems([f"Ch{i}" for i in range(8)])
        self.uart_ch_combo.setCurrentIndex(0)
        uart_layout.addWidget(self.uart_ch_combo)
        uart_layout.addWidget(QLabel("Baud:"))
        self.uart_baud_combo = QComboBox()
        self.uart_baud_combo.addItems(["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600", "1000000", "2000000"])
        self.uart_baud_combo.setCurrentText("115200")
        uart_layout.addWidget(self.uart_baud_combo)
        self.dec_stack.addWidget(uart_widget)

        # Page 1: I2C Settings
        i2c_widget = QWidget()
        i2c_layout = QHBoxLayout(i2c_widget)
        i2c_layout.setContentsMargins(0, 0, 0, 0)
        i2c_layout.addWidget(QLabel("SCL Ch:"))
        self.i2c_scl_combo = QComboBox()
        self.i2c_scl_combo.addItems([f"Ch{i}" for i in range(8)])
        self.i2c_scl_combo.setCurrentIndex(0)
        i2c_layout.addWidget(self.i2c_scl_combo)
        i2c_layout.addWidget(QLabel("SDA Ch:"))
        self.i2c_sda_combo = QComboBox()
        self.i2c_sda_combo.addItems([f"Ch{i}" for i in range(8)])
        self.i2c_sda_combo.setCurrentIndex(1)
        i2c_layout.addWidget(self.i2c_sda_combo)
        self.dec_stack.addWidget(i2c_widget)

        # Page 2: SPI Settings
        spi_widget = QWidget()
        spi_layout = QHBoxLayout(spi_widget)
        spi_layout.setContentsMargins(0, 0, 0, 0)
        spi_layout.addWidget(QLabel("SCK:"))
        self.spi_sck_combo = QComboBox()
        self.spi_sck_combo.addItems([f"Ch{i}" for i in range(8)])
        self.spi_sck_combo.setCurrentIndex(2)
        spi_layout.addWidget(self.spi_sck_combo)
        spi_layout.addWidget(QLabel("MOSI:"))
        self.spi_mosi_combo = QComboBox()
        self.spi_mosi_combo.addItems([f"Ch{i}" for i in range(8)])
        self.spi_mosi_combo.setCurrentIndex(3)
        spi_layout.addWidget(self.spi_mosi_combo)
        spi_layout.addWidget(QLabel("MISO:"))
        self.spi_miso_combo = QComboBox()
        self.spi_miso_combo.addItems([f"Ch{i}" for i in range(8)])
        self.spi_miso_combo.setCurrentIndex(4)
        spi_layout.addWidget(self.spi_miso_combo)
        spi_layout.addWidget(QLabel("CS:"))
        self.spi_cs_combo = QComboBox()
        self.spi_cs_combo.addItems(["None"] + [f"Ch{i}" for i in range(8)])
        self.spi_cs_combo.setCurrentIndex(6) # Ch5
        spi_layout.addWidget(self.spi_cs_combo)
        spi_layout.addWidget(QLabel("Mode:"))
        self.spi_mode_combo = QComboBox()
        self.spi_mode_combo.addItems(["Mode 0 (0,0)", "Mode 1 (0,1)", "Mode 2 (1,0)", "Mode 3 (1,1)"])
        self.spi_mode_combo.setCurrentIndex(0)
        spi_layout.addWidget(self.spi_mode_combo)
        self.dec_stack.addWidget(spi_widget)

        dec_opts_bar.addWidget(self.dec_stack)
        dec_opts_bar.addSpacing(10)

        btn_run_decoder = QPushButton("Decode Current Buffer")
        btn_run_decoder.setStyleSheet("background-color: #4338ca; color: white; font-weight: bold; padding: 4px 10px;")
        btn_run_decoder.clicked.connect(self.run_active_decoder)
        dec_opts_bar.addWidget(btn_run_decoder)
        dec_opts_bar.addStretch()

        dec_layout.addLayout(dec_opts_bar)

        self.decoder_table = QTableWidget(0, 4)
        self.decoder_table.setHorizontalHeaderLabels(["Timestamp (s)", "Channel / Protocol", "Hex Data", "Decoded ASCII"])
        self.decoder_table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.decoder_table.setStyleSheet("background-color: #1e293b; color: #f8fafc;")
        dec_layout.addWidget(self.decoder_table)
        bottom_tabs.addTab(decoder_tab, "Protocol Decoders (UART / I2C / SPI)")

        # Tab 2: Bus Pirate Interactive Terminal Console
        terminal_tab = QWidget()
        term_layout = QVBoxLayout(terminal_tab)

        self.term_output = QTextEdit()
        self.term_output.setReadOnly(True)
        self.term_output.document().setMaximumBlockCount(3000)
        self.term_output.setStyleSheet("background-color: #020617; color: #38bdf8; font-family: Consolas, monospace; font-size: 10pt;")
        term_layout.addWidget(self.term_output)

        input_layout = QHBoxLayout()
        self.term_input = QLineEdit()
        self.term_input.setPlaceholderText("Enter Bus Pirate syntax (e.g., [ 0x9F r:3 ], scan, flash probe, swd, help)...")
        self.term_input.setStyleSheet("background-color: #1e293b; color: white; padding: 6px;")
        self.term_input.returnPressed.connect(self.on_term_enter)
        input_layout.addWidget(self.term_input)

        btn_send = QPushButton("Send")
        btn_send.clicked.connect(self.on_term_enter)
        input_layout.addWidget(btn_send)

        term_layout.addLayout(input_layout)
        bottom_tabs.addTab(terminal_tab, "Bus Pirate Interactive CLI")

        splitter.addWidget(bottom_tabs)
        splitter.setSizes([500, 260])

        main_layout.addWidget(splitter)

    def on_proto_selection_changed(self, idx):
        self.dec_stack.setCurrentIndex(idx)

    def refresh_ports(self):
        self.port_combo.clear()
        ports = LogicAnalyzerDriver.list_ports()
        self.port_combo.addItems(ports if ports else ["No Ports Found"])

    def toggle_connection(self):
        if not self.driver.is_connected:
            port = self.port_combo.currentText()
            if self.driver.connect(port):
                self.btn_connect.setText("Disconnect")
                self.btn_connect.setStyleSheet("background-color: #991b1b; color: #fca5a5; font-weight: bold;")
                self.term_output.append("[Connected to UDOO Key on " + port + "]\n")
            else:
                self.term_output.append(f"[Failed to open {port}. Ensure board is plugged in.]\n")
        else:
            self.driver.disconnect()

    def on_disconnected(self):
        self.btn_connect.setText("Connect")
        self.btn_connect.setStyleSheet("background-color: #065f46; color: #34d399; font-weight: bold;")
        self.term_output.append("[Device Disconnected / Port Closed]\n")
        self.refresh_ports()

    def send_bp_cmd(self, cmd_str):
        self.term_output.append(f"> {cmd_str}")
        if not self.driver.is_connected:
            self.term_output.append("[Error: Not connected. Select a port and click Connect.]\n")
            return
        if not self.driver.send_command(cmd_str):
            self.term_output.append("[Error: Failed to write command. Device disconnected or port reset.]\n")
            self.on_disconnected()

    def on_term_enter(self):
        cmd = self.term_input.text().strip()
        if not cmd:
            return
        self.send_bp_cmd(cmd)
        self.term_input.clear()

    def on_text_received(self, text):
        self.term_output.insertPlainText(text)
        self.term_output.ensureCursorVisible()

    def start_capture(self):
        if not self.driver.is_connected:
            self.term_output.append("[Error: Not connected. Select a port and click Connect.]\n")
            return

        rates = [100000, 1000000, 5000000, 10000000, 25000000, 50000000, 100000000]
        sr = rates[self.rate_combo.currentIndex()]
        sc = self.samples_spin.value()

        # Send both binary config and CLI commands for rock-solid reliability
        self.driver.set_config(sr, sc, 0, 0)
        self.driver.start_capture()
        self.send_bp_cmd(f"rate {sr}")
        self.send_bp_cmd("start")

    def on_data_received(self, data_bytes, sample_rate):
        self.current_buffer = data_bytes
        self.current_sample_rate = sample_rate

        samples = np.frombuffer(data_bytes, dtype=np.uint8)
        num_samples = len(samples)
        time_axis = np.linspace(0, num_samples / sample_rate, num_samples)

        plot_item = self.plot_widget.getPlotItem()
        plot_item.disableAutoRange()
        self.plot_widget.clear()

        # Re-add cursor lines to plot
        self.plot_widget.addItem(self.cursor_line_a)
        self.plot_widget.addItem(self.cursor_line_b)

        # Plot 8 Channels stacked vertically with fast downsampled rendering
        for ch in range(8):
            bit_trace = (samples >> ch) & 0x01
            offset_y = (7 - ch) * 1.5
            trace_y = bit_trace * 0.8 + offset_y

            pen = pg.mkPen(color=CHANNEL_COLORS[ch], width=1.5)
            self.plot_widget.plot(time_axis, trace_y, pen=pen, name=f"Ch{ch}")

        # Position cursors nicely at 20% and 80% span
        t_max = num_samples / sample_rate
        self.cursor_line_a.setPos(t_max * 0.2)
        self.cursor_line_b.setPos(t_max * 0.8)
        self.update_cursor_readout()

        plot_item.autoRange()

        # Run decoder automatically
        self.run_active_decoder()

    def reset_cursors(self):
        if self.current_buffer is not None and self.current_sample_rate > 0:
            num_samples = len(self.current_buffer)
            t_max = num_samples / self.current_sample_rate
            self.cursor_line_a.setPos(t_max * 0.25)
            self.cursor_line_b.setPos(t_max * 0.75)
            self.update_cursor_readout()

    def update_cursor_readout(self):
        pos_a = self.cursor_line_a.value()
        pos_b = self.cursor_line_b.value()
        delta_t = abs(pos_b - pos_a)

        def format_time(t):
            if abs(t) < 1e-6:
                return f"{t*1e9:.2f} ns"
            elif abs(t) < 1e-3:
                return f"{t*1e6:.2f} µs"
            elif abs(t) < 1.0:
                return f"{t*1e3:.2f} ms"
            else:
                return f"{t:.4f} s"

        self.lbl_cursor_a.setText(f"Cursor A: {format_time(pos_a)}")
        self.lbl_cursor_b.setText(f"Cursor B: {format_time(pos_b)}")

        if delta_t > 0:
            freq = 1.0 / delta_t
            if freq >= 1e6:
                freq_str = f"{freq/1e6:.3f} MHz"
            elif freq >= 1e3:
                freq_str = f"{freq/1e3:.2f} kHz"
            else:
                freq_str = f"{freq:.1f} Hz"
            self.lbl_cursor_delta.setText(f"Δt: {format_time(delta_t)} (Freq: {freq_str})")
        else:
            self.lbl_cursor_delta.setText("Δt: 0.000 µs (Freq: ---)")

    def run_active_decoder(self):
        if self.current_buffer is None:
            return

        proto_idx = self.dec_proto_combo.currentIndex()
        events = []

        try:
            if proto_idx == 0:  # UART
                rx_ch = self.uart_ch_combo.currentIndex()
                baud = int(self.uart_baud_combo.currentText())
                events = ProtocolDecoders.decode_uart(self.current_buffer, self.current_sample_rate, rx_channel=rx_ch, baud_rate=baud)
            elif proto_idx == 1:  # I2C
                scl_ch = self.i2c_scl_combo.currentIndex()
                sda_ch = self.i2c_sda_combo.currentIndex()
                events = ProtocolDecoders.decode_i2c(self.current_buffer, self.current_sample_rate, scl_ch=scl_ch, sda_ch=sda_ch)
            elif proto_idx == 2:  # SPI
                sck_ch = self.spi_sck_combo.currentIndex()
                mosi_ch = self.spi_mosi_combo.currentIndex()
                miso_ch = self.spi_miso_combo.currentIndex()
                cs_idx = self.spi_cs_combo.currentIndex()
                cs_ch = (cs_idx - 1) if cs_idx > 0 else None
                mode_str = self.spi_mode_combo.currentIndex()
                cpol = 1 if mode_str in [2, 3] else 0
                cpha = 1 if mode_str in [1, 3] else 0
                events = ProtocolDecoders.decode_spi(
                    self.current_buffer, self.current_sample_rate,
                    sck_ch=sck_ch, mosi_ch=mosi_ch, miso_ch=miso_ch, cs_ch=cs_ch,
                    cpol=cpol, cpha=cpha
                )

            max_rows = min(len(events), 500)
            self.decoder_table.setUpdatesEnabled(False)
            self.decoder_table.setRowCount(max_rows)
            for row in range(max_rows):
                evt = events[row]
                self.decoder_table.setItem(row, 0, QTableWidgetItem(f"{evt['timestamp']:.6f}"))
                self.decoder_table.setItem(row, 1, QTableWidgetItem(evt['protocol']))
                self.decoder_table.setItem(row, 2, QTableWidgetItem(evt['hex']))
                self.decoder_table.setItem(row, 3, QTableWidgetItem(evt['ascii']))
            self.decoder_table.setUpdatesEnabled(True)
        except Exception as e:
            print(f"[Decoder Notice] {e}")

    def export_vcd(self):
        if self.current_buffer is None:
            return

        file_path, _ = QFileDialog.getSaveFileName(self, "Export Value Change Dump (VCD)", "", "VCD Files (*.vcd)")
        if not file_path:
            return

        samples = np.frombuffer(self.current_buffer, dtype=np.uint8)
        time_scale = int(1e9 / self.current_sample_rate)

        with open(file_path, 'w') as f:
            f.write("$date Today $end\n$version UDOO Key Scope $end\n")
            f.write(f"$timescale {time_scale}ns $end\n$scope module top $end\n")
            for ch in range(8):
                f.write(f"$var wire 1 ch{ch} Ch{ch} $end\n")
            f.write("$upscope $end\n$enddefinitions $end\n#0\n")

            prev_sample = None
            for i, s in enumerate(samples):
                if s != prev_sample:
                    f.write(f"#{i}\n")
                    for ch in range(8):
                        bit = (s >> ch) & 0x01
                        f.write(f"{bit}ch{ch}\n")
                    prev_sample = s

    def export_csv(self):
        if self.current_buffer is None:
            return

        file_path, _ = QFileDialog.getSaveFileName(self, "Export Tabular Scope Data (CSV)", "", "CSV Files (*.csv)")
        if not file_path:
            return

        samples = np.frombuffer(self.current_buffer, dtype=np.uint8)
        dt = 1.0 / self.current_sample_rate

        with open(file_path, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(["Timestamp_s", "CH0", "CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7", "Hex_Byte"])
            for i, s in enumerate(samples):
                t = i * dt
                bits = [(s >> ch) & 0x01 for ch in range(8)]
                writer.writerow([f"{t:.8f}"] + bits + [f"0x{s:02X}"])

def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    gui = LogicAnalyzerGUI()
    gui.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
