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
#include <LiquidCrystal_I2C.h>

//============================================================
// リモコンのボタンアドレス定義
#define IR_CMD_ENTER_SENSING_MODE 0x40
#define IR_CMD_ENTER_IDLE_MODE    0x44

// 設定構造体
struct g_SystemConfig {
  uint32_t  _supplyStartDelayMs  = 30*1000;  // 供給動作の開始遅延時間 ms
  uint32_t  _supplyStopDelayMs   = 6000;   // 供給動作の停止遅延時間 ms
  uint32_t  _exhaustStartDelayMs = 30*1000;  // 排気動作の開始遅延時間 ms
  uint32_t  _exhaustStopDelayMs  = 3000;  // 排気動作の停止遅延時間 ms
  uint8_t   _lcdMode             = 0;      // ＬＣＤのモードセレクト
  uint8_t   _logMode             = 0;      // ログの保存形式
  uint16_t  _diveCount           = 10;     // 潜行回数 回
  uint16_t  _inPressThresh       = 0;      // 内部加圧閾値
} config;

//============================================================
// EEPROM関連
const uint8_t MAX_DATA_LENGTH = 32;  // 必要に応じて調整
// SDカード関連
String g_dataFileName = "DATAFILE";
//============================================================
// ピン・インターフェース設定
const uint8_t PIN_BTN           = 1;
const uint8_t PIN_CS_SD         = 10;
const uint8_t PIN_ONEWIRE       = 4;
const uint8_t PIN_ONEWIRE_BIT   = 4;
const uint8_t PIN_IN_PRESSURE   = A3;
const uint8_t PIN_LED_GREEN     = 9;
const uint8_t PIN_LED_RED       = 8;
const uint8_t PIN_VALVE1_SUPPLY  = 7;
const uint8_t PIN_VALVE2_EXHAUST = 6;
const uint8_t PIN_VALVE3_PRESS   = 5;
const uint8_t PIN_IR_REMOTE     = 14;

SoftwareSerial gpsSerial(3, 2); // RX=2ピン, TX=3ピン秋月とスイッチサイエンスで逆
TinyGPSPlus gps;
MS5837 DepthSensor;
RTC_RX8025NB rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

//============================================================
// センサ変数
float g_noramlTemp              = 0.0;

float g_prsInternalRaw          = 0.0;
float g_prsInternalMbar         = 0.0;

float g_prsExternal             = 0.0;
float g_prsExternalDepth        = 0.0;
float g_prsExternalTmp          = 0.0;

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
bool g_isValve1SupplyOpen, g_isValve2ExhaustOpen, g_isValve3PressOpen;
bool g_isControllingValve;
int8_t g_valveCtrlState = 0;   // 0:V1SUP CLOSE, 1:V2EXH OPEN 2:V2EXH CLOSE 3:V1SUP OPEN
int8_t g_movementState;    // 1:浮上中, 2:沈降中, 3:加圧
unsigned int g_divedCount = 0;
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

// センシング関連
void acquireSensorData();
void getTemperatureData();
void getGPSData();
bool isLeapYear(int year);
void correctTime();

// 制御関連
void CtrlValve();

// SDカード関連
bool handleSDcard();
bool writeSDcard_CTRL();
void mkStringSDcardMode0(String &Sensorlog);
void mkStringSDcardMode1(String &Sensorlog);
void mkStringSDcardMode2(String &Sensorlog);

//LCD関連
void handleLcdDisp();
void dispLcd_CTRL();
void dispLcdError(String errorCode);
bool i2CAddrTest(uint8_t addr);

