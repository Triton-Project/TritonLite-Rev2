# Copilotに書かせたのでちゃんと動くかは不明

import serial.tools.list_ports

def get_device_description(com_port):
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if port.device == com_port:
            return port.description
    return None

def get_arduino_ports():
    # 接続されているすべてのポートを取得
    ports = serial.tools.list_ports.comports()
    arduino_ports = []

    # 各ポートのデバイスディスクリプションを調べ、「Arduino」を含むものを抽出
    for port in ports:
        if "Arduino" in port.description:
            arduino_ports.append(port.device)

    return arduino_ports

# Arduinoが接続されているCOMポートを取得
arduino_ports = get_arduino_ports()

if arduino_ports:
    print("Arduinoが接続されているポート:")
    for port in arduino_ports:
        print(get_device_description(port))
else:
    print("Arduinoが接続されているポートは見つかりませんでした。")
