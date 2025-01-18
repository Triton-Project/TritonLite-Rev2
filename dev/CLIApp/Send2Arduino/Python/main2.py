import serial
import time
import datetime
import serial.tools.list_ports
from utils import calculate_checksum, encode_data, get_valid_input

def list_serial_ports():
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

def select_serial_port():
    ports = list_serial_ports()
    if not ports:
        print("No COM ports found.")
        return None

    print("Available COM ports:")
    for i, port in enumerate(ports):
        print(f"{i + 1}: {get_device_description(port)}")

    while True:
        try:
            choice = int(input("Select COM port: ")) - 1
            if 0 <= choice < len(ports):
                return ports[choice]
            else:
                print("Invalid selection. Please try again.")
        except ValueError:
            print("Invalid input. Please enter a number.")

def get_device_description(com_port):
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if port.device == com_port:
            return port.description
    return None

if __name__ == '__main__':

    # COMポートの選択
    com_port = select_serial_port()
    if com_port is None:
        print("No COM port selected. Exiting.")
        exit()

    # シリアルポートの設定
    ser = serial.Serial(com_port, 9600)  # 選択されたCOMポートを使用

    # シリアルポートが開くまで待機
    while not ser.is_open:
        time.sleep(0.1)  # 少し待機して再チェック
    print(f"Connected to {com_port}")

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
    ser.close()