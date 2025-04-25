import datetime

"""
year, month, day, hour, minute, second : 1 Byte (0‑255)
sup_start, sup_stop, exh_start, exh_stop : 2 Bytes (0‑65535)
dive_count, press_threshold            : 1 Byte (0‑255)   ←★追加
lcd_mode, log_mode                     : 4 bit ずつ (0‑15)
checksum                               : 1 Byte
HEADER(0x24) … DATA … CHECKSUM … FOOTER(0x3B)
詳細情報 : https://1drv.ms/x/s!Ap1QA7D_yZ9yjLwx6heJXK6cVo8n1A?e=T9Ugq5
"""

# ------------------------------------------------------------
# 共通関数
# ------------------------------------------------------------
def calculate_checksum(data_bytes):
    """HEADER〜press_threshold までの合計下位 1 Byte をチェックサムとする"""
    return sum(data_bytes) & 0xFF


def get_valid_input(prompt, max_value):
    while True:
        try:
            value = int(input(prompt))
            if 0 <= value <= max_value:
                return value
            print(f"Error: Value must be between 0 and {max_value}. Try again.")
        except ValueError:
            print("Error: Invalid input. Please enter an integer.")


# ------------------------------------------------------------
# エンコード本体
# ------------------------------------------------------------
def encode_data(
    *,  # 以降はキーワード専用引数
    year,
    month,
    day,
    hour,
    minute,
    second,
    sup_start,
    sup_stop,
    exh_start,
    exh_stop,
    dive_count,
    press_threshold,
    lcd_mode,
    log_mode,
):
    # HEADER
    header = 0x24  # '$'

    # 時刻（Year は 2000 年オフセット）
    year_offset = year - 2000
    time_bytes = [year_offset, month, day, hour, minute, second]

    # 16 bit イベント時刻
    sup_start_bytes = sup_start.to_bytes(2, "big")
    sup_stop_bytes = sup_stop.to_bytes(2, "big")
    exh_start_bytes = exh_start.to_bytes(2, "big")
    exh_stop_bytes = exh_stop.to_bytes(2, "big")

    # 追加 1 Byte フィールド
    dive_count_byte = dive_count & 0xFF
    press_threshold_byte = press_threshold & 0xFF

    # 4 bit×2 のモード
    mode_byte = ((lcd_mode & 0x0F) << 4) | (log_mode & 0x0F)

    # チェックサム計算対象データを構築
    data_bytes = [
        header,
        *time_bytes,
        *sup_start_bytes,
        *sup_stop_bytes,
        *exh_start_bytes,
        *exh_stop_bytes,
        dive_count_byte,
        press_threshold_byte,
        mode_byte,
    ]

    # CHECKSUM
    checksum = calculate_checksum(data_bytes)

    # FOOTER
    footer = 0x3B  # ';'

    # 送信列完成
    frame = data_bytes + [checksum, footer]
    return "".join(f"{b:02X}" for b in frame)


# ------------------------------------------------------------
# 実行ブロック
# ------------------------------------------------------------
if __name__ == "__main__":
    # 現在時刻
    dt_now = datetime.datetime.now()

    # ユーザー入力
    sup_start = get_valid_input("Enter sup_start (0‑65535): ", 65535)
    sup_stop = get_valid_input("Enter sup_stop (0‑65535): ", 65535)
    exh_start = get_valid_input("Enter exh_start (0‑65535): ", 65535)
    exh_stop = get_valid_input("Enter exh_stop (0‑65535): ", 65535)
    dive_count = get_valid_input("Enter dive_count (0‑255): ", 255)
    press_threshold = get_valid_input("Enter press_threshold (0‑255): ", 255)
    lcd_mode = get_valid_input("Enter lcd_mode (0‑15): ", 15)
    log_mode = get_valid_input("Enter log_mode (0‑15): ", 15)

    # エンコード実行
    encoded = encode_data(
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
        dive_count=dive_count,
        press_threshold=press_threshold,
        lcd_mode=lcd_mode,
        log_mode=log_mode,
    )

    print(encoded)
