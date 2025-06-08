#include <EEPROM.h>
#include <TimeLib.h>

const size_t MAX_DATA_LENGTH = 32;  // 必要に応じて調整
const uint8_t btn1 = 1;
const uint8_t btn2 = 2;

uint16_t g_floatTime[4];
uint8_t g_lcdMode;
uint8_t g_logMode;

//EEPROM関連の関数

// EEPROM 書き込み処理
bool writeEEPROM(String &str) {
  //データ長チェック
  size_t length = str.length() / 2;
  if (length > MAX_DATA_LENGTH || length < 3) { // 最低限 header + 1byte + footer
    Serial.println("Data length is invalid");
    return false;
  }

  //データをバイトに変換・格納
  uint8_t data_bytes[length];
  for (size_t i = 0; i < length; i++) {
    sscanf(str.c_str() + 2 * i, "%2hhx", &data_bytes[i]);
    Serial.print(data_bytes[i], HEX);
    Serial.print(",");
  }
  Serial.println("");

  //ヘッダーとフッターのチェック
  uint8_t header = data_bytes[0];
  uint8_t footer = data_bytes[length-1];
  if (header != 0x24) {
    Serial.print("Invalid header: ");
    Serial.println(header, HEX);
    return false;
  }
  if (footer != 0x3B) {
    Serial.println("Footer not found");
    return false;
  }

  //チェックサム
  uint8_t expectedChecksum = data_bytes[length - 2];  // フッターの1つ前がチェックサム
  uint8_t calculatedChecksum = calculateChecksum(&data_bytes[0], length-3); // チェックサム, フッター除く

  if (calculatedChecksum != expectedChecksum) {
    Serial.print("Checksum mismatch. Expected: ");
    Serial.print(expectedChecksum, HEX);
    Serial.print(" Calculated: ");
    Serial.println(calculatedChecksum, HEX);
    return false;
  }

  //デコード
  decodeData(data_bytes);

  //EEPROM書き込み
  EEPROM.write(0, 0xAA); // ダミー書き込み

  for (size_t i = 0; i < length; i++) {
    EEPROM.write(i + 1, data_bytes[i]);
    Serial.print(data_bytes[i], HEX);
    Serial.print(",");
  }
  Serial.println("");
  return true;
}

// EEPROM 読み込み処理
bool readEEPROM(const size_t length) {
  Serial.println("reading from EEPROM ...");

  uint8_t header = EEPROM.read(1);
  if (header != 0x24) {
    Serial.print("Invalid header: ");
    Serial.println(header, HEX);
    return false;
  }

  uint8_t rawData[length];
  
  for (size_t i = 0; i < length; i++) {
    rawData[i] = EEPROM.read(i+1);
    Serial.print(rawData[i], HEX);
    Serial.print(",");
  }
  Serial.println("");

  if (rawData[length-1] != 0x3B) {
    Serial.println("Footer not found");
    return false;
  }

  uint8_t expectedChecksum = rawData[length - 2];  // フッターの1つ前がチェックサム
  uint8_t calculatedChecksum = calculateChecksum(&rawData[0], length-3); // header除く, チェックサム除く

  if (calculatedChecksum != expectedChecksum) {
    Serial.print("Checksum mismatch. Expected: ");
    Serial.print(expectedChecksum, HEX);
    Serial.print(" Calculated: ");
    Serial.println(calculatedChecksum, HEX);
    return false;
  }

  // 読み取ったデータをデコード
  decodeData(rawData);
  return true;
}

// チェックサム計算
uint8_t calculateChecksum(const uint8_t* data, size_t length) {
  uint8_t sum = 0;
  for (size_t i = 0; i <= length; i++) {
    sum += data[i];
    // Serial.println(data[i], HEX);
  }
  return sum & 0xFF;
}

// デコード処理
void decodeData(const uint8_t* data) {
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
  g_floatTime[0] = sup_start;
  g_floatTime[1] = sup_stop;
  g_floatTime[2] = exh_start;
  g_floatTime[3] = exh_stop;

  uint8_t mode_byte = data[15];
  uint8_t lcd_mode = (mode_byte >> 4) & 0x0F;
  g_lcdMode = lcd_mode;

  uint8_t log_mode = mode_byte & 0x0F;
  g_logMode = log_mode;

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

size_t handleEEPROMWriteOnSerial() {
  if (Serial.available() > 0) {
    String receivedData = Serial.readStringUntil('\n');
    Serial.println(receivedData);
    size_t length = receivedData.length() / 2;

    bool isWriteSucceed = writeEEPROM(receivedData);
    if (!isWriteSucceed) {
      Serial.println("EEPROM write failed.");
      return length;
    }else {
      Serial.println("Data written to EEPROM successfully!");
    }

    //EEPROMから読み込み、変数に保存
    bool isReadSucceed = readEEPROM(length);
    if (!isReadSucceed) {
      Serial.println("EEPROM read failed.");
      return length;
    }else {
      Serial.println("Data read from EEPROM successfully!");
      return length;
    }
  }
}

void setup() {
  // 準備処理
  Serial.begin(9600);
  while (!Serial);
  Serial.println("Serial Ready...");

  // PCアプリからEEPROMに書き込み処理
  bool isSensingMode = false;
  size_t length;
  while(!isSensingMode) { //待機モード　ここではPCから書き込み可
    if(digitalRead(btn1)) isSensingMode = true;
    length = handleEEPROMWriteOnSerial();
  }

  //EEPROMから読み込んで変数に格納
  bool isReadSucceed = readEEPROM(length);
  if (!isReadSucceed) {
    Serial.println("EEPROM read failed.");
  }else {
    Serial.println("Data read from EEPROM successfully!");
  }
  Serial.println("turning to sensingMode ...");
}

void loop() {
  // put your main code here, to run repeatedly:
  String s = "Loaded Modes: \n";
  s += "lcd: ";
  s += g_lcdMode;
  s += ", log: ";
  s += g_logMode;
  s += ",";
  Serial.println(s);
  delay(1000);
}
