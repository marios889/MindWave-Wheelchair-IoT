import time

class SingleBlinkProcessor:
    def __init__(self):
        # --- THE SCHMITT TRIGGER HYSTERESIS ---
        self.UPPER_THRESH = 1000  # Wave must cross this to count as a blink
        self.LOWER_THRESH = 450     # Wave must crash below this to reset

        self.FAST_GAP = 1.2       # Still wait 1.2s to fire the final command
        
        # Memory & State
        self.blink_timestamps = []
        self.last_detection_time = 0
        
        # THIS IS THE MAGIC VARIABLE (The State)
        self.is_blinking = False  

    def process_raw(self, raw_value):
        now = time.time()

        # STATE 1: We are IDLE, looking for a huge spike
        if not self.is_blinking:
            if raw_value > self.UPPER_THRESH:
                self.is_blinking = True
                self.blink_timestamps.append(now)
                self.last_detection_time = now
                print("  [+] Eye Closed! (Crossed Upper Threshold)")

        # STATE 2: We are IN A BLINK, waiting for the wave to crash
        elif self.is_blinking:
            if raw_value < self.LOWER_THRESH:
                self.is_blinking = False
                # print("  [-] Eye Opened! (Crossed Lower Threshold - System Reset)")

        # Always check if the 1.2s waiting period is over
        return self.check_sequence()

    def check_sequence(self):
        if not self.blink_timestamps:
            return None

        now = time.time()
        
        # If 1.2 seconds have passed since the blink STARTED
        if (now - self.last_detection_time) > self.FAST_GAP:
            
            count = len(self.blink_timestamps)
            self.blink_timestamps = [] 
            
            if count >= 1:
                return f"SINGLE_BLINK (Schmitt Trigger Logic - {count} blinks caught)"
                
        return None