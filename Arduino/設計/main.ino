/**
 *
 * 20250607:PCの専用アプリから設定を書き込めるように(EEPROMに保存)
 *
 *
 * @file TritonLite_main.ino * 
 * @brief Triton-LiteRev2大瀬崎試験本番用のプログラム
 * @author Ryusei Kamiyama
 * @date 2025/06/07
 */

//============================================================
// ライブラリインクルード

// EEPROM関連
#include <EEPROM.h>
#include <TimeLib.h>
// 通信関連
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Wire.h>
// SDカード
#include <SD.h>
// GPS
#include <TinyGPS++.h>
// 温度センサ
#include <DallasTemperature.h>
#include <OneWire.h>
#include <OneWireNg.h>
#include <OneWireNg_BitBang.h>
#include <OneWireNg_Config.h>
#include <OneWireNg_CurrentPlatform.h>
// 水圧
#include <MS5837.h>
// RTC
#include <TimeLib.h>
#include <RTC_RX8025NB.h>

//============================================================
// システム設定構造体 (PCアプリから書き込み可能)
struct SystemConfig {
  uint32_t  _supplyStartDelayMs  = 30000;  // 供給動作の開始遅延時間   デフォルト 30000ms
  uint32_t  _supplyStopDelayMs   = 6000;   // 供給動作の停止遅延時間   デフォルト 6000ms
  uint32_t  _exhaustStartDelayMs = 30000;  // 排気動作の開始遅延時間   デフォルト 30000ms
  uint32_t  _exhaustStopDelayMs  = 10000;  // 排気動作の停止遅延時間   デフォルト 10000ms
  uint8_t   _lcdMode             = 0;      // ＬＣＤのモードセレクト   デフォルト モード0
  uint8_t   _logMode             = 0;      // ログの保存形式          デフォルト モード0
  uint16_t  _diveCount           = 10;     // 潜行回数                デフォルト 10回
  uint16_t  _inPressThresh       = 0;      // 内部加圧閾値            デフォルト 0
} config;

//============================================================
// 各種設定

// EEPROM関連
const size_t MAX_DATA_LENGTH = 32;  // Serialから受け取る最大文字数
const uint8_t BTN_PIN1 = 1;
const uint8_t BTN_PIN2 = 2;
// 電磁弁開閉時間指定
#define SUP_START_TIME (config.sup_start_time)
#define SUP_STOP_TIME (config.sup_stop_time)
#define EXH_START_TIME (config.exh_start_time)
#define EXH_STOP_TIME (config.exh_stop_time)
//SDカードシールド
const uint8_t CHIP_SELECT = 10;
//GPS
SoftwareSerial mygps(3, 2);  // RX=2ピン, TX=3ピン秋月とスイッチサイエンスで逆
TinyGPSPlus gps;
//温度センサ
#define ONE_WIRE_BUS 4
#define SENSOR_BIT 8
//水圧センサ
MS5837 DepthSensor;
//気圧センサ
const uint8_t IN_PRS_PIN = A0;
//RTC
RTC_RX8025NB rtc;
//LED
const uint8_t GREEN_PIN = 8;
const uint8_t RED_PIN = 9;
// 電磁弁
const uint8_t VALVE_0 = 6;  // 注入バルブ
const uint8_t VALVE_1 = 5;  // 排気バルブ
const uint8_t VALVE_2 = 7;  // 加圧バルブ

//============================================================
// 変数定義

//GPS
uint16_t gpsAltitude;
uint8_t gpsSatellites;
String lat, lng; //  <------------------------------------------------------??
//GPS時刻修正用
uint8_t daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
//水温
float temp = 0;
//気圧センサ
float inPrsRawdata = 0;
float inPrsVoltage = 0;
float inPrsPressure = 0;
//水圧センサ
float outPrePressure = 0;
float outPreDepth = 0;
float outPreTmp = 0;
//RTC
uint16_t  rtc_year;
uint8_t   rtc_month, rtc_day, rtc_hour, rtc_minute, rtc_second;
//millis
unsigned long miliTime, lastCtrl;
// 電磁弁開閉状況,何かコントロール中か
bool isV0_Open, isV1_Open, isV2_Open, isControling;
int8_t ctrlState;
/*
*ctrlStateの詳細
*状態
* 0 V0 CLOSE
* 1 V0 OPEN
* 2 V1 CLOSE
* 3 V1 OPEN
*/
int8_t movementState;
// 状態(上昇=1,下降=2,加圧=3)

