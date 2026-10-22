import serial
import serial.tools.list_ports
import struct
import threading
import time

PROTOCOL_MAGIC = 0x5A
CMD_GET_INFO = 0x01
CMD_SET_CONFIG = 0x02
CMD_START_CAPTURE = 0x03
CMD_STOP_CAPTURE = 0x04
CMD_STREAM_DATA = 0x05

class LogicAnalyzerDriver:
    def __init__(self, port=None, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.is_connected = False
        self.rx_thread = None
        self.running = False
        self.on_data_callback = None
        self.on_terminal_text_callback = None
        self.on_disconnect_callback = None

    @staticmethod
    def list_ports():
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self, port=None):
        if port:
            self.port = port
        if not self.port or self.port == "No Ports Found":
            return False
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            self.is_connected = True
            self.running = True
            self.rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
            self.rx_thread.start()
            return True
        except Exception as e:
            print(f"[Driver Error] Connection failed to {self.port}: {e}")
            self.is_connected = False
            return False

    def disconnect(self):
        was_connected = self.is_connected
        self.running = False
        self.is_connected = False
        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass
            self.ser = None
        if was_connected and self.on_disconnect_callback:
            try:
                self.on_disconnect_callback()
            except Exception:
                pass

    def send_command(self, cmd_str):
        """Sends an interactive Bus Pirate command string over serial"""
        if not self.is_connected or not self.ser:
            return False
        if not cmd_str.endswith('\n'):
            cmd_str += '\n'
        try:
            self.ser.write(cmd_str.encode('utf-8'))
            return True
        except (serial.SerialException, OSError) as e:
            print(f"[Driver Notice] Write failed (device disconnected?): {e}")
            self.disconnect()
            return False

    def set_config(self, sample_rate, sample_count, trigger_channel, trigger_mode):
        if not self.is_connected or not self.ser:
            return False
        payload = struct.pack('<IIBB', sample_rate, sample_count, trigger_channel, trigger_mode)
        hdr = struct.pack('<BBH', PROTOCOL_MAGIC, CMD_SET_CONFIG, len(payload))
        try:
            self.ser.write(hdr + payload)
            return True
        except (serial.SerialException, OSError) as e:
            print(f"[Driver Notice] Write failed: {e}")
            self.disconnect()
            return False

    def start_capture(self):
        if not self.is_connected or not self.ser:
            return False
        hdr = struct.pack('<BBH', PROTOCOL_MAGIC, CMD_START_CAPTURE, 0)
        try:
            self.ser.write(hdr)
            return True
        except (serial.SerialException, OSError) as e:
            print(f"[Driver Notice] Write failed: {e}")
            self.disconnect()
            return False

    def stop_capture(self):
        if not self.is_connected or not self.ser:
            return False
        hdr = struct.pack('<BBH', PROTOCOL_MAGIC, CMD_STOP_CAPTURE, 0)
        try:
            self.ser.write(hdr)
            return True
        except (serial.SerialException, OSError) as e:
            print(f"[Driver Notice] Write failed: {e}")
            self.disconnect()
            return False

    def _read_exact(self, count, timeout_sec=2.0):
        buf = bytearray()
        start_t = time.time()
        while len(buf) < count and (time.time() - start_t < timeout_sec):
            if not self.ser or not self.ser.is_open:
                return None
            chunk = self.ser.read(min(count - len(buf), 4096))
            if chunk:
                buf.extend(chunk)
            else:
                time.sleep(0.002)
        return bytes(buf) if len(buf) == count else None

    def _rx_loop(self):
        ascii_buf = bytearray()
        while self.running and self.ser:
            try:
                if not self.ser.is_open:
                    break
                raw_byte = self.ser.read(1)
                if not raw_byte:
                    continue

                # Check if it's the start of a binary Logic Capture packet
                if raw_byte[0] == PROTOCOL_MAGIC:
                    # Lookahead: check if next byte is a valid command (1..5)
                    cmd_byte = self.ser.read(1)
                    if not cmd_byte:
                        ascii_buf.append(raw_byte[0])
                        continue
                    cmd = cmd_byte[0]

                    if cmd not in (CMD_GET_INFO, CMD_SET_CONFIG, CMD_START_CAPTURE, CMD_STOP_CAPTURE, CMD_STREAM_DATA):
                        # Not a binary packet! It's ASCII text that happened to have 'Z'
                        ascii_buf.append(raw_byte[0])
                        ascii_buf.append(cmd_byte[0])
                        if b'\n' in ascii_buf or len(ascii_buf) >= 128:
                            text = ascii_buf.decode('utf-8', errors='replace')
                            ascii_buf.clear()
                            if self.on_terminal_text_callback:
                                self.on_terminal_text_callback(text)
                        continue

                    if cmd == CMD_STREAM_DATA:
                        length_bytes = self._read_exact(2, timeout_sec=0.5)
                        if not length_bytes:
                            continue
                        header_meta = self._read_exact(8, timeout_sec=1.0)
                        if not header_meta:
                            continue
                        buf_len, sample_rate = struct.unpack('<II', header_meta)

                        if buf_len > 131072 or buf_len == 0:
                            continue

                        data = self._read_exact(buf_len, timeout_sec=5.0)
                        if data and self.on_data_callback:
                            self.on_data_callback(data, sample_rate)
                    else:
                        # Other binary command responses (e.g. GET_INFO)
                        length_bytes = self._read_exact(2, timeout_sec=0.5)
                else:
                    # ASCII text from Bus Pirate CLI
                    ascii_buf.append(raw_byte[0])
                    if raw_byte[0] == ord('\n') or len(ascii_buf) >= 128:
                        text = ascii_buf.decode('utf-8', errors='replace')
                        ascii_buf.clear()
                        if self.on_terminal_text_callback:
                            self.on_terminal_text_callback(text)

            except (serial.SerialException, OSError) as e:
                print(f"[Driver Notice] Serial port disconnected: {e}")
                self.disconnect()
                break
            except Exception as e:
                time.sleep(0.05)
