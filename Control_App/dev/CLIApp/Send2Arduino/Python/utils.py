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