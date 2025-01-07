//EEPROMに日時データを書き込み、読み出してデコードするサンプルコード

#include <EEPROM.h>
#include <TimeLib.h>

// Function to decode the hex string to tmElements_t
void decodeDateTime(const char* hexStr, tmElements_t &tm) {
  tm.Year = strtol(String(hexStr).substring(0, 2).c_str(), NULL, 16) + 2000 - 1970; // Offset from 1970
  tm.Month = strtol(String(hexStr).substring(2, 4).c_str(), NULL, 16);
  tm.Day = strtol(String(hexStr).substring(4, 6).c_str(), NULL, 16);
  tm.Hour = strtol(String(hexStr).substring(6, 8).c_str(), NULL, 16);
  tm.Minute = strtol(String(hexStr).substring(8, 10).c_str(), NULL, 16);
  tm.Second = strtol(String(hexStr).substring(10, 12).c_str(), NULL, 16);
}

void setup() {
  Serial.begin(9600);

  // ダミーデータを書き込む
  EEPROM.write(0, 0xaa);
  Serial.println("Writing dummy data to EEPROM: 0 -> 0xAA");

  // データを書き込む
  const char data[] = "190107150e35";
  for (int i = 0; i < 12; i += 2) {
    char byteStr[3] = {data[i], data[i + 1], '\0'};
    byte byteValue = strtol(byteStr, NULL, 16);
    EEPROM.write((i / 2) + 1, byteValue);
    Serial.print("Writing to EEPROM: ");
    Serial.print((i / 2) + 1);
    Serial.print(" -> ");
    Serial.println(byteValue, HEX);
  }

  // データを読み出す
  char readData[13];
  for (int i = 0; i < 6; i++) {
    byte byteValue = EEPROM.read(i + 1);
    sprintf(&readData[i * 2], "%02x", byteValue);
    Serial.print("Reading from EEPROM: ");
    Serial.print(i + 1);
    Serial.print(" -> ");
    Serial.println(byteValue, HEX);
  }
  readData[12] = '\0'; // Null-terminate the string

  // 読み出したデータをデコードする
  tmElements_t tm;
  decodeDateTime(readData, tm);

  // デコードした日時を表示する
  Serial.print("Decoded DateTime: ");
  Serial.print(tmYearToCalendar(tm.Year)); // Year offset from 1970
  Serial.print("-");
  Serial.print(tm.Month);
  Serial.print("-");
  Serial.print(tm.Day);
  Serial.print(" ");
  Serial.print(tm.Hour);
  Serial.print(":");
  Serial.print(tm.Minute);
  Serial.print(":");
  Serial.print(tm.Second);
  Serial.println();
}

void loop() {
  // ループ内で特に何もしない
}