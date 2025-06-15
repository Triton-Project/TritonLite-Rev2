// Triton-LiteRev2 本番用プログラム（大瀬崎試験）
// 2025/06/15
// 作者: Ryusei Kamiyama
//============================================================
// ライブラリ
#include <EEPROM.h>
#include <TimeLib.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <OneWireNg.h>
#include <OneWireNg_BitBang.h>
#include <OneWireNg_Config.h>
#include <OneWireNg_CurrentPlatform.h>
#include <MS5837.h>
#include <RTC_RX8025NB.h>
#include <IRremote.hpp>

//============================================================
// リモコンのボタンアドレス定義
#define IR_CMD_ENTER_SENSING_MODE 0x40
#define IR_CMD_ENTER_IDLE_MODE    0x44

//============================================================
// 設定構造体
struct g_SystemConfig {
  uint32_t  _supplyStartDelayMs  = 30000;  // 供給動作の開始遅延時間 s
  uint32_t  _supplyStopDelayMs   = 6000;   // 供給動作の停止遅延時間 ms
  uint32_t  _exhaustStartDelayMs = 30000;  // 排気動作の開始遅延時間 s
  uint32_t  _exhaustStopDelayMs  = 10000;  // 排気動作の停止遅延時間 ms
  uint8_t   _lcdMode             = 0;      // ＬＣＤのモードセレクト
  uint8_t   _logMode             = 0;      // ログの保存形式
  uint16_t  _diveCount           = 10;     // 潜行回数 回
  uint16_t  _inPressThresh       = 0;      // 内部加圧閾値
} config;

//============================================================
// EEPROM関連の定義
const uint8_t MAX_DATA_LENGTH = 32;  // 必要に応じて調整

//============================================================
// ピン・インターフェース設定
const uint8_t PIN_BTN           = 1;
const uint8_t PIN_CS_SD         = 10;
const uint8_t PIN_ONEWIRE       = 4;
const uint8_t PIN_IN_PRESSURE   = A0;
const uint8_t PIN_LED_GREEN     = 9;
const uint8_t PIN_LED_RED       = 8;
const uint8_t PIN_VALVE_SUPPLY  = 6;
const uint8_t PIN_VALVE_EXHAUST = 5;
const uint8_t PIN_VALVE_PRESS   = 7;
const uint8_t IR_REMOTE_PIN     = 14;

SoftwareSerial gpsSerial(3, 2); // RX=2ピン, TX=3ピン秋月とスイッチサイエンスで逆
TinyGPSPlus gps;
MS5837 DepthSensor;
RTC_RX8025NB rtc;

//============================================================
// センサ変数
float g_tempWater = 0.0;
float g_pressureInternalRaw = 0.0;
float g_pressureInternalVolt = 0.0;
float g_pressureInternalMbar = 0.0;
float g_pressureExternal = 0.0;
float g_depthExternal = 0.0;
float g_tempExternal = 0.0;

//============================================================
// GPSデータ
uint16_t g_gpsAltitude;
uint8_t g_gpsSatellites;
String g_gpsLat, g_gpsLng;

//============================================================
// RTC変数
uint16_t g_rtcYear;
uint8_t g_rtcMonth, g_rtcDay, g_rtcHour, g_rtcMinute, g_rtcSecond;

//============================================================
// 制御状態変数
unsigned long g_timeNowMs, g_timeLastControlMs;
bool g_isValve0Open, g_isValve1Open, g_isValve2Open;
bool g_isControlling;
int8_t g_valveCtrlState;   // 0:V0 Close, 1:V0 Open, 2:V1 Close, 3:V1 Open
int8_t g_movementState;    // 1:上昇, 2:下降, 3:加圧
bool g_isSensingMode = false;

//============================================================
// 各種関数定義（関数本体省略可）

// EEPROM関連の関数
uint8_t handleEEPROMWriteOnSerial();
bool writeEEPROM(String &str);
bool readEEPROM();
uint8_t calculateChecksum(const uint8_t* data, uint8_t length);
void decodeData(const uint8_t* data);

// 赤外線リモコンから遷移状態を更新
bool updateControlStateFromIR();

//============================================================
// 準備処理
//============================================================
void setup() {
  // Serialの開始
  Serial.begin(9600);
  Wire.begin();
  IrReceiver.begin(IR_REMOTE_PIN, true);
  while (!Serial);
  Serial.println("Serial Ready...");

  水圧センサ
  while (!DepthSensor.init()) {
    Serial.println("Init failed!");
    Serial.println("Are SDA/SCL connected correctly?");
    Serial.println("Blue Robotics Bar30: White=SDA, Green=SCL");
    delay(5000);
  }
  DepthSensor.setModel(MS5837::MS5837_30BA);
  DepthSensor.setFluidDensity(997); // なにこれ

  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_VALVE_SUPPLY, OUTPUT);
  pinMode(PIN_VALVE_EXHAUST, OUTPUT);
  pinMode(PIN_VALVE_PRESS, OUTPUT);
  pinMode(PIN_CS_SD, OUTPUT);
  pinMode(PIN_BTN, INPUT);

  if (!SD.begin(PIN_CS_SD)) {
    Serial.println("Card failed, or not present");
    return;
  }

  //EEPROMから読み込んで変数に格納
  bool isReadSucceed = readEEPROM();
  if (!isReadSucceed) Serial.println("EEPROM read failed.");

  g_timeLastControlMs = millis();

  Serial.println("Setup Finished Successfully");
}

