import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import numpy as np

# Import your Chad architecture
from Bt_manager import BTManager
from signal_reader import SignalReader

# 1. Setup the Graph Data
WINDOW_SIZE = 512 # 1 second of data at 512Hz
y_data = deque([0] * WINDOW_SIZE, maxlen=WINDOW_SIZE)
x_data = np.linspace(0, 1, WINDOW_SIZE)

# 2. Setup the Plot visual style
fig, ax = plt.subplots(figsize=(10, 5))
fig.canvas.manager.set_window_title('MindWave Live Raw Data')
line, = ax.plot(x_data, y_data, color='#00ff00', linewidth=1)

# Format the graph
ax.set_ylim(-1500, 1500) # Most normal raw data falls between -1000 and 1000
ax.set_xlim(0, 1)
ax.set_facecolor('black')
fig.patch.set_facecolor('#222222')
ax.tick_params(colors='white')
ax.set_title("Real-Time Raw EEG & Blink Detection", color='white')

# Draw the Threshold Line (So you can see when your blinks cross it)
THRESHOLD = 800
ax.axhline(y=THRESHOLD, color='red', linestyle='--', label=f'Threshold ({THRESHOLD})')
ax.legend(loc='upper right')

# 3. Setup Bluetooth and Reader
bt = BTManager(port='COM3', baud=57600)
reader = SignalReader()

if not bt.connect():
    print("Cannot start visualizer without headset connection.")
    exit()

# 4. The Callback Hook
def handle_raw(raw_val):
    # Only draw the data if the signal is decent (0 or 26)
    if reader.state["poor_signal"] <= 26:
        y_data.append(raw_val)
    else:
        # If signal drops, draw zeros so we visually see the drop
        y_data.append(0)

reader.on_raw_received = handle_raw

# 5. The Animation Loop
def update(frame):
    # Read a batch of packets fast to keep up with the 512Hz stream
    # We do a mini-loop here otherwise the graph lags behind reality
    for _ in range(30):
        payload = reader.read_packet(bt.ser)
        if payload:
            reader.parse_payload(payload)
            
    # Update the line on the graph with the new deque data
    line.set_ydata(list(y_data))
    return line,

print("Graph is opening... Put the headset on.")

# Start the live animation
ani = animation.FuncAnimation(fig, update, interval=20, blit=True, cache_frame_data=False)
plt.show()

# Cleanup when you close the graph window
bt.close()