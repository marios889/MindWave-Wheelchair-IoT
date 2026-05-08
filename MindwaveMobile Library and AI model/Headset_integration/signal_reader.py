import serial
import datetime

# NeuroSky Byte Codes
SYNC = b'\xaa'
EXCODE = 0x55
POOR_SIGNAL = 0x02
ATTENTION = 0x04
MEDITATION = 0x05
RAW_VALUE = 0x80
ASIC_EEG_POWER = 0x83

class SignalReader:
    def __init__(self):
        self.on_raw_received = None  # This is our empty hook (callback)
        # Initializing the internal state of the brainwaves
        self.state = {
            "attention": 0,
            "meditation": 0,
            "poor_signal": 200,
            "delta": 0,
            "theta": 0,
            "low_alpha": 0,
            "high_alpha": 0,
            "low_beta": 0,
            "high_beta": 0,
            "low_gamma": 0,
            "mid_gamma": 0,
            "blink": 0
        }

    def read_packet(self, ser):
        """Finds the sync bytes and validates the checksum of the packet."""
        try:
            if ser.read() == SYNC and ser.read() == SYNC:
                while True:
                    plength = ord(ser.read())
                    if plength != 170: # Skip extra sync bytes
                        break
                
                if plength > 170:
                    return None

                payload = ser.read(plength)
                
                # Checksum validation
                val = sum(payload) & 0xff
                val = (~val) & 0xff
                chksum = ord(ser.read())
                
                if val == chksum:
                    return payload
        except (OSError, serial.SerialException):
            return None
        return None

    def parse_payload(self, payload):
        """Iterates through the payload to update the state dictionary."""
        is_power_updated = False

        while payload:
            code = payload[0]
            payload = payload[1:]

            # Handle Extended Code (EXCODE)
            while code == EXCODE:
                code = payload[0]
                payload = payload[1:]

            # Single-byte codes (Attention, Meditation, Poor Signal)
            if code < 0x80:
                value = payload[0]
                payload = payload[1:]

                if code == POOR_SIGNAL:
                    self.state["poor_signal"] = value
                elif code == ATTENTION:
                    self.state["attention"] = value
                elif code == MEDITATION:
                    self.state["meditation"] = value

            # Multi-byte codes (Raw Wave, EEG Power Bands)
            else:
                vlength = payload[0]
                payload = payload[1:]
                value = payload[:vlength]
                payload = payload[vlength:]

                if code == RAW_VALUE and len(value) >= 2:
                    raw = value[0] * 256 + value[1]
                    if raw >= 32768:
                        raw -= 65536

                    # self.state["raw"] = raw

                    if self.on_raw_received:
                        self.on_raw_received(raw)

                elif code == ASIC_EEG_POWER and len(value) >= 24:
                    names = [
                        "delta", "theta", "low_alpha", "high_alpha",
                        "low_beta", "high_beta", "low_gamma", "mid_gamma"
                    ]
                    j = 0
                    for name in names:
                        # 3-byte big-endian values for EEG bands
                        self.state[name] = value[j]*65536 + value[j+1]*256 + value[j+2]
                        j += 3
                    
                    is_power_updated = True

                    # Optional: Print for debugging
                    print(f"[{datetime.datetime.now().strftime('%H:%M:%S')}] "
                            f"Att: {self.state['attention']} | Med: {self.state['meditation']} | Signal: {self.state['poor_signal']}")
        if is_power_updated:
            return self.state
        return None