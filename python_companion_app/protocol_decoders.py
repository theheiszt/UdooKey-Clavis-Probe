import numpy as np

class ProtocolDecoders:
    @staticmethod
    def decode_uart(data_bytes, sample_rate, rx_channel=0, baud_rate=115200):
        """
        Decodes UART async serial data (8N1) on rx_channel.
        Returns list of decoded dicts: [{'timestamp': float, 'protocol': 'UART', 'hex': str, 'ascii': str}]
        """
        if not data_bytes or sample_rate <= 0 or baud_rate <= 0:
            return []

        samples = np.frombuffer(data_bytes, dtype=np.uint8)
        bit_samples = (samples >> rx_channel) & 0x01
        
        samples_per_bit = int(sample_rate / baud_rate)
        if samples_per_bit < 1:
            return []

        decoded = []
        n = len(bit_samples)
        i = 0

        while i < n - (10 * samples_per_bit):
            # Look for falling edge (START bit)
            if bit_samples[i] == 1 and bit_samples[i+1] == 0:
                start_sample = i + 1 + (samples_per_bit // 2)
                byte_val = 0
                valid = True
                
                # Sample 8 data bits (LSB first)
                for b in range(8):
                    sample_pos = start_sample + ((b + 1) * samples_per_bit)
                    if sample_pos >= n:
                        valid = False
                        break
                    bit = bit_samples[sample_pos]
                    byte_val |= (bit << b)

                if valid:
                    char_repr = chr(byte_val) if 32 <= byte_val <= 126 else '.'
                    decoded.append({
                        'sample_idx': start_sample,
                        'timestamp': start_sample / sample_rate,
                        'protocol': f"UART (Ch{rx_channel})",
                        'hex': f"0x{byte_val:02X}",
                        'ascii': char_repr
                    })
                    # Skip past stop bit
                    i = start_sample + (9 * samples_per_bit)
                else:
                    i += 1
            else:
                i += 1

        return decoded

    @staticmethod
    def decode_i2c(data_bytes, sample_rate, scl_ch=0, sda_ch=1):
        """
        Decodes I2C protocol transactions (START, Address+RW, Data, ACK/NACK, STOP).
        Returns list of decoded dicts: [{'timestamp': float, 'protocol': 'I2C', 'hex': str, 'ascii': str}]
        """
        if not data_bytes or sample_rate <= 0:
            return []

        samples = np.frombuffer(data_bytes, dtype=np.uint8)
        scl = (samples >> scl_ch) & 0x01
        sda = (samples >> sda_ch) & 0x01
        
        events = []
        n = len(samples)
        if n < 4:
            return []

        in_packet = False
        first_byte = True
        current_byte = 0
        bit_count = 0
        byte_start_idx = 0

        for i in range(1, n):
            scl_prev, scl_curr = scl[i-1], scl[i]
            sda_prev, sda_curr = sda[i-1], sda[i]

            # START condition: SDA falls while SCL is HIGH
            if scl_curr == 1 and sda_prev == 1 and sda_curr == 0:
                tag = "START" if not in_packet else "RESTART"
                events.append({
                    'sample_idx': i,
                    'timestamp': i / sample_rate,
                    'protocol': "I2C",
                    'hex': tag,
                    'ascii': "S" if tag == "START" else "Sr"
                })
                in_packet = True
                first_byte = True
                bit_count = 0
                current_byte = 0
                continue

            # STOP condition: SDA rises while SCL is HIGH
            if scl_curr == 1 and sda_prev == 0 and sda_curr == 1:
                if in_packet:
                    events.append({
                        'sample_idx': i,
                        'timestamp': i / sample_rate,
                        'protocol': "I2C",
                        'hex': "STOP",
                        'ascii': "P"
                    })
                    in_packet = False
                continue

            # Clock data in on SCL rising edge
            if in_packet and scl_prev == 0 and scl_curr == 1:
                if bit_count == 0:
                    byte_start_idx = i

                if bit_count < 8:
                    # Data bit (MSB first)
                    current_byte = (current_byte << 1) | sda_curr
                    bit_count += 1
                elif bit_count == 8:
                    # 9th bit: ACK/NACK (0 = ACK, 1 = NACK)
                    ack_str = "ACK" if sda_curr == 0 else "NACK"
                    if first_byte:
                        addr = (current_byte >> 1) & 0x7F
                        rw = "RD" if (current_byte & 0x01) else "WR"
                        events.append({
                            'sample_idx': byte_start_idx,
                            'timestamp': byte_start_idx / sample_rate,
                            'protocol': "I2C",
                            'hex': f"ADDR 0x{addr:02X} [{rw}]",
                            'ascii': f"{ack_str}"
                        })
                        first_byte = False
                    else:
                        char_repr = chr(current_byte) if 32 <= current_byte <= 126 else '.'
                        events.append({
                            'sample_idx': byte_start_idx,
                            'timestamp': byte_start_idx / sample_rate,
                            'protocol': "I2C",
                            'hex': f"DATA 0x{current_byte:02X}",
                            'ascii': f"'{char_repr}' {ack_str}"
                        })

                    bit_count = 0
                    current_byte = 0

        return events

    @staticmethod
    def decode_spi(data_bytes, sample_rate, sck_ch=2, mosi_ch=3, miso_ch=4, cs_ch=5, cpol=0, cpha=0):
        """
        Decodes SPI full-duplex transfers (MOSI & MISO) with configurable CPOL/CPHA and CS active-low.
        Returns list of decoded dicts: [{'timestamp': float, 'protocol': 'SPI', 'hex': str, 'ascii': str}]
        """
        if not data_bytes or sample_rate <= 0:
            return []

        samples = np.frombuffer(data_bytes, dtype=np.uint8)
        sck = (samples >> sck_ch) & 0x01
        mosi = (samples >> mosi_ch) & 0x01 if mosi_ch is not None and mosi_ch >= 0 else np.zeros(len(samples), dtype=np.uint8)
        miso = (samples >> miso_ch) & 0x01 if miso_ch is not None and miso_ch >= 0 else np.zeros(len(samples), dtype=np.uint8)
        cs = (samples >> cs_ch) & 0x01 if cs_ch is not None and cs_ch >= 0 else np.zeros(len(samples), dtype=np.uint8)

        events = []
        n = len(samples)
        if n < 2:
            return []

        # Sampling edge:
        # CPOL=0, CPHA=0: rising edge
        # CPOL=0, CPHA=1: falling edge
        # CPOL=1, CPHA=0: falling edge
        # CPOL=1, CPHA=1: rising edge
        sample_on_rising = ((cpol ^ cpha) == 0)

        mosi_byte = 0
        miso_byte = 0
        bit_count = 0
        byte_start_idx = 0
        prev_cs = cs[0]

        for i in range(1, n):
            cs_active = (cs[i] == 0) if (cs_ch is not None and cs_ch >= 0) else True

            # Detect CS edge events
            if cs_ch is not None and cs_ch >= 0:
                if prev_cs == 1 and cs[i] == 0:
                    events.append({
                        'sample_idx': i,
                        'timestamp': i / sample_rate,
                        'protocol': "SPI",
                        'hex': "CS_ASSERT",
                        'ascii': "START"
                    })
                    bit_count = 0
                    mosi_byte = 0
                    miso_byte = 0
                elif prev_cs == 0 and cs[i] == 1:
                    events.append({
                        'sample_idx': i,
                        'timestamp': i / sample_rate,
                        'protocol': "SPI",
                        'hex': "CS_DEASSERT",
                        'ascii': "END"
                    })
                    bit_count = 0
                prev_cs = cs[i]

            if not cs_active:
                continue

            sck_prev, sck_curr = sck[i-1], sck[i]
            is_sample_edge = (sck_prev == 0 and sck_curr == 1) if sample_on_rising else (sck_prev == 1 and sck_curr == 0)

            if is_sample_edge:
                if bit_count == 0:
                    byte_start_idx = i

                # Shift in MSB first
                mosi_byte = (mosi_byte << 1) | mosi[i]
                miso_byte = (miso_byte << 1) | miso[i]
                bit_count += 1

                if bit_count == 8:
                    mosi_char = chr(mosi_byte) if 32 <= mosi_byte <= 126 else '.'
                    miso_char = chr(miso_byte) if 32 <= miso_byte <= 126 else '.'

                    events.append({
                        'sample_idx': byte_start_idx,
                        'timestamp': byte_start_idx / sample_rate,
                        'protocol': "SPI",
                        'hex': f"TX:0x{mosi_byte:02X} RX:0x{miso_byte:02X}",
                        'ascii': f"TX:'{mosi_char}' RX:'{miso_char}'"
                    })
                    bit_count = 0
                    mosi_byte = 0
                    miso_byte = 0

        return events
