import serial
import time
import datetime
from utils import calculate_checksum, encode_data, get_valid_input

"""
year, month, day, hour, minute, second : 1Byte (0-255)
sup_start, sup_stop, exh_start, exh_stop : 2Bytes (0-65535)
checksum : 1Byte
lcd_mode, log_mode : ビットシフトで4bitずつ (0-15)

詳細情報 : https://1drv.ms/x/s!Ap1QA7D_yZ9yjLwx6heJXK6cVo8n1A?e=T9Ugq5ああ
"""

if __name__ == '__main__':
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

    # シリアルポートの設定
    ser = serial.Serial('COM11', 9600)  # COMポートは適宜変更してください
    time.sleep(2)  # 接続の安定化のために少し待つ

    # 送信するデータ
    data = data_string

    # データを送信
    ser.write(data.encode())

    # データを受信し続ける
    while True:
        if ser.in_waiting > 0:
            received_data = ser.readline().decode().strip()
            print(f"Received: {received_data}")

    # シリアルポートを閉じる
    ser.close()