import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import numpy as np
import sys

# ==========================================
# الإعدادات
# ==========================================
ESP32_PORT = 'COM5'  # تأكد إن ده البورت بتاعك
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
WINDOW_SIZE = 512 
y_data = deque([0] * WINDOW_SIZE, maxlen=WINDOW_SIZE)
x_data = np.linspace(0, 1, WINDOW_SIZE)

fig, ax = plt.subplots(figsize=(10, 5))
fig.canvas.manager.set_window_title('MindWave Live Raw Data')
line, = ax.plot(x_data, y_data, color='#00ff00', linewidth=1)

# تنسيق الشاشة عشان تستوعب الـ 1000 بتاعتك
ax.set_ylim(-1000, 2000) 
ax.set_xlim(0, 1)
ax.set_facecolor('black')
fig.patch.set_facecolor('#222222')
ax.tick_params(colors='white')

# رسم الخطوط على أرقامك بالظبط
UPPER_THRESH = 1000
LOWER_THRESH = 450
ax.axhline(y=UPPER_THRESH, color='red', linestyle='--', label=f'Upper ({UPPER_THRESH})')
ax.axhline(y=LOWER_THRESH, color='blue', linestyle='--', label=f'Lower ({LOWER_THRESH})')
ax.legend(loc='upper right')

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