//============================================================
// 準備処理
//============================================================
void setup() {
  // Serialの開始
  Serial.begin(9600);
  Wire.begin();
  IrReceiver.begin(PIN_IR_REMOTE, true);
  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();                // LCDを初期化
  lcd.backlight();           // バックライトを点灯

  Serial.println("Serial Ready...");

  // 水圧センサ
  while (!DepthSensor.init()) {
    Serial.println("Init failed!");
    Serial.println("Are SDA/SCL connected correctly?");
    Serial.println("Blue Robotics Bar30: White=SDA, Green=SCL");
    dispLcdError("Depth FAILED!");
  }
  DepthSensor.setModel(MS5837::MS5837_30BA);
  DepthSensor.setFluidDensity(997); // NANIKORE

  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_VALVE1_SUPPLY, OUTPUT);
  pinMode(PIN_VALVE2_EXHAUST, OUTPUT);
  pinMode(PIN_VALVE3_PRESS, OUTPUT);
  pinMode(PIN_CS_SD, OUTPUT);

  pinMode(PIN_BTN, INPUT);

  if (!SD.begin(PIN_CS_SD)) {
    Serial.println("Card failed, or not present");
    lcd.setCursor(0,0);
    lcd.print("SD CARD FAILED");
    lcd.setCursor(0,1);
    lcd.print("OR NOT PRESENT");
    return;
  }

  //EEPROMから読み込んで変数に格納
  bool isReadSucceed = readEEPROM();
  if (!isReadSucceed) Serial.println("EEPROM read failed.");

  g_timeLastControlMs = millis();

  lcd.setCursor(0,0);
  lcd.print("Setup Finished!");
  lcd.setCursor(0,1);
  lcd.print("LOG: ");
  lcd.print(config._logMode);
  lcd.print("LCD: ");
  lcd.print(config._lcdMode);

  Serial.println("Setup Finished Successfully");
  delay(3000); // LCD表示を確認するための猶予時間
}

//============================================================
// メイン処理ループ
//============================================================
void loop() {
  g_timeNowMs = millis(); // 時刻の更新
  // 遷移モードの受信
  updateControlStateFromIR();
  if (g_isSensingMode) { // センシングモードの処理
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, LOW);

    // 基本センサデータの取得（温度センサ以外）
    acquireSensorData();
    // 加圧制御
    if (((g_prsInternalMbar * 68.94) + 1013.25) < g_prsExternal) {
      digitalWrite(PIN_VALVE3_PRESS, HIGH);
      g_isValve3PressOpen = true;
      g_isControllingValve = true;
      g_movementState = 3;
      delay(100);
    } else {
      digitalWrite(PIN_VALVE3_PRESS, LOW);
      g_isValve3PressOpen = false;
    }
    // OneWireを終了してGPSデータを取得
    getGPSData();
    // GPS取得後に温度センサデータを取得
    getTemperatureData();
    // バルブ制御
    CtrlValve();
    // データ表示と記録
    handleLcdDisp();
    handleSDcard();
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
      dispLcdError("EEPROM READ FAILED!");
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
  config._supplyStartDelayMs = sup_start*1000; // msに変換
  config._supplyStopDelayMs = sup_stop;
  config._exhaustStartDelayMs = exh_start*1000; // 同上
  config._exhaustStopDelayMs = exh_stop;

  config._lcdMode = lcd_mode;
  config._logMode = log_mode;
  config._diveCount = divingCount;
  config._inPressThresh = inPressThresh;

  // SDカードに保存するファイル名を作成
  g_dataFileName = String(month);
  g_dataFileName += day;
  g_dataFileName += "_";
  g_dataFileName += hour;
  g_dataFileName += ".csv";
}

//============================================================
// センシング関連
// 基本センサデータ取得（OneWire以外）
void acquireSensorData() {
  // 水圧センサデータの取得
  DepthSensor.read();
  g_prsExternal = DepthSensor.pressure();
  g_prsExternalDepth = DepthSensor.depth();  //mber
  g_prsExternalTmp = DepthSensor.temperature();

  // 気圧センサデータの取得
  g_prsInternalRaw = analogRead(PIN_IN_PRESSURE);
  float prsInternalVolt = g_prsInternalRaw / 1024 * 5 - 0.25;
  g_prsInternalMbar = prsInternalVolt / 4.5 * 30;  //PSI　mbarに変換する場合は68.94をかける

  getTemperatureData();

  getGPSData();
}

