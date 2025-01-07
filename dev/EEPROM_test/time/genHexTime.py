

import datetime

def encode_datetime(dt):
    """日時を16進数にエンコード"""
    year = dt.year - 2000  # 2000年を基準にする
    month = dt.month
    day = dt.day
    hour = dt.hour
    minute = dt.minute
    second = dt.second
    
    encoded = f"{year:02x}{month:02x}{day:02x}{hour:02x}{minute:02x}{second:02x}"
    return encoded

def decode_datetime(hex_str):
    """16進数から日時をデコード"""
    year = int(hex_str[0:2], 16) + 2000
    month = int(hex_str[2:4], 16)
    day = int(hex_str[4:6], 16)
    hour = int(hex_str[6:8], 16)
    minute = int(hex_str[8:10], 16)
    second = int(hex_str[10:12], 16)
    
    return datetime.datetime(year, month, day, hour, minute, second)

# 現在時刻を取得
now = datetime.datetime.now()

# 現在時刻をエンコード
encoded_time = encode_datetime(now)
print(f"エンコードされた時刻: {encoded_time}")

# 文字列をバイト列に変換
data = encoded_time.encode()
print(f"データ: {data}") # 例: "19010715232c"

# エンコードされた時刻をデコード
decoded_time = decode_datetime(encoded_time)
print(f"デコードされた時刻: {decoded_time.strftime('%Y-%m-%d %H:%M:%S')}")