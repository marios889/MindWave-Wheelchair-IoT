import time
import pydirectinput
import re
from Bt_manager import BTManager
from signal_reader import SignalReader
import pyautogui
import mouse
import keyboard

# Importing your exact, untouched processor
from blink_processor import SingleBlinkProcessor

def run_minecraft_bci():
    print("🎧 Connecting to MindWave on COM3...")
    bt = BTManager(port='COM3', baud=57600)
    if not bt.connect():
        return

    reader = SignalReader()
    processor = SingleBlinkProcessor()

    print("\n✅ SYSTEM READY. You have 5 seconds to Alt-Tab to Minecraft...")
    time.sleep(2)
    print("🎮 BCI ACTIVE. Use your eyes to play. (Ctrl+C to stop)")

    def handle_raw(raw_val):
        # 1. Deadman Switch (Allowing up to 50 for electrode motion artifacts)
        if reader.state["poor_signal"] <= 50:
            
            # 2. Feed your untouched processor
            result = processor.process_raw(raw_val)
            
            # 3. MOVEMENT LOGIC
            if result:
                # result looks like: "SINGLE_BLINK (Schmitt Trigger Logic - 2 blinks caught)"
                # We use regex to extract the exact integer count from that string
                match = re.search(r'- (\d+) blinks', result)
                
                if match:
                    count = int(match.group(1))
                    attention = reader.state.get("attention", 0)

                    # --- YOUR 4 COMMANDS ---
                    if count == 1:
                        print(f"🚀 [1 BLINK] -> COMMAND: V | Att: {attention}")
                        pydirectinput.press('v')
                        
                    elif count == 2:
                        if attention > 45:
                            print(f"⌨️ [2 BLINKS + HIGH ATTENTION] -> COMMAND: ENTER | Att: {attention}")
                            pydirectinput.click()
                        else:
                            print(f"🖱️ [2 BLINKS] -> COMMAND: Scroll Up | Att: {attention}")
                            mouse.wheel(1)  # Increased to 100 for a noticeable Minecraft hotbar shift
                            
                    elif count >= 3:
                        print(f"🖱️ [3+ BLINKS] -> COMMAND: Scroll Down | Att: {attention}")
                        pydirectinput.press('f5')

    # Hook the logic to the raw data stream
    reader.on_raw_received = handle_raw
    
    try:
        while True:
            # Buffer Overflow Protection
            if bt.ser.in_waiting > 100:
                bt.ser.reset_input_buffer()
                continue
                
            payload = reader.read_packet(bt.ser)
            if payload:
                reader.parse_payload(payload)
                
    except KeyboardInterrupt:
        print("\n🛑 Shutting down BCI...")
        bt.close()

if __name__ == "__main__":
    run_minecraft_bci()