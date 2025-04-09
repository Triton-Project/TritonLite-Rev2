import serial
import time
import datetime

def sync_datetime(): # PCの時刻をArduinoに送信
    ser = serial.Serial('/dev/tty.usbmodem11201', 9600) # Macの場合，WindowsはCOMポートを指定
    while not ser.is_open:
        time.sleep(0.1)  # 接続するまで待機

    try:
        dt_now = datetime.datetime.now() #PCの時刻を取得
        data_to_send = dt_now.strftime("%Y-%m-%d %H:%M:%S")
        print(f"Sending to Arduino: {data_to_send}")
        ser.write((data_to_send + '\n').encode())

        confirmation = ser.readline().decode().strip()
        print(f"Arduino response: {confirmation}")

    except KeyboardInterrupt:
        print("Exiting...")

    finally:
        ser.close()

sync_datetime()