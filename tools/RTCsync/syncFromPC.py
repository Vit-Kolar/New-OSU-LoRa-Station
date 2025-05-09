import serial
import time
from datetime import datetime

SERIAL_PORT = 'COM4'
BAUDRATE = 9600

def send_time(ser):
    now = datetime.now()
    formatted = f"{now.year},{now.month},{now.day},{now.hour},{now.minute},{now.second},\n"
    ser.write(formatted.encode())
    print(f"Time sent: {formatted.strip()}")

def main():
    with serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1) as ser:
        time.sleep(2)  

        send_time(ser)

        print("Serial monitor started (press CTRL+C to stop)...")
        try:
            while True:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(f"{line}")
                time.sleep(0.1)  
        except KeyboardInterrupt:
            print("Stopped by user.")

if __name__ == "__main__":
    main()
