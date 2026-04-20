import time
import pydirectinput # Guarantees Minecraft sees the key presses
import pandas as pd
from bci_ai_model import BCIModel
from signal_reader import SignalReader
from Bt_manager import BTManager

def run_minecraft_bci_flex():
    print("🧠 Loading AI Model...")
    ai = BCIModel(model_path="test_rf_model.joblib")
    
    # Load the model and its required feature list
    ai.load_or_train(csv_path="master_raw_training_data.csv")

    if ai.model is None:
        print("❌ Critical Error: Model failed to load.")
        return

    print("🎧 Connecting to headset on COM3...")
    bt = BTManager(port='COM3', baud=57600)
    if not bt.connect():
        return

    reader = SignalReader()

    print("\n✅ SYSTEM READY. You have 5 seconds to Alt-Tab to Minecraft...")
    time.sleep(5)
    print("🎮 BCI ACTIVE. Recording Mode. Press Ctrl+C to stop.")

    cooldown_seconds = 0.8
    last_print_time = 0.0
    last_action_time = 0.0
    FORWARD_THRESHOLD = 0.40 # If you eventually add 'F' back in

    try:
        while True:
            # 1. Buffer Overflow Protection
            if bt.ser.in_waiting > 100:
                bt.ser.reset_input_buffer()
                continue

            # 2. Read raw bytes
            payload = reader.read_packet(bt.ser)
            if not payload:
                continue
                
            # 3. Parse payload (1Hz update)
            state = reader.parse_payload(payload)
            if not state:
                continue

            # 4. Deadman Switch
            if state["poor_signal"] > 0:
                print(f"⚠️ BAD SIGNAL ({state['poor_signal']})! Pausing commands.")
                continue

            # --- THE MAGIC FIX: Buffer & Math Engine ---
            # Add current state to the AI's memory buffer (keeps last 3 seconds)
            ai.state_buffer.append(state)
            if len(ai.state_buffer) > ai.window_size:
                ai.state_buffer.pop(0)
            
            # Convert buffer to dataframe and run the math engine
            df_buffer = pd.DataFrame(ai.state_buffer)
            df_engineered = ai._engineer_features(df_buffer)
            
            # Extract the final calculated row
            live_row = df_engineered.iloc[-1:]
            
            # Make sure columns match perfectly to prevent crashes
            for f in ai.features:
                if f not in live_row.columns:
                    live_row[f] = 0
            live_row = live_row[ai.features]

            # 5. AI Prediction (Now safe to get probabilities!)
            probs = ai.model.predict_proba(live_row)[0]
            class_probs = dict(zip(ai.model.classes_, probs))
            
            prediction = ai.model.predict(live_row)[0]
            confidence = max(probs) * 100
            f_prob = class_probs.get('F', 0.0)

            # 6. Movement Logic with Cooldown
            current_time = time.time()
            time_since_last_action = current_time - last_action_time

            # If we are in the cooldown window, block commands
            if time_since_last_action >= cooldown_seconds:
            
                if prediction == 'L':
                    # Priority 1: Left (Transform)
                    pydirectinput.click(button='left')
                    last_action_time = time.time() 
                    print(f"[LIVE] 👽 COMMAND: TRANSFORM (Left Click) | Conf: {confidence:.1f}% | Att: {state.get('attention', 0)}")
                    
                elif f_prob > FORWARD_THRESHOLD:
                    # Priority 2: Forward (If you trained an 'F' class)
                    # pydirectinput.press('v')
                    last_action_time = time.time()
                    print(f"[LIVE] 💥 COMMAND: FORWARD (V) | F-Prob: {f_prob:.2f} | Att: {state.get('attention', 0)}")
                
                elif prediction == 'N':
                    # Neutral state
                    if current_time - last_print_time > 1.0:
                        print(f"[LIVE] 💤 Idle (Neutral) | Conf: {confidence:.1f}%")
                        last_print_time = current_time

    except KeyboardInterrupt:
        print("\n🛑 Shutting down BCI...")
        bt.close()

if __name__ == "__main__":
    run_minecraft_bci_flex()