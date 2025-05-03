#include <EEPROM.h>
#include <TimeLib.h>

const size_t MAX_DATA_LENGTH = 32;  // 必要に応じて調整

void setup() {
  Serial.begin(9600);
  while (!Serial) {}
  Serial.println("Serial Ready...");
}

void loop() {
  if (Serial.available() > 0) {
    String receivedData = Serial.readStringUntil('\n');
    Serial.println(receivedData);

    size_t length = receivedData.length() / 2;
    if (length > MAX_DATA_LENGTH) {
      Serial.println("Data too long");
      return;
    }

    uint8_t data_bytes[length];
    for (size_t i = 0; i < length; i++) {
      sscanf(receivedData.c_str() + 2 * i, "%2hhx", &data_bytes[i]);
      Serial.print(data_bytes[i], HEX);
      Serial.print(",");
    }
    Serial.println("");

    // EEPROM 書き込み
    bool success = writeEEPROM(data_bytes, length);
    if (!success) {
      Serial.println("EEPROM write failed.");
      return;
    }

    // EEPROM 読み出しと検証
    size_t outLength;
    uint8_t* read_data = readEEPROM(&outLength);
    if (read_data == nullptr) {
      Serial.println("EEPROM read failed.");
      return;
    }

    decode_data(read_data);  // headerスキップ
    free(read_data);
  }
}

// EEPROM 書き込み処理
bool writeEEPROM(const uint8_t* data, size_t length) {
  if (length < 3) return false;  // 最低限 header + 1byte + footer

  EEPROM.write(0, 0xAA); // ダミー書き込み

  for (size_t i = 0; i < length; i++) {
    EEPROM.write(i + 1, data[i]);
    Serial.print(data[i], HEX);
    Serial.print(",");
  }
  Serial.println("");
  return true;
}

// EEPROM 読み込み処理
uint8_t* readEEPROM(size_t* outLength) {
  *outLength = 0;

  uint8_t header = EEPROM.read(1);
  if (header != 0x24) {
    Serial.print("Invalid header: ");
    Serial.println(header, HEX);
    return nullptr;
  }

  uint8_t rawData[MAX_DATA_LENGTH];
  size_t i = 1;
  for (; i < MAX_DATA_LENGTH; i++) {
    rawData[i] = EEPROM.read(i);
    Serial.print(rawData[i], HEX);
    Serial.print(",");
    if (rawData[i] == 0x3B) {  // フッターを見つけたら終了
      break;
    }
  }
  Serial.println("");

  if (rawData[i] != 0x3B) {
    Serial.println("Footer not found");
    return nullptr;
  }

  size_t length = i + 1;  // データ長（header〜footer含む）

  uint8_t expectedChecksum = rawData[length - 2];  // フッターの1つ前がチェックサム
  uint8_t calculatedChecksum = calculate_checksum(&rawData[1], length-3); // header除く, チェックサム除く

  if (calculatedChecksum != expectedChecksum) {
    Serial.print("Checksum mismatch. Expected: ");
    Serial.print(expectedChecksum, HEX);
    Serial.print(" Calculated: ");
    Serial.println(calculatedChecksum, HEX);
    return nullptr;
  }

  // メモリ確保してコピー
  uint8_t* dataCopy = (uint8_t*)malloc(length);
  memcpy(dataCopy, &rawData[1], length);  // EEPROM[1]から開始
  *outLength = length;
  Serial.println("EEPROM read and validated successfully");
  return dataCopy;
}

// チェックサム計算
uint8_t calculate_checksum(const uint8_t* data, size_t length) {
  uint8_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum & 0xFF;
}

// デコード処理
void decode_data(const uint8_t* data) {
  uint8_t year_offset = data[1];
  uint16_t year = 2000 + year_offset;
  uint8_t month = data[2];
  uint8_t day = data[3];
  uint8_t hour = data[4];
  uint8_t minute = data[5];
  uint8_t second = data[6];

  uint16_t sup_start = (data[7] << 8) | data[8];
  uint16_t sup_stop = (data[9] << 8) | data[10];
  uint16_t exh_start = (data[11] << 8) | data[12];
  uint16_t exh_stop = (data[13] << 8) | data[14];

  uint8_t mode_byte = data[15];
  uint8_t lcd_mode = (mode_byte >> 4) & 0x0F;
  uint8_t log_mode = mode_byte & 0x0F;

  // 出力
  Serial.println("Decoded Data:");
  Serial.print("Year: "); Serial.println(year);
  Serial.print("Month: "); Serial.println(month);
  Serial.print("Day: "); Serial.println(day);
  Serial.print("Hour: "); Serial.println(hour);
  Serial.print("Minute: "); Serial.println(minute);
  Serial.print("Second: "); Serial.println(second);
  Serial.print("Sup Start: "); Serial.println(sup_start);
  Serial.print("Sup Stop: "); Serial.println(sup_stop);
  Serial.print("Exh Start: "); Serial.println(exh_start);
  Serial.print("Exh Stop: "); Serial.println(exh_stop);
  Serial.print("LCD Mode: "); Serial.println(lcd_mode);
  Serial.print("Log Mode: "); Serial.println(log_mode);
}
