import time

# Import your custom modules
from Bt_manager import BTManager
from signal_reader import SignalReader
from blink_processor import SingleBlinkProcessor

def main():
    print("="*50)
    print("🧠 MindWave SINGLE BLINK Modular Test")
    print("="*50)

    # 1. Initialize our modules
    bt = BTManager(port='COM3', baud=57600)
    reader = SignalReader()
    processor = SingleBlinkProcessor()

    # 2. Connect to the headset
    if not bt.connect():
        print("Exiting...")
        return

    print("Waiting for signal quality to drop to 0 (Please put the headset on)...")

    # 3. Define the callback hook
    # This function gets triggered automatically by your SignalReader every time a raw value arrives
    def handle_raw_data(raw_val):
        # ONLY process the blink if the headset has a perfect connection (poor_signal == 0)
        if reader.state["poor_signal"] == 0:
            
            # Feed the raw value into our blink processor
            result = processor.process_raw(raw_val)
            
            # If the sequence is complete, print the massive alert
            if result:
                print("\n" + "🟢" * 20)
                print(f"👉 FINAL COMMAND: {result} 👈")
                print("🟢" * 20 + "\n")

    # Hook the callback to the reader
    reader.on_raw_received = handle_raw_data

    # Track signal status so we can print when it's ready
    is_ready = False

    # 4. The Main Loop
    try:
        while True:
            # We pass bt.ser directly into your reader
            payload = reader.read_packet(bt.ser)
            
            if payload:
                # This parses the packet, updates the state dictionary, 
                # AND automatically triggers our handle_raw_data hook
                reader.parse_payload(payload)

                # Just a nice visual check so you know when the headset is actually working
                if reader.state["poor_signal"] == 26 and not is_ready:
                    print("✅ SIGNAL IS PERFECT (0). System Armed. You can blink now.")
                    is_ready = True
                elif reader.state["poor_signal"] > 0 and is_ready:
                    print(f"⚠️ SIGNAL LOST (Quality: {reader.state['poor_signal']}). System Disarmed.")
                    is_ready = False

    except KeyboardInterrupt:
        print("\nTest stopped by user.")
    finally:
        bt.close()

if __name__ == "__main__":
    main()