//============================================================
// 関数定義

void CtrlValve();
void acquireSensorData();
void getTemperatureData();
void getGPSData();
bool isLeapYear(int year);
void correctTime();
void writeSDcard();

bool writeEEPROM(String &str);
bool readEEPROM(const size_t length);
uint8_t calculateChecksum(const uint8_t* data, size_t length);
void decodeData(const uint8_t* data);
size_t handleEEPROMWriteOnSerial();

//void writeSDcard_CTRL();

//============================================================
// 各種関数

//------------------------------------------------------------
// データ取得系
// 基本センサデータ取得（OneWire以外）
void acquireSensorData() {
  // 水圧センサデータの取得
  DepthSensor.read();
  out_pre_pressure = DepthSensor.pressure();
  out_pre_depth = DepthSensor.depth();  //mber
  out_pre_tmp = DepthSensor.temperature();

  // 気圧センサデータの取得
  in_prs_rawdata = analogRead(in_prs_pin);
  in_prs_voltage = in_prs_rawdata / 1024 * 5 - 0.25;
  in_prs_pressure = in_prs_voltage / 4.5 * 30;  //PSI　mbarに変換する場合は68.94をかける
}
// OneWire温度センサデータ取得
void getTemperatureData() {
  // OneWireを初期化
  OneWire oneWire(ONE_WIRE_BUS);
  DallasTemperature sensors(&oneWire);
  sensors.begin();
  
  // 温度センサデータの取得
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
}
// GPSデータ取得
void getGPSData() {
  // GPSシリアルを開始
  mygps.begin(9600);
  
  // GPSデータの取得（一定時間または有効なデータを受信するまで）
  unsigned long startTime = millis();
  bool timeUpdated = false;
  
  while ((millis() - startTime) < 1000) { // 1秒間データを読み取り
    while (mygps.available()) {
      if (gps.encode(mygps.read())) {
        // 位置情報の更新
        if (gps.location.isUpdated()) {
          lat = String(gps.location.lat(), 6);
          lng = String(gps.location.lng(), 6);
          altitude = gps.altitude.meters();
          gpssatellites = gps.satellites.value();
        }
        
        // 時刻情報の更新
        if (gps.time.isValid() && gps.date.isValid()) {
          // RTCの時刻を補正
          correctTime();
          timeUpdated = true;
        }
      }
    }
    
    // 時刻が更新されたら終了
    if (timeUpdated) {
      break;
    }
  }
  
  // デバッグ出力
  Serial.print("GPS Time: ");
  if (gps.time.isValid()) {
    Serial.print(gps.time.hour());
    Serial.print(":");
    Serial.print(gps.time.minute());
    Serial.print(":");
    Serial.println(gps.time.second());
  } else {
    Serial.println("Invalid");
  }
  
  // GPSシリアルを停止
  mygps.end();
}
// 閏年判定
bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}
//RTCの時刻をGPSで補正
void correctTime() {
  miliTime = millis();
  
  if (gps.time.isValid() && gps.date.isValid()) {
    //時刻の修正（UTC -> JST）
    int gps_year = gps.date.year();
    int gps_month = gps.date.month();
    int gps_day = gps.date.day();
    int gps_hour = gps.time.hour() + 9;  // UTC+9
    int gps_minute = gps.time.minute();
    int gps_second = gps.time.second();

    // デバッグ出力
    Serial.print("GPS DateTime: ");
    Serial.print(gps_year);
    Serial.print("/");
    Serial.print(gps_month);
    Serial.print("/");
    Serial.print(gps_day);
    Serial.print(" ");
    Serial.print(gps_hour);
    Serial.print(":");
    Serial.print(gps_minute);
    Serial.print(":");
    Serial.println(gps_second);

    //時刻が24を超える場合の処理
    if (gps_hour >= 24) {
      gps_hour -= 24;
      gps_day += 1;
    }

    //月の日数を考慮して日付を修正
    if (gps_day > daysInMonth[gps_month - 1]) {
      if (!(gps_month == 2 && gps_day == 29 && isLeapYear(gps_year))) {
        gps_day = 1;
        gps_month += 1;
      }
    }

    //月が12を超える場合の処理
    if (gps_month > 12) {
      gps_month = 1;
      gps_year += 1;
    }

    //GPSから取得した時刻をRTCに適用
    rtc.setDateTime(gps_year, gps_month, gps_day, gps_hour, gps_minute, gps_second);
    
    // RTCから読み取って確認
    tmElements_t tm = rtc.read();
    rtc_year = tmYearToCalendar(tm.Year);
    rtc_month = tm.Month;
    rtc_day = tm.Day;
    rtc_hour = tm.Hour;
    rtc_minute = tm.Minute;
    rtc_second = tm.Second;
    
    // デバッグ出力G
    Serial.print("RTC Set: ");
    Serial.print(rtc_year);
    Serial.print("/");
    Serial.print(rtc_month);
    Serial.print("/");
    Serial.print(rtc_day);
    Serial.print(" ");
    Serial.print(rtc_hour);
    Serial.print(":");
    Serial.print(rtc_minute);
    Serial.print(":");
    Serial.println(rtc_second);
  }
}
// センサデータ書き込み
void writeSDcard() {
  String Sensorlog = "";
  Sensorlog += miliTime;
  Sensorlog += ",";
  Sensorlog += rtc_year;
  Sensorlog += "/";
  Sensorlog += rtc_month;
  Sensorlog += "/";
  Sensorlog += rtc_day;
  Sensorlog += "-";
  Sensorlog += rtc_hour;
  Sensorlog += ":";
  Sensorlog += rtc_minute;
  Sensorlog += ":";
  Sensorlog += rtc_second;
  Sensorlog += ",DATA,";
  Sensorlog += "LAT,";
  Sensorlog += lat;
  Sensorlog += ",LNG,";
  Sensorlog += lng;
  Sensorlog += ",SATNUM,";
  Sensorlog += gpssatellites;
  Sensorlog += ",PIN_RAW,";
  Sensorlog += in_prs_rawdata;
  Sensorlog += ",PIN_PRS,";
  Sensorlog += in_prs_pressure;
  Sensorlog += ",POUT_PRS,";
  Sensorlog += out_pre_pressure;
  Sensorlog += ",POUT_DEPTH,";
  Sensorlog += out_pre_depth;
  Sensorlog += ",POUT_TMP,";
  Sensorlog += out_pre_tmp;
  Sensorlog += ",TMP,";
  Sensorlog += temperature;
  Sensorlog += ",";

  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println(Sensorlog);
    dataFile.close();
    Serial.println(Sensorlog);
    digitalWrite(GREEN, HIGH);
    digitalWrite(RED, LOW);
  } else {
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, HIGH);
    delay(2000);
  }
}

