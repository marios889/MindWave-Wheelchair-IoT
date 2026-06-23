import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.widgets import TextBox
from collections import deque
import numpy as np
import sys

# ==========================================
# الإعدادات
# ==========================================
ESP32_PORT = 'COM6'  # تأكد إن ده البورت بتاعك
BAUD_RATE = 115200

try:
    ser = serial.Serial(ESP32_PORT, BAUD_RATE, timeout=0.01)
    print(f"Connected to ESP32 on {ESP32_PORT}")
except Exception as e:
    print(f"Error connecting to {ESP32_PORT}. Is VS Code Serial Monitor closed?")
    sys.exit()

# ==========================================
# إعدادات الجراف
# ==========================================
WINDOW_SIZE = 1024
y_data = deque([0] * WINDOW_SIZE, maxlen=WINDOW_SIZE)
x_data = np.linspace(0, 2, WINDOW_SIZE)

fig, ax = plt.subplots(figsize=(10, 6)) # Slightly taller to fit the text box
fig.canvas.manager.set_window_title('MindWave Live Raw Data')

# Make room for the text box at the bottom
plt.subplots_adjust(bottom=0.2)

line, = ax.plot(x_data, y_data, color='#00ff00', linewidth=1)

# تنسيق الشاشة عشان تستوعب الـ 1000 بتاعتك
ax.set_ylim(-1000, 2000) 
ax.set_xlim(0, 2)
ax.set_facecolor('black')
fig.patch.set_facecolor('#222222')
ax.tick_params(colors='white')

# رسم الخطوط على أرقامك بالظبط
UPPER_THRESH = 1000
LOWER_THRESH = 450
ax.axhline(y=UPPER_THRESH, color='red', linestyle='--', label=f'Upper ({UPPER_THRESH})')
ax.axhline(y=LOWER_THRESH, color='blue', linestyle='--', label=f'Lower ({LOWER_THRESH})')
ax.legend(loc='upper right')
ax.set_xlabel('Time (Seconds)', color='white')
ax.set_ylabel('Raw EEG (Amplitude)', color='white')

# ==========================================
# Text Box Setup (SEND COMMANDS)
# ==========================================
# Position: [left, bottom, width, height]
axbox = fig.add_axes([0.2, 0.05, 0.6, 0.075])
# FIXED LINE:
text_box = TextBox(axbox, 'Command: ', color='0.95', hovercolor='1.0')
text_box.label.set_color('white')

def submit_command(text):
    if ser and ser.is_open and text:
        # Send the character over serial
        ser.write(text.encode('utf-8'))
        print(f"\n>> Sent Command to Wemos: {text}\n")
    text_box.set_val("") # Clear the box automatically after sending

# Bind the Enter key to the submit function
text_box.on_submit(submit_command)

# ==========================================
# دورة التحديث
# ==========================================
def update(frame):
    while ser.in_waiting:
        try:
            line_data = ser.readline().decode('utf-8', errors='ignore').strip()
            
            if line_data:
                # لو داتا للرسم
                if line_data.startswith("RAW:"):
                    try:
                        val = int(line_data.split(":")[1])
                        y_data.append(val)
                    except:
                        pass
                # لو رسائل تانية، اطبعها زي ما هي
                else:
                    print(line_data) 
                    
        except Exception as e:
            pass 
            
    line.set_ydata(list(y_data))
    return line,

# تشغيل الجراف
ani = animation.FuncAnimation(fig, update, interval=20, blit=True, cache_frame_data=False)
plt.show()

ser.close()