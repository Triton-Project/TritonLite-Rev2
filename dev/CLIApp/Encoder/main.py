import datetime

"""
year, month, day, hour, minute, second : 1Byte (0-255)
sup_start, sup_stop, exh_start, exh_stop : 2Bytes (0-65535)
checksum : 1Byte
lcd_mode, log_mode : ビットシフトで4bitずつ (0-15)

詳細情報 : https://1drv.ms/x/s!Ap1QA7D_yZ9yjLwx6heJXK6cVo8n1A?e=T9Ugq5
"""


def calculate_checksum(data_bytes):
    """バイト列の合計下位1Byteを取り，チェックサムを計算"""
    return sum(data_bytes) & 0xFF

def encode_data(year, month, day, hour, minute, second, sup_start, sup_stop, exh_start, exh_stop, lcd_mode, log_mode):
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
    while True:
        try:
            value = int(input(prompt))
            if 0 <= value <= max_value:
                return value
            else:
                print(f"Error: Value must be between 0 and {max_value}. Please try again.")
        except ValueError:
            print("Error: Invalid input. Please enter an integer.")

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