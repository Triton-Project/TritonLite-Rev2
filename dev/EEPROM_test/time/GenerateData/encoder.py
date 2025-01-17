# データフォーマット: https://1drv.ms/x/s!Ap1QA7D_yZ9yjLwx6heJXK6cVo8n1A?e=T9Ugq5

def calculate_checksum(data_bytes):
    """Calculate the checksum by summing the bytes and taking the least significant byte."""
    return sum(data_bytes) & 0xFF

def encode_data(year, month, day, hour, minute, second, sup_start, sup_stop, exh_start, exh_stop, lcd_mode, log_mode):
    """Format the data according to the specified protocol."""
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
    data_bytes.append(footer)
    data_bytes.append(checksum)

    # Convert to hex string
    hex_string = ''.join(f'{byte:02X}' for byte in data_bytes)
    return f'${hex_string};'

# Example usage
data_string = encode_data(
    year=2025,
    month=1,
    day=7,
    hour=19,
    minute=9,
    second=54,
    sup_start=50,
    sup_stop=3333,
    exh_start=4096,
    exh_stop=13311,
    lcd_mode=3,
    log_mode=11
)

print(data_string)
