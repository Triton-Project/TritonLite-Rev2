"""
# @file utils.py
# @author Shintaro Matsumoto
# @date 2025-01-18
# @brief Triton-Lite用のCLIアプリの関数類 for Windows
"""

import serial.tools.list_ports

def calculate_checksum(data_bytes):
    """
    @brief バイト列の合計下位1Byteを取り，チェックサムを計算
    @param data_bytes チェックサムを計算するバイト列
    @return チェックサム値
    """
    return sum(data_bytes) & 0xFF

def encode_data(year, month, day, hour, minute, second, sup_start, sup_stop, exh_start, exh_stop, lcd_mode, log_mode):
    """
    @brief データをエンコードして送信可能な形式に変換
    @param year 年
    @param month 月
    @param day 日
    @param hour 時
    @param minute 分
    @param second 秒
    @param sup_start sup_start値
    @param sup_stop sup_stop値
    @param exh_start exh_start値
    @param exh_stop exh_stop値
    @param lcd_mode lcd_mode値
    @param log_mode log_mode値
    @return エンコードされたデータの16進文字列
    """
    # HEADER
    header = 0x24  # '$' symbol

    # Time fields
    year_offset = year - 2000
    time_bytes = [
        year_offset, month, day, hour, minute, second
    ]

    # Event times (16-bit each)
    sup_start_bytes = sup_start.to_bytes(2, 'big')
    sup_stop_bytes = sup_stop.to_bytes(2, 'big')
    exh_start_bytes = exh_start.to_bytes(2, 'big')
    exh_stop_bytes = exh_stop.to_bytes(2, 'big')

    # Modes
    lcd_mode = lcd_mode & 0x0F  # 4 bits
    log_mode = log_mode & 0x0F  # 4 bits
    mode_byte = (lcd_mode << 4) | log_mode

    # Combine all bytes
    data_bytes = [
        header,
        *time_bytes,
        *sup_start_bytes,
        *sup_stop_bytes,
        *exh_start_bytes,
        *exh_stop_bytes,
        mode_byte,
    ]

    # Calculate checksum
    checksum = calculate_checksum(data_bytes)

    # Footer
    footer = 0x3B  # ';' symbol

    # Finalize the data
    data_bytes.append(checksum)
    data_bytes.append(footer)

    # Convert to hex string
    hex_string = ''.join(f'{byte:02X}' for byte in data_bytes)
    return f'{hex_string}'

def get_valid_input(prompt, max_value):
    """
    @brief ユーザーからの有効な入力を取得
    @param prompt 入力を促すメッセージ
    @param max_value 入力の最大値
    @return 有効な入力値
    """
    while True:
        try:
            value = int(input(prompt))
            if 0 <= value <= max_value:
                return value
            else:
                print(f"Error: Value must be between 0 and {max_value}. Please try again.")
        except ValueError:
            print("Error: Invalid input. Please enter an integer.")

def list_serial_ports():
    """
    @brief 利用可能なシリアルポートのリストを取得
    @return 利用可能なシリアルポートのリスト
    """
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

def select_serial_port():
    """
    @brief ユーザーにシリアルポートを選択させる
    @return 選択されたシリアルポート
    """
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
    """
    @brief 指定されたCOMポートのデバイス説明を取得
    @param com_port COMポート名
    @return デバイス説明
    """
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if port.device == com_port:
            return port.description
    return None

def triton_logo():
    """
    @brief Tritonのロゴを表示
    """
    print("""
$$$$$$$$\        $$\   $$\                                 $$\       $$\   $$\               
\__$$  __|       \__|  $$ |                                $$ |      \__|  $$ |              
   $$ | $$$$$$\  $$\ $$$$$$\    $$$$$$\  $$$$$$$\          $$ |      $$\ $$$$$$\    $$$$$$\  
   $$ |$$  __$$\ $$ |\_$$  _|  $$  __$$\ $$  __$$\ $$$$$$\ $$ |      $$ |\_$$  _|  $$  __$$\ 
   $$ |$$ |  \__|$$ |  $$ |    $$ /  $$ |$$ |  $$ |\______|$$ |      $$ |  $$ |    $$$$$$$$ |
   $$ |$$ |      $$ |  $$ |$$\ $$ |  $$ |$$ |  $$ |        $$ |      $$ |  $$ |$$\ $$   ____|
   $$ |$$ |      $$ |  \$$$$  |\$$$$$$  |$$ |  $$ |        $$$$$$$$\ $$ |  \$$$$  |\$$$$$$$\ 
   \__|\__|      \__|   \____/  \______/ \__|  \__|        \________|\__|   \____/  \_______|
""")