// OneWire温度センサデータ取得
void getTemperatureData() {
  // OneWireを初期化
  OneWire oneWire(PIN_ONEWIRE);
  DallasTemperature sensors(&oneWire);
  sensors.begin();
  
  // 温度センサデータの取得
  sensors.requestTemperatures();
  g_noramlTemp = sensors.getTempCByIndex(0);
}

// GPSデータ取得
void getGPSData() {
  // GPSシリアルを開始
  gpsSerial.begin(9600);
  
  // GPSデータの取得（一定時間または有効なデータを受信するまで）
  unsigned long startTime = millis();
  bool timeUpdated = false;
  
  while ((millis() - startTime) < 1000) { // 1秒間データを読み取り
    while (gpsSerial.available()) {
      if (gps.encode(gpsSerial.read())) {
        // 位置情報の更新
        if (gps.location.isUpdated()) {
          g_gpsLat = String(gps.location.lat(), 6);
          g_gpsLng = String(gps.location.lng(), 6);
          g_gpsAltitude = gps.altitude.meters();
          g_gpsSatellites = gps.satellites.value();
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
  // Serial.print("GPS Time: ");
  if (gps.time.isValid()) {
    // Serial.print(gps.time.hour());
    // Serial.print(":");
    // Serial.print(gps.time.minute());
    // Serial.print(":");
    // Serial.println(gps.time.second());
  } else {
    Serial.println("GPS Time: Invalid!");
  }
  
  // GPSシリアルを停止
  gpsSerial.end();
}

// 閏年判定
bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

//RTCの時刻をGPSで補正
void correctTime() {
  // miliTime = millis();
  
  if (gps.time.isValid() && gps.date.isValid()) {
    //時刻の修正（UTC -> JST）
    int gps_year = gps.date.year();
    int gps_month = gps.date.month();
    int gps_day = gps.date.day();
    int gps_hour = gps.time.hour() + 9;  // UTC+9
    int gps_minute = gps.time.minute();
    int gps_second = gps.time.second();

    // デバッグ出力
    // Serial.print("GPS DateTime: ");
    // Serial.print(gps_year);
    // Serial.print("/");
    // Serial.print(gps_month);
    // Serial.print("/");
    // Serial.print(gps_day);
    // Serial.print(" ");
    // Serial.print(gps_hour);
    // Serial.print(":");
    // Serial.print(gps_minute);
    // Serial.print(":");
    // Serial.println(gps_second);

    //時刻が24を超える場合の処理
    if (gps_hour >= 24) {
      gps_hour -= 24;
      gps_day += 1;
    }
    
    //月の日数を考慮して日付を修正
    uint8_t daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
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
    g_rtcYear = tmYearToCalendar(tm.Year);
    g_rtcMonth = tm.Month;
    g_rtcDay = tm.Day;
    g_rtcHour = tm.Hour;
    g_rtcMinute = tm.Minute;
    g_rtcSecond = tm.Second;
    
    // デバッグ出力G
    // Serial.print("RTC Set: ");
    // Serial.print(g_rtcYear);
    // Serial.print("/");
    // Serial.print(g_rtcMonth);
    // Serial.print("/");
    // Serial.print(g_rtcDay);
    // Serial.print(" ");
    // Serial.print(g_rtcHour);
    // Serial.print(":");
    // Serial.print(g_rtcMinute);
    // Serial.print(":");
    // Serial.println(g_rtcSecond);
  }
}

// getRTCTime() {
//   // RTCから秒単位で時刻を取得
// }

//============================================================
// 制御関連
// 浮沈用電磁弁制御
void CtrlValve() {
  if (config._diveCount <= g_divedCount) return;
  switch (g_valveCtrlState) {
    case 0:
      if ((g_timeNowMs - g_timeLastControlMs) > config._exhaustStartDelayMs) {
        digitalWrite(PIN_VALVE2_EXHAUST, HIGH);
        g_isValve2ExhaustOpen = true;
        g_isControllingValve = true;
        g_valveCtrlState = 1; // V2 Open
        g_movementState = 2;  // 沈降中
        g_timeLastControlMs = g_timeNowMs;
      }
      break;
    case 1:
      if ((g_timeNowMs - g_timeLastControlMs) > config._exhaustStopDelayMs) {
        digitalWrite(PIN_VALVE2_EXHAUST, LOW);
        g_isValve2ExhaustOpen = false;
        g_isControllingValve = true;
        g_valveCtrlState = 2; // V2 Close
        g_movementState = 2;  // 沈降中
        g_timeLastControlMs = g_timeNowMs;
      }
      break;
    case 2:
      if ((g_timeNowMs - g_timeLastControlMs) > config._supplyStartDelayMs) {
        digitalWrite(PIN_VALVE1_SUPPLY, HIGH);
        g_isValve1SupplyOpen = true;
        g_isControllingValve = true;
        g_valveCtrlState = 3; // V1 Open
        g_movementState = 1;  // 浮上中
        g_timeLastControlMs = g_timeNowMs;
      }
      break;
    case 3:
      if ((g_timeNowMs - g_timeLastControlMs) > config._supplyStopDelayMs) {
        digitalWrite(PIN_VALVE1_SUPPLY, LOW);
        g_isValve1SupplyOpen = false;
        g_isControllingValve = true;
        g_valveCtrlState = 0; // V1 Close
        g_movementState = 1;  // 浮上中
        g_divedCount++;
        g_timeLastControlMs = g_timeNowMs;
      }
      break;
  }
}

//============================================================
// SDカード保存関連

// SDカードのメイン関数
bool handleSDcard() {
  // 制御中であれば記録する
  if (g_isControllingValve == true) {
    if (!(config._logMode == 3 || config._logMode == 4)) {
      bool isSucceed = writeSDcard_CTRL();
      if (!isSucceed) {
        dispLcdError("WRITE SD CTRL FAILED!");
        Serial.println("writeSDcard_CTRL failed!");
      }
    }
    g_isControllingValve = false;
  }
  String Sensorlog = "";
  switch(config._logMode) {
    case 0:
      mkStringSDcardMode0(Sensorlog);
      break;
    case 1:
      mkStringSDcardMode1(Sensorlog);
      break;
    case 2:
      mkStringSDcardMode2(Sensorlog);
      break;
    case 3: // 電磁弁割り込みなし
      mkStringSDcardMode0(Sensorlog);
      break;
    case 4: // 電磁弁割り込みなし
      mkStringSDcardMode2(Sensorlog);
      break;
    default:
      dispLcdError("INVALID LOGMODE!");
      Serial.println("Invalid logMode!");
      return false;
      break;
  }

  File dataFile = SD.open(g_dataFileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println(Sensorlog);
    dataFile.close();
    Serial.println(Sensorlog);
    return true;
  } else {
    dispLcdError("WRITE SD FAILED!");
    Serial.println("writeSDcard failed!");
    return false;
  }
}

// 電磁弁の保存
bool writeSDcard_CTRL() {
  String CTRLlog = "";
  CTRLlog += g_timeNowMs;
  CTRLlog += ",";
  CTRLlog += g_rtcYear;
  CTRLlog += "/";
  CTRLlog += g_rtcMonth;
  CTRLlog += "/";
  CTRLlog += g_rtcDay;
  CTRLlog += "-";
  CTRLlog += g_rtcHour;
  CTRLlog += ":";
  CTRLlog += g_rtcMinute;
  CTRLlog += ":";
  CTRLlog += g_rtcSecond;
  CTRLlog += ",CTRL,";
  CTRLlog += "MSG,";

  switch(g_valveCtrlState) {
    case 1:
      CTRLlog += "UP";
      break;
    case 2:
      CTRLlog += "DOWN";
      break;
    case 3:
      CTRLlog += "PRESSURE";
      break;
    default:
      CTRLlog += "UNDEFINED";
      break;
  }

  CTRLlog += ",V1SUP,";
  CTRLlog += g_isValve1SupplyOpen;
  CTRLlog += ",V2EXH,";
  CTRLlog += g_isValve2ExhaustOpen;
  CTRLlog += ",V3PRS,";
  CTRLlog += g_isValve3PressOpen;

  File dataFile = SD.open(g_dataFileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println(CTRLlog);
    dataFile.close();
    Serial.println(CTRLlog);
    return true;
  } else {
    return false;
  }
}

// SDカード保存フォーマット　モード0~2
void mkStringSDcardMode0(String &Sensorlog) { // すべての変数を保存 *エラー検知含む
  Sensorlog += g_timeNowMs;
  Sensorlog += ",";
  Sensorlog += g_rtcYear;
  Sensorlog += "/";
  Sensorlog += g_rtcMonth;
  Sensorlog += "/";
  Sensorlog += g_rtcDay;
  Sensorlog += "-";
  Sensorlog += g_rtcHour;
  Sensorlog += ":";
  Sensorlog += g_rtcMinute;
  Sensorlog += ":";
  Sensorlog += g_rtcSecond;
  Sensorlog += ",DATA,";
  Sensorlog += "LAT,";
  Sensorlog += g_gpsLat;
  Sensorlog += ",LNG,";
  Sensorlog += g_gpsLng;
  Sensorlog += ",SATNUM,";
  Sensorlog += g_gpsSatellites;
  Sensorlog += ",ALT,";
  Sensorlog += g_gpsAltitude;
  Sensorlog += ",PIN_RAW,";
  Sensorlog += g_prsInternalRaw;
  Sensorlog += ",PIN_MBAR,";
  Sensorlog += g_prsInternalMbar;
  Sensorlog += ",POUT,";
  Sensorlog += g_prsExternal;
  Sensorlog += ",POUT_DEPTH,";
  Sensorlog += g_prsExternalDepth;
  Sensorlog += ",POUT_TMP,";
  Sensorlog += g_prsExternalTmp;
  Sensorlog += ",TMP,";
  Sensorlog += g_noramlTemp;
  Sensorlog += ",VCTRL_STATE,";
  Sensorlog += g_valveCtrlState;
  Sensorlog += ",MOV_STATE,";
  Sensorlog += g_movementState;
  Sensorlog += ",DIVE_COUNT,";
  Sensorlog += g_divedCount;
  Sensorlog += ",";
}
void mkStringSDcardMode1(String &Sensorlog) { // ほぼすべての変数を保存
  Sensorlog += g_timeNowMs;
  Sensorlog += ",";
  Sensorlog += g_rtcYear;
  Sensorlog += "/";
  Sensorlog += g_rtcMonth;
  Sensorlog += "/";
  Sensorlog += g_rtcDay;
  Sensorlog += "-";
  Sensorlog += g_rtcHour;
  Sensorlog += ":";
  Sensorlog += g_rtcMinute;
  Sensorlog += ":";
  Sensorlog += g_rtcSecond;
  Sensorlog += ",DATA,";
  Sensorlog += "LAT,";
  Sensorlog += g_gpsLat;
  Sensorlog += ",LNG,";
  Sensorlog += g_gpsLng;
  Sensorlog += ",SATNUM,";
  Sensorlog += g_gpsSatellites;
  Sensorlog += ",ALT,";
  Sensorlog += g_gpsAltitude;
  Sensorlog += ",PIN_RAW,";
  Sensorlog += g_prsInternalRaw;
  Sensorlog += ",PIN_MBAR,";
  Sensorlog += g_prsInternalMbar;
  Sensorlog += ",POUT,";
  Sensorlog += g_prsExternal;
  Sensorlog += ",POUT_DEPTH,";
  Sensorlog += g_prsExternalDepth;
  Sensorlog += ",POUT_TMP,";
  Sensorlog += g_prsExternalTmp;
  Sensorlog += ",TMP,";
  Sensorlog += g_noramlTemp;
  Sensorlog += ",VCTRL_STATE,";
  Sensorlog += g_valveCtrlState;
  Sensorlog += ",MOV_STATE,";
  Sensorlog += g_movementState;
  Sensorlog += ",DIVE_COUNT,";
  Sensorlog += g_divedCount;
  Sensorlog += ",";
}
void mkStringSDcardMode2(String &Sensorlog) { // 必要最低限のデータを保存　*GPS抜き, Rawデータなし
  Sensorlog += g_timeNowMs;
  Sensorlog += ",";
  Sensorlog += g_rtcYear;
  Sensorlog += "/";
  Sensorlog += g_rtcMonth;
  Sensorlog += "/";
  Sensorlog += g_rtcDay;
  Sensorlog += "-";
  Sensorlog += g_rtcHour;
  Sensorlog += ":";
  Sensorlog += g_rtcMinute;
  Sensorlog += ":";
  Sensorlog += g_rtcSecond;
  Sensorlog += ",DATA";
  Sensorlog += ",PIN_MBAR,";
  Sensorlog += g_prsInternalMbar;
  Sensorlog += ",POUT,";
  Sensorlog += g_prsExternal;
  Sensorlog += ",POUT_DEPTH,";
  Sensorlog += g_prsExternalDepth;
  Sensorlog += ",POUT_TMP,";
  Sensorlog += g_prsExternalTmp;
  Sensorlog += ",TMP,";
  Sensorlog += g_noramlTemp;
  Sensorlog += ",VCTRL_STATE,";
  Sensorlog += g_valveCtrlState;
  Sensorlog += ",MOV_STATE,";
  Sensorlog += g_movementState;
  Sensorlog += ",DIVE_COUNT,";
  Sensorlog += g_divedCount;
  Sensorlog += ",";
}

//============================================================
// LCD表示関数

// LCD表示のメイン関数
void handleLcdDisp() {
  lcd.setCursor(0,0); // 1行目にセット
  // 制御中であれば表示する
  if (g_isControllingValve == true) {
    if (config._lcdMode == 1) dispLcd_CTRL();
  }
  switch(config._lcdMode) {
    case 0: // 時刻 機体の状態遷移(isSensingMode)
      lcd.clear();

      lcd.print(g_rtcYear);
      lcd.print("/");
      lcd.print(g_rtcMonth);
      lcd.print("/");
      lcd.print(g_rtcDay);
      lcd.print("/");
      lcd.print(g_rtcHour);
      lcd.print(":");
      lcd.print(g_rtcMinute);
      lcd.print(":");
      lcd.print(g_rtcSecond);
      lcd.setCursor(0,1); // 2行目
      lcd.print("MODE: ");
      if (g_isSensingMode) {
        lcd.print("SENSING");
      }else {
        lcd.print("WAITING");
      }
      break;
    case 1: // 電磁弁表示
      break;
    case 2: // 水温・外圧・内圧
      lcd.clear();

      lcd.print("TMP: ");
      lcd.print(g_noramlTemp);
      lcd.print(" PRS-TMP: ");
      lcd.print(g_prsExternalTmp);
      lcd.setCursor(0,1);
      lcd.print("PRS ");
      lcd.print("EXT: ");
      lcd.print(g_prsExternal);
      lcd.print("INT: ");
      lcd.print(g_prsInternalMbar);
      break;
    default: // LCD表示なし
      lcd.noBacklight();
      break;
  }
}

void dispLcd_CTRL() {
  lcd.clear();

  lcd.print("V_CTRL: ");
  switch(g_valveCtrlState) {
    case 1:
      lcd.print("UP");
      break;
    case 2:
      lcd.print("DOWN");
      break;
    case 3:
      lcd.print("PRESSURE");
      break;
    default:
      lcd.print("UNDEFINED");
      break;
  }
  lcd.setCursor(0,1);
  lcd.print("V1SUP: ");
  lcd.print(g_isValve1SupplyOpen);
  lcd.print(" V2EXH: ");
  lcd.print(g_isValve2ExhaustOpen);
}

void dispLcdError(String errorCode) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(errorCode);
  delay(3000); // エラー確認のための猶予時間
}

bool i2CAddrTest(uint8_t addr) {
  Wire.begin();
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {
    return true;
  }
  return false;
}