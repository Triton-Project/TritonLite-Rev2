"""
# @file main.py
# @author Shintaro Matsumoto
# @date 2025-01-18
# @brief Triton-Lite用のCLIアプリ for Windows
"""

import serial
import time
import datetime
import serial.tools.list_ports
from utils import encode_data, get_valid_input, select_serial_port, triton_logo

if __name__ == '__main__':
    triton_logo()
    while True:
        # COMポートの選択
        com_port = select_serial_port()
        if com_port is None:
            print("No COM port selected. Exiting.")
            exit()

        try:
            # シリアルポートの設定
            ser = serial.Serial(com_port, 9600)  # 選択されたCOMポートを使用

            # シリアルポートが開くまで待機
            while not ser.is_open:
                time.sleep(0.1)  # 少し待機して再チェック
            print(f"Connected to {com_port}")
            break
        except serial.SerialException as e:
            print(f"Error: Could not open port {com_port}. {e}")
            print("If you have more than one instance of the application open, please close them and try again.")

    # 現在時刻を取得
    dt_now = datetime.datetime.now()

    # ユーザーに入力を求める
    sup_start = get_valid_input("Enter sup_start: ", 65535)
    sup_stop = get_valid_input("Enter sup_stop: ", 65535)
    exh_start = get_valid_input("Enter exh_start: ", 65535)
    exh_stop = get_valid_input("Enter exh_stop: ", 65535)
    lcd_mode = get_valid_input("Enter lcd_mode: ", 15)
    log_mode = get_valid_input("Enter log_mode: ", 15)

    # データをエンコード
    data_string = encode_data(
        year=dt_now.year,
        month=dt_now.month,
        day=dt_now.day,
        hour=dt_now.hour,
        minute=dt_now.minute,
        second=dt_now.second,
        sup_start=sup_start,
        sup_stop=sup_stop,
        exh_start=exh_start,
        exh_stop=exh_stop,
        lcd_mode=lcd_mode,
        log_mode=log_mode
    )

    print(data_string)

    # 送信するデータ
    data = data_string

    # データを送信
    ser.write(data.encode())
    print(f"Data sent to {com_port}")

    # データを受信し続ける
    while True:
        if ser.in_waiting > 0:
            received_data = ser.readline().decode().strip()
            print(f"Received: {received_data}")
            if "Checksum valid: true" in received_data:
                print("Checksum valid. Closing connection...")
                break
    ser.close()
    time.sleep(3)