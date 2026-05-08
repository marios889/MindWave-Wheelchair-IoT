import serial
import time

class BTManager:
    def __init__(self, port='COM3', baud=57600):
        self.port = port
        self.baud = baud
        self.ser = None

    def connect(self):
        """Initializes the serial connection."""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=2)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            print(f"✅ Connected to {self.port} successfully.")
            return True
        except serial.SerialException as e:
            print(f"❌ Serial error: {e}")
            return False

    def read_byte(self):
        """Reads a single byte from the stream."""
        if self.ser and self.ser.is_open:
            return self.ser.read(1)
        return None

    def close(self):
        """Safely closes the connection."""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("🔌 Serial port closed.")