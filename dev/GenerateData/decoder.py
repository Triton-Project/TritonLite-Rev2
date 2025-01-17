def calculate_checksum(data_bytes):
    """Calculate the checksum by summing the bytes and taking the least significant byte."""
    return sum(data_bytes) & 0xFF

def decode_data(encoded_string):
    """Decode the data according to the specified protocol and verify checksum."""
    if not (encoded_string.startswith('$') and encoded_string.endswith(';')):
        raise ValueError("Invalid data format")

    # Remove the start and end symbols
    encoded_string = encoded_string[1:-1]

    # Convert hex string to bytes
    data_bytes = bytes.fromhex(encoded_string)

    # Extract the footer and checksum
    footer = data_bytes[-2]
    checksum = data_bytes[-1]
    data_bytes = data_bytes[:-2]

    # Verify footer
    if footer != 0x3B:
        raise ValueError("Invalid footer")

    # Verify checksum
    calculated_checksum = calculate_checksum(data_bytes)
    if calculated_checksum != checksum:
        raise ValueError("Checksum does not match")

    # Extract data fields
    header = data_bytes[0]
    if header != 0x24:
        raise ValueError("Invalid header")

    year_offset = data_bytes[1]
    year = 2000 + year_offset
    month = data_bytes[2]
    day = data_bytes[3]
    hour = data_bytes[4]
    minute = data_bytes[5]
    second = data_bytes[6]

    sup_start = int.from_bytes(data_bytes[7:9], 'big')
    sup_stop = int.from_bytes(data_bytes[9:11], 'big')
    exh_start = int.from_bytes(data_bytes[11:13], 'big')
    exh_stop = int.from_bytes(data_bytes[13:15], 'big')

    mode_byte = data_bytes[15]
    lcd_mode = (mode_byte >> 4) & 0x0F
    log_mode = mode_byte & 0x0F

    return {
        "year": year,
        "month": month,
        "day": day,
        "hour": hour,
        "minute": minute,
        "second": second,
        "sup_start": sup_start,
        "sup_stop": sup_stop,
        "exh_start": exh_start,
        "exh_stop": exh_stop,
        "lcd_mode": lcd_mode,
        "log_mode": log_mode,
        "checksum_valid": True
    }

# Example usage
encoded_string = "$2419010713093600320D05100033FF3B3B58;"
decoded_data = decode_data(encoded_string)
print(decoded_data)