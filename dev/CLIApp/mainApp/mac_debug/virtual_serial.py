# File: virtual_serial.py
# Author: Shintaro Matsumoto(m-shintaro)
# Date: 2025-01-19
# Description: 仮想シリアルポートを使用し，アプリからの入力に返答するデバッグスクリプト

"""
Mac専用です

sudo socat -d -d PTY,raw,echo=0 PTY,raw,echo=0
で出てきたポート名(/dev/ttys000とか)の権限を変更
sudo chmod 666 /dev/ttys000
sudo chmod 666 /dev/ttys001

番号が若い方をmain_mac.pyのcom_portに入れる
遅い方はvirtual_serial.pyのcreate_serial_connectionの第三引数に入れる
"""

import asyncio
import serial_asyncio

class SerialProtocol(asyncio.Protocol):
    def __init__(self):
        self.transport = None

    def connection_made(self, transport):
        self.transport = transport
        print('Serial port opened')

    def data_received(self, data):
        received_data = data.decode().strip()
        print(f"Received: {received_data}")
        self.decode_data(received_data)

    def connection_lost(self, exc):
        print('Serial port closed')
        self.transport.loop.stop()

    def decode_data(self, encoded_string):
        print(f"Decoding data: {encoded_string}")
        length = len(encoded_string) // 2
        data_bytes = bytearray(length)

        # Convert hex string to bytes
        for i in range(length):
            data_bytes[i] = int(encoded_string[2 * i:2 * i + 2], 16)

        # Extract the footer and checksum
        footer = data_bytes[-1]
        checksum = data_bytes[-2]
        data_bytes = data_bytes[:-2]

        # Extract data fields
        header = data_bytes[0]
        if header != 0x24:
            print("Invalid header")
            return

        # Verify footer
        if footer != 0x3B:
            print("Invalid footer")
            return

        # Verify checksum
        calculated_checksum = self.calculate_checksum(data_bytes)
        if calculated_checksum != checksum:
            print("Checksum does not match")
            return

        year_offset = data_bytes[1]
        year = 2000 + year_offset
        month = data_bytes[2]
        day = data_bytes[3]
        hour = data_bytes[4]
        minute = data_bytes[5]
        second = data_bytes[6]

        sup_start = (data_bytes[7] << 8) | data_bytes[8]
        sup_stop = (data_bytes[9] << 8) | data_bytes[10]
        exh_start = (data_bytes[11] << 8) | data_bytes[12]
        exh_stop = (data_bytes[13] << 8) | data_bytes[14]

        mode_byte = data_bytes[15]
        lcd_mode = (mode_byte >> 4) & 0x0F
        log_mode = mode_byte & 0x0F

        # Print decoded data
        print(f"Year: {year}")
        print(f"Month: {month}")
        print(f"Day: {day}")
        print(f"Hour: {hour}")
        print(f"Minute: {minute}")
        print(f"Second: {second}")
        print(f"Sup Start: {sup_start}")
        print(f"Sup Stop: {sup_stop}")
        print(f"Exh Start: {exh_start}")
        print(f"Exh Stop: {exh_stop}")
        print(f"LCD Mode: {lcd_mode}")
        print(f"Log Mode: {log_mode}")
        print("Checksum valid: true")

        # Send response back
        response = "Checksum valid: true\n"
        self.transport.write(response.encode())
        print("Response sent")

    def calculate_checksum(self, data_bytes):
        return sum(data_bytes) & 0xFF

async def main():
    loop = asyncio.get_running_loop()
    transport, protocol = await serial_asyncio.create_serial_connection(loop, SerialProtocol, '/dev/ttys018', baudrate=9600)

    try:
        await asyncio.sleep(3600)  # Keep the loop running for an hour
    except KeyboardInterrupt:
        pass
    finally:
        transport.close()

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except RuntimeError as e:
        if str(e) == "This event loop is already running":
            loop = asyncio.get_event_loop()
            loop.run_until_complete(main())
        else:
            raise