//------------------------------------------------------------
// 制御系

// 浮沈用電磁弁制御
void CtrlValve() {
  switch (Ctrl_state) {
    case 0:
      if ((miliTime - last_ctrl) > SUP_START_TIME) {
        digitalWrite(valve0, HIGH);
        V0 = 1;
        isControling = 1;
        Ctrl_state = 1;
        state = 1;  // 浮上中
        last_ctrl = millis();
      }
      break;
    case 1:
      if ((miliTime - last_ctrl) > SUP_STOP_TIME) {
        digitalWrite(valve0, LOW);
        V0 = 0;
        isControling = 1;
        Ctrl_state = 2;
        state = 1;  // 浮上中
        last_ctrl = millis();
      }
      break;
    case 2:
      if ((miliTime - last_ctrl) > EXH_START_TIME) {
        digitalWrite(valve1, HIGH);
        V1 = 1;
        isControling = 1;
        Ctrl_state = 3;
        state = 2;  // 沈降中
        last_ctrl = millis();
      }
      break;
    case 3:
      if ((miliTime - last_ctrl) > EXH_STOP_TIME) {
        digitalWrite(valve1, LOW);
        V1 = 0;
        isControling = 1;
        Ctrl_state = 0;
        state = 2;  // 沈降中
        last_ctrl = millis();
      }
      break;
  }
}

