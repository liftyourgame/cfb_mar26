import serial
import sys

try:
    ser = serial.Serial('COM29', 115200, timeout=1)
    print("Connected to COM29 at 115200 baud")
    print("=" * 60)
    
    while True:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting)
            try:
                print(data.decode('utf-8', errors='replace'), end='')
            except:
                print(data.hex())
            sys.stdout.flush()
            
except KeyboardInterrupt:
    print("\nMonitor stopped by user")
except Exception as e:
    print(f"Error: {e}")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
