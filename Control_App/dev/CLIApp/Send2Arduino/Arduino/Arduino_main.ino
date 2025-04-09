void setup() {
  // シリアル通信の初期化
  Serial.begin(9600);
  while (!Serial) {
    ;  // シリアルポートが接続されるまで待つ
  }
}

void loop() {
  // シリアルデータの受信
  if (Serial.available() > 0) {
    String receivedData = Serial.readStringUntil('\n');
    Serial.print("Received: ");
    Serial.println(receivedData);

    // デコード処理
    decode_data(receivedData.c_str());
  }
}

uint8_t calculate_checksum(uint8_t* data_bytes, size_t length) {
  uint8_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data_bytes[i];
  }
  return sum & 0xFF;
}

void decode_data(const char* encoded_string) {
  size_t length = strlen(encoded_string) / 2;
  uint8_t data_bytes[length];

  // Convert hex string to bytes
  for (size_t i = 0; i < length; i++) {
    sscanf(encoded_string + 2 * i, "%2hhx", &data_bytes[i]);
  }

  // Extract the footer and checksum
  uint8_t footer = data_bytes[length - 1];
  uint8_t checksum = data_bytes[length - 2];
  length -= 2;

  // Extract data fields
  uint8_t header = data_bytes[0];
  if (header != 0x24) {
    Serial.println("Invalid header");
    return;
  }

  // Verify footer
  if (footer != 0x3B) {
    Serial.println("Invalid footer");
    return;
  }

  // Verify checksum
  uint8_t calculated_checksum = calculate_checksum(data_bytes, length);
  if (calculated_checksum != checksum) {
    Serial.println("Checksum does not match");
    return;
  }

  uint8_t year_offset = data_bytes[1];
  uint16_t year = 2000 + year_offset;
  uint8_t month = data_bytes[2];
  uint8_t day = data_bytes[3];
  uint8_t hour = data_bytes[4];
  uint8_t minute = data_bytes[5];
  uint8_t second = data_bytes[6];

  uint16_t sup_start = (data_bytes[7] << 8) | data_bytes[8];
  uint16_t sup_stop = (data_bytes[9] << 8) | data_bytes[10];
  uint16_t exh_start = (data_bytes[11] << 8) | data_bytes[12];
  uint16_t exh_stop = (data_bytes[13] << 8) | data_bytes[14];

  uint8_t mode_byte = data_bytes[15];
  uint8_t lcd_mode = (mode_byte >> 4) & 0x0F;
  uint8_t log_mode = mode_byte & 0x0F;

  // Print decoded data
  Serial.print("Year: ");
  Serial.println(year);
  Serial.print("Month: ");
  Serial.println(month);
  Serial.print("Day: ");
  Serial.println(day);
  Serial.print("Hour: ");
  Serial.println(hour);
  Serial.print("Minute: ");
  Serial.println(minute);
  Serial.print("Second: ");
  Serial.println(second);
  Serial.print("Sup Start: ");
  Serial.println(sup_start);
  Serial.print("Sup Stop: ");
  Serial.println(sup_stop);
  Serial.print("Exh Start: ");
  Serial.println(exh_start);
  Serial.print("Exh Stop: ");
  Serial.println(exh_stop);
  Serial.print("LCD Mode: ");
  Serial.println(lcd_mode);
  Serial.print("Log Mode: ");
  Serial.println(log_mode);
  Serial.println("Checksum valid: true");
}