// 電磁弁状況書き込み

/* void writeSDcard_CTRL() {
  String CTRLlog = "";
  CTRLlog += miliTime;
  CTRLlog += ",";
  CTRLlog += rtc_year;
  CTRLlog += "/";
  CTRLlog += rtc_month;
  CTRLlog += "/";
  CTRLlog += rtc_day;
  CTRLlog += "-";
  CTRLlog += rtc_hour;
  CTRLlog += ":";
  CTRLlog += rtc_minute;
  CTRLlog += ":";
  CTRLlog += rtc_second;
  CTRLlog += ",CTRL,";
  CTRLlog += "MSG,";

  if (state == 1) {
    CTRLlog += "UP";
  } else if (state == 2) {
    CTRLlog += "DOWN";
  } else if (state == 3) {
    CTRLlog += "PRESSURE";
  } else {
    CTRLlog += "UNDEFIND";
  }

  CTRLlog += ",V0,";
  CTRLlog += V0;
  CTRLlog += ",V1,";
  CTRLlog += V1;
  CTRLlog += ",V2,";
  CTRLlog += V2;

  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println(CTRLlog);
    dataFile.close();
    Serial.println(CTRLlog);
    digitalWrite(GREEN, HIGH);
    digitalWrite(RED, LOW);
  } else {
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, HIGH);
    delay(2000);
  }
}
*/

//------------------------------------------------------------
// EEPROM関連

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

  for (size_t i = 0; i < length; i++) {
    EEPROM.write(i + 1, data_bytes[i]);
    Serial.print(data_bytes[i], HEX);
    Serial.print(",");
  }
  Serial.println("");
  return true;
}
// EEPROM 読み込み処理
bool readEEPROM(const uint8_t length) {
  Serial.println("reading from EEPROM ...");

  uint8_t header = EEPROM.read(1);
  if (header != 0x24) {
    Serial.print("Invalid header: ");
    Serial.println(header, HEX);
    return false;
  }

  uint8_t rawData[length];
  
  for (uint8_t i = 0; i < length; i++) {
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
  g_floatTime[0] = sup_start;
  g_floatTime[1] = sup_stop;
  g_floatTime[2] = exh_start;
  g_floatTime[3] = exh_stop;

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
// EEPROM全体の処理
size_t handleEEPROMWriteOnSerial() {
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

//============================================================
// setup

void setup() {
  // 準備処理
  Serial.begin(9600);
  while (!Serial);
  Serial.println("Serial Ready...");

  //SDカードシールド
  pinMode(SS, OUTPUT);
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    return;
  }
  Serial.println("SD ok");

  // 設定の読み込み
  loadConfig();

  Wire.begin();

  //水圧センサ
  while (!DepthSensor.init()) {
    Serial.println("Init failed!");
    Serial.println("Are SDA/SCL connected correctly?");
    Serial.println("Blue Robotics Bar30: White=SDA, Green=SCL");
    delay(5000);
  }
  DepthSensor.setModel(MS5837::MS5837_30BA);
  DepthSensor.setFluidDensity(997);
  //RTC
  rtc.setDateTime(2024, 1, 27, 20, 27, 0);

  pinMode(valve0, OUTPUT);
  pinMode(valve1, OUTPUT);
  pinMode(valve2, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(RED, OUTPUT);

  last_ctrl = millis();
  Serial.println("Setup Done");


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
}