//============================================================
// メイン処理ループ
//============================================================
void loop() {
  // 遷移モードの受信　関数にまとめる
  updateControlStateFromIR(); // 未完成！！

  if (g_isSensingMode) { // センシングモードの処理
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
    // 基本センサデータの取得（温度センサ以外）
    // acquireSensorData();
    // // 加圧制御
    // if (((in_prs_pressure * 68.94) + 1013.25) < out_pre_pressure) {
    //   digitalWrite(valve2, HIGH);
    //   V2 = 1;
    //   isControling = 1;
    //   delay(100);
    // } else {
    //   digitalWrite(valve2, LOW);
    //   V2 = 0;
    // }
    // // OneWireを終了してGPSデータを取得
    // getGPSData();
    // // GPS取得後に温度センサデータを取得
    // getTemperatureData();
    // // バルブ制御
    // CtrlValve();
    // // データ記録
    // writeSDcard();
  } else { // 待機モードの処理
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    handleEEPROMWriteOnSerial();
  }
}

//============================================================
// 各種関数
//============================================================

//============================================================
// 赤外線センサから遷移状態を更新
bool updateControlStateFromIR() {
  // 遷移モードの受信
  if (IrReceiver.decode()) {
    // IrReceiver.printIRResultShort(&Serial);
    IrReceiver.resume();
    if (IrReceiver.decodedIRData.address == 0) {
      if (IrReceiver.decodedIRData.command == IR_CMD_ENTER_SENSING_MODE) {
        // 遷移状態をセンシングモードに変更
        g_isSensingMode = true;
      } else if (IrReceiver.decodedIRData.command == IR_CMD_ENTER_IDLE_MODE) {
        // 遷移状態を待機モードに変更
        g_isSensingMode = false;
      }
    }
  }
}

//============================================================
// EEPROM関連

// EEPROM全体の処理
uint8_t handleEEPROMWriteOnSerial() {
  if (Serial.available() > 0) {
    String receivedData = Serial.readStringUntil('\n');
    Serial.println(receivedData);
    uint8_t length = receivedData.length() / 2;

    bool isWriteSucceed = writeEEPROM(receivedData);
    if (!isWriteSucceed) {
      Serial.println("EEPROM write failed.");
      return length;
    }else {
      Serial.println("Data written to EEPROM successfully!");
    }

    //EEPROMから読み込み、変数に保存
    bool isReadSucceed = readEEPROM();
    if (!isReadSucceed) {
      Serial.println("EEPROM read failed.");
      return length;
    }else {
      Serial.println("Data read from EEPROM successfully!");
      return length;
    }
  }
}

// EEPROM 書き込み処理
bool writeEEPROM(String &str) {
  //データ長チェック
  uint8_t length = str.length() / 2;
  if (length > MAX_DATA_LENGTH || length < 3) { // 最低限 header + 1byte + footer
    Serial.println("Data length is invalid");
    return false;
  }

  //データをバイトに変換・格納
  uint8_t data_bytes[length];
  for (uint8_t i = 0; i < length; i++) {
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
  EEPROM.write(1, length);

  for (size_t i = 0; i < length; i++) {
    EEPROM.write(i + 2, data_bytes[i]);
    Serial.print(data_bytes[i], HEX);
    Serial.print(",");
  }
  Serial.println("");
  return true;
}

// EEPROM 読み込み処理
bool readEEPROM() {
  Serial.println("reading from EEPROM ...");
  uint8_t length = EEPROM.read(1);
  uint8_t header = EEPROM.read(2);
  if (header != 0x24) {
    Serial.print("Invalid header: ");
    Serial.println(header, HEX);
    return false;
  }

  uint8_t rawData[length];
  
  for (uint8_t i = 0; i < length; i++) {
    rawData[i] = EEPROM.read(i+2);
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
uint8_t calculateChecksum(const uint8_t* data, uint8_t length) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i <= length; i++) {
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

  uint8_t mode_byte = data[15];
  uint8_t lcd_mode = (mode_byte >> 4) & 0x0F;
  uint8_t log_mode = mode_byte & 0x0F;

  uint16_t divingCount = data[16];
  uint16_t inPressThresh = data[17];

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
  Serial.print("Diving Count: "); Serial.println(divingCount);
  Serial.print("InPressThresh : "); Serial.println(inPressThresh);

  // RTCの時刻データを更新
  rtc.setDateTime(year, month, day, hour, minute, second);
  
  // 機体の設定を更新
  config._supplyStartDelayMs = sup_start;
  config._supplyStopDelayMs = sup_start;
  config._exhaustStartDelayMs = sup_start;
  config._exhaustStopDelayMs = sup_start;

  config._lcdMode = lcd_mode;
  config._logMode = log_mode;
  config._diveCount = divingCount;
  config._inPressThresh = inPressThresh;
}

//============================================================
// EEPROM関連


// void writeSDcard() {
//   if (isControling == true) {
//     writeSDcard_CTRL();
//     isControling = 0;
//     state = 0;
//   }
//   switch case
//     case:1
//     .
//     .
//     .

//   File dataFile = SD.open("data.txt", FILE_WRITE);
//   if (dataFile) {
//     dataFile.println(Sensorlog);
//     dataFile.close();
//     Serial.println(Sensorlog);
//     digitalWrite(GREEN, HIGH);
//     digitalWrite(RED, LOW);
//   } else {
//     digitalWrite(GREEN, LOW);
//     digitalWrite(RED, HIGH);
//     delay(2000);
//   }
// }