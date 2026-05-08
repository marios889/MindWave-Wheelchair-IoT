import time

class JawClenchProcessor:
    def __init__(self, threshold=1500, required_spikes=10):
        self.threshold = threshold
        self.required_spikes = required_spikes
        self.spike_count = 0
        self.last_check_time = time.time()
        
        # Result flag
        self.clench_detected = False

    def process_raw(self, raw_val):
        """
        Checks for high-frequency, high-amplitude EMG noise (Jaw Clench).
        """
        current_time = time.time()
        
        # 1. Check if the current value is 'screaming' (high amplitude)
        if abs(raw_val) > self.threshold:
            self.spike_count += 1
            
        # 2. Every 100ms, evaluate the density of spikes
        if current_time - self.last_check_time > 0.1:
            if self.spike_count >= self.required_spikes:
                self.clench_detected = True
            else:
                self.clench_detected = False
                
            # Reset for the next window
            self.spike_count = 0
            self.last_check_time = current_time

    def is_clenched(self):
        return self.clench_detected