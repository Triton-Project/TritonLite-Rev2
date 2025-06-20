// Triton-LiteRev2 本番用プログラム（大瀬崎試験）- 最適化版v2
// 2025/06/15
// 作者: Ryusei Kamiyama
//============================================================
// ライブラリ
#include <EEPROM.h>
#include <TimeLib.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <MS5837.h>
#include <RTC_RX8025NB.h>
#include <IRremote.hpp>
#include <LiquidCrystal_I2C.h>

//============================================================
// リモコンのボタンアドレス定義
#define IR_CMD_ENTER_SENSING_MODE 0x40
#define IR_CMD_ENTER_IDLE_MODE    0x44

// 設定構造体
struct {
  uint32_t  supplyStartDelayMs  = 30000;
  uint32_t  supplyStopDelayMs   = 6000;
  uint32_t  exhaustStartDelayMs = 30000;
  uint32_t  exhaustStopDelayMs  = 3000;
  uint8_t   lcdMode             = 0;
  uint8_t   logMode             = 0;
  uint16_t  diveCount           = 10;
  uint16_t  inPressThresh       = 0;
} cfg;

//============================================================
// EEPROM関連
#define MAX_DATA_LENGTH 32
// SDカード関連
char dataFileName[13];
// 水の密度設定
#define FLUID_DENSITY 997

//============================================================
// ピン設定
#define PIN_CS_SD         10
#define PIN_ONEWIRE       4
#define PIN_IN_PRESSURE   A3
#define PIN_LED_GREEN     9
#define PIN_LED_RED       8
#define PIN_VALVE1_SUPPLY  7
#define PIN_VALVE2_EXHAUST 6
#define PIN_VALVE3_PRESS   5
#define PIN_IR_REMOTE     14

SoftwareSerial gpsSerial(3, 2);
TinyGPSPlus gps;
MS5837 DepthSensor;
RTC_RX8025NB rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

//============================================================
// センサ変数
float noramlTemp;
float prsInternalRaw;
float prsInternalMbar;
float prsExternal;
float prsExternalDepth;
float prsExternalTmp;

//============================================================
// GPSデータ
uint16_t gpsAltitude;
uint8_t gpsSatellites;
float gpsLat, gpsLng;

//============================================================
// RTC変数
uint16_t rtcYear;
uint8_t rtcMonth, rtcDay, rtcHour, rtcMinute, rtcSecond;

//============================================================
// 制御状態変数
unsigned long timeNowMs, timeLastControlMs;
bool isValve1SupplyOpen, isValve2ExhaustOpen, isValve3PressOpen;
bool isControllingValve;
int8_t valveCtrlState = 0;
int8_t movementState;
unsigned int divedCount = 0;
bool isSensingMode = false;

//============================================================
// 準備処理
//============================================================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  IrReceiver.begin(PIN_IR_REMOTE, true);
  
  // LCD初期化
  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() != 0) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();
  lcd.backlight();

  // 水圧センサ
  while (!DepthSensor.init()) {
    lcd.setCursor(0,0);
    lcd.print(F("Depth FAILED!"));
    delay(1000);
  }
  DepthSensor.setModel(MS5837::MS5837_30BA);
  DepthSensor.setFluidDensity(FLUID_DENSITY);

  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_VALVE1_SUPPLY, OUTPUT);
  pinMode(PIN_VALVE2_EXHAUST, OUTPUT);
  pinMode(PIN_VALVE3_PRESS, OUTPUT);
  pinMode(PIN_CS_SD, OUTPUT);

  if (!SD.begin(PIN_CS_SD)) {
    lcd.setCursor(0,0);
    lcd.print(F("SD FAILED"));
    return;
  }

  readEEPROM();
  timeLastControlMs = millis();

  lcd.clear();
  lcd.print(F("Ready"));
  lcd.setCursor(0,1);
  lcd.print(F("log:"));
  lcd.print(cfg.logMode);
  lcd.print(F(" lcd:"));
  lcd.print(cfg.lcdMode);
  delay(2000);
}

//============================================================
// メイン処理ループ
//============================================================
void loop() {
  timeNowMs = millis();
  
  // IR受信処理
  if (IrReceiver.decode()) {
    IrReceiver.resume();
    if (IrReceiver.decodedIRData.address == 0) {
      uint8_t cmd = IrReceiver.decodedIRData.command;
      if (cmd == IR_CMD_ENTER_SENSING_MODE) {
        isSensingMode = true;
      } else if (cmd == IR_CMD_ENTER_IDLE_MODE) {
        isSensingMode = false;
      }
    }
  }
  
  if (isSensingMode) {
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, LOW);

    // センサデータ取得
    DepthSensor.read();
    prsExternal = DepthSensor.pressure();
    prsExternalDepth = DepthSensor.depth();
    prsExternalTmp = DepthSensor.temperature();

    prsInternalRaw = analogRead(PIN_IN_PRESSURE);
    float v = prsInternalRaw * 0.00488 - 0.25; // /1024*5を最適化
    prsInternalMbar = v * 6.667; // /4.5*30を最適化
    float prsDiff = (prsInternalMbar * 68.94 + 1013.25) - prsExternal;
    // 加圧制御
    if (prsDiff+cfg.inPressThresh < 0) {
      digitalWrite(PIN_VALVE3_PRESS, HIGH);
      isValve3PressOpen = true;
      isControllingValve = true;
      movementState = 3;
      delay(100);
    } else {
      digitalWrite(PIN_VALVE3_PRESS, LOW);
      isValve3PressOpen = false;
    }
    
    getGPSData();
    getTemperatureData();
    ctrlValve();
    handleLcdDisp();
    handleSDcard();
  } else {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    handleEEPROMSerial();
  }
}

//============================================================
// EEPROM関連（簡略化）
void handleEEPROMSerial() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    Serial.print("Recieved: ");
    Serial.println(data);
    writeEEPROM(data);
    readEEPROM();
  }
}

bool writeEEPROM(String &str) {
  uint8_t len = str.length() / 2;
  if (len > MAX_DATA_LENGTH || len < 3) return false;

  uint8_t buf[len];
  for (uint8_t i = 0; i < len; i++) {
    sscanf(str.c_str() + 2 * i, "%2hhx", &buf[i]);
  }

  if (buf[0] != 0x24 || buf[len-1] != 0x3B) return false;

  uint8_t sum = 0;
  for (uint8_t i = 0; i <= len-3; i++) sum += buf[i];
  if ((sum & 0xFF) != buf[len-2]) return false;

  decodeData(buf);

  EEPROM.write(0, 0xAA);
  EEPROM.write(1, len);
  for (uint8_t i = 0; i < len; i++) {
    EEPROM.write(i + 2, buf[i]);
  }
  return true;
}

bool readEEPROM() {
  uint8_t len = EEPROM.read(1);
  if (EEPROM.read(2) != 0x24) return false;

  uint8_t buf[len];
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = EEPROM.read(i+2);
  }

  if (buf[len-1] != 0x3B) return false;

  uint8_t sum = 0;
  for (uint8_t i = 0; i <= len-3; i++) sum += buf[i];
  if ((sum & 0xFF) != buf[len-2]) return false;

  decodeData(buf);
  return true;
}

void decodeData(const uint8_t* d) {
  uint16_t yr = 2000 + d[1];
  uint8_t mo = d[2], dy = d[3], hr = d[4], mn = d[5], sc = d[6];
  
  rtc.setDateTime(yr, mo, dy, hr, mn, sc);
  
  cfg.supplyStartDelayMs = ((uint32_t)(d[7] << 8 | d[8])) * 1000;
  cfg.supplyStopDelayMs = d[9] << 8 | d[10];
  cfg.exhaustStartDelayMs = ((uint32_t)(d[11] << 8 | d[12])) * 1000;
  cfg.exhaustStopDelayMs = d[13] << 8 | d[14];
  cfg.lcdMode = (d[15] >> 4) & 0x0F;
  cfg.logMode = d[15] & 0x0F;
  cfg.diveCount = d[16];
  cfg.inPressThresh = d[17];

  // 表示
  Serial.print(yr);Serial.print('/');
  Serial.print(mo);Serial.print('/');
  Serial.print(dy);Serial.print(' ');
  Serial.print(hr);Serial.print(':');
  Serial.print(mn);Serial.print(':');
  Serial.println(sc);

  Serial.print("Sup Start: "); Serial.println(cfg.supplyStartDelayMs);
  Serial.print("Sup Stop : "); Serial.println(cfg.supplyStopDelayMs);
  Serial.print("Exh Start: "); Serial.println(cfg.exhaustStartDelayMs);
  Serial.print("Exh Stop : "); Serial.println(cfg.exhaustStopDelayMs);
  Serial.print("LCD Mode : "); Serial.println(cfg.lcdMode);
  Serial.print("Log Mode : "); Serial.println(cfg.logMode);
  Serial.print("Dive Cnt : "); Serial.println(cfg.diveCount);
  Serial.print("Thresh   : "); Serial.println(cfg.inPressThresh);

  sprintf(dataFileName, "%02d%02d_%02d.csv", mo, dy, hr);
}

//============================================================
// センシング関連
void getTemperatureData() {
  OneWire ow(PIN_ONEWIRE);
  DallasTemperature s(&ow);
  s.begin();
  s.requestTemperatures();
  noramlTemp = s.getTempCByIndex(0);
}

void getGPSData() {
  gpsSerial.begin(9600);
  
  unsigned long st = millis();
  while ((millis() - st) < 1000) {
    while (gpsSerial.available()) {
      if (gps.encode(gpsSerial.read())) {
        if (gps.location.isUpdated()) {
          gpsLat = gps.location.lat();
          gpsLng = gps.location.lng();
          gpsAltitude = gps.altitude.meters();
          gpsSatellites = gps.satellites.value();
        }
        if (gps.time.isValid() && gps.date.isValid()) {
          correctTime();
          break;
        }
      }
    }
  }
  gpsSerial.end();
}

void correctTime() {
  if (!gps.time.isValid() || !gps.date.isValid()) return;
  
  int y = gps.date.year();
  int m = gps.date.month();
  int d = gps.date.day();
  int h = gps.time.hour() + 9;
  int mn = gps.time.minute();
  int s = gps.time.second();

  if (h >= 24) { h -= 24; d++; }
  
  uint8_t dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (d > dim[m-1]) {
    if (!(m == 2 && d == 29 && ((y%4==0 && y%100!=0) || y%400==0))) {
      d = 1; m++;
    }
  }
  if (m > 12) { m = 1; y++; }

  rtc.setDateTime(y, m, d, h, mn, s);
  
  tmElements_t tm = rtc.read();
  rtcYear = tmYearToCalendar(tm.Year);
  rtcMonth = tm.Month;
  rtcDay = tm.Day;
  rtcHour = tm.Hour;
  rtcMinute = tm.Minute;
  rtcSecond = tm.Second;
}

//============================================================
// 制御関連
void ctrlValve() {
  if (cfg.diveCount <= divedCount && cfg.diveCount != 0) return;
  
  unsigned long dt = timeNowMs - timeLastControlMs;
  
  switch (valveCtrlState) {
    case 0:
      if (dt > cfg.exhaustStartDelayMs) {
        digitalWrite(PIN_VALVE2_EXHAUST, HIGH);
        isValve2ExhaustOpen = true;
        isControllingValve = true;
        valveCtrlState = 1;
        movementState = 2;
        timeLastControlMs = timeNowMs;
      }
      break;
    case 1:
      if (dt > cfg.exhaustStopDelayMs) {
        digitalWrite(PIN_VALVE2_EXHAUST, LOW);
        isValve2ExhaustOpen = false;
        isControllingValve = true;
        valveCtrlState = 2;
        movementState = 2;
        timeLastControlMs = timeNowMs;
      }
      break;
    case 2:
      if (dt > cfg.supplyStartDelayMs) {
        digitalWrite(PIN_VALVE1_SUPPLY, HIGH);
        isValve1SupplyOpen = true;
        isControllingValve = true;
        valveCtrlState = 3;
        movementState = 1;
        timeLastControlMs = timeNowMs;
      }
      break;
    case 3:
      if (dt > cfg.supplyStopDelayMs) {
        digitalWrite(PIN_VALVE1_SUPPLY, LOW);
        isValve1SupplyOpen = false;
        isControllingValve = true;
        valveCtrlState = 0;
        movementState = 1;
        divedCount++;
        timeLastControlMs = timeNowMs;
      }
      break;
  }
}

//============================================================
// SDカード保存関連
bool handleSDcard() {
  char buf[256];
  
  if (isControllingValve && cfg.logMode != 2 && cfg.logMode != 3) {
    sprintf(buf, "%lu,%04d/%02d/%02d-%02d:%02d:%02d,CTRL,MSG,%s,V1SUP,%d,V2EXH,%d,V3PRS,%d",
      timeNowMs, rtcYear, rtcMonth, rtcDay, rtcHour, rtcMinute, rtcSecond,
      movementState==1?"UP":movementState==2?"DOWN":movementState==3?"PRESSURE":"UNDEF",
      isValve1SupplyOpen, isValve2ExhaustOpen, isValve3PressOpen);
    
    File f = SD.open(dataFileName, FILE_WRITE);
    if (f) {
      f.println(buf);
      f.close();
    }
    isControllingValve = false;
  }
  
  // データログ
  if (cfg.logMode == 0 || cfg.logMode == 2) {
    sprintf(buf, "%lu,%04d/%02d/%02d-%02d:%02d:%02d,DATA,LAT,%.6f,LNG,%.6f,SATNUM,%d,ALT,%d,PIN_RAW,%.0f,PIN_MBAR,%.1f,POUT,%.1f,POUT_DEPTH,%.1f,POUT_TMP,%.1f,TMP,%.1f,VCTRL_STATE,%d,MOV_STATE,%d,DIVE_COUNT,%d,",
      timeNowMs, rtcYear, rtcMonth, rtcDay, rtcHour, rtcMinute, rtcSecond,
      gpsLat, gpsLng, gpsSatellites, gpsAltitude,
      prsInternalRaw, prsInternalMbar, prsExternal, prsExternalDepth, prsExternalTmp,
      noramlTemp, valveCtrlState, movementState, divedCount);
  } else if (cfg.logMode == 1 || cfg.logMode == 3) {
    sprintf(buf, "%lu,%04d/%02d/%02d-%02d:%02d:%02d,DATA,PIN_MBAR,%.1f,POUT,%.1f,POUT_DEPTH,%.1f,POUT_TMP,%.1f,TMP,%.1f,VCTRL_STATE,%d,MOV_STATE,%d,DIVE_COUNT,%d,",
      timeNowMs, rtcYear, rtcMonth, rtcDay, rtcHour, rtcMinute, rtcSecond,
      prsInternalMbar, prsExternal, prsExternalDepth, prsExternalTmp,
      noramlTemp, valveCtrlState, movementState, divedCount);
  } else {
    lcd.clear();
    lcd.print(F("logMode UNKNOWN!"));
  }
  
  File f = SD.open(dataFileName, FILE_WRITE);
  if (f) {
    f.println(buf);
    f.close();
    return true;
  }
  return false;
}

//============================================================
// LCD表示関数
void handleLcdDisp() {
  if (isControllingValve && cfg.lcdMode == 1) {
    lcd.clear();
    lcd.print(F("V_CTRL:"));
    lcd.print(movementState==1?F("UP"):movementState==2?F("DOWN"):movementState==3?F("PRESS"):F("UNDEF"));
    lcd.setCursor(0,1);
    lcd.print(F("V1:"));
    lcd.print(isValve1SupplyOpen);
    lcd.print(F(" V2:"));
    lcd.print(isValve2ExhaustOpen);
  }
  
  switch(cfg.lcdMode) {
    case 0:
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(rtcYear);
      lcd.print('/');
      lcd.print(rtcMonth);
      lcd.print('/');
      lcd.print(rtcDay);
      lcd.print(' ');
      lcd.print(rtcHour);
      lcd.print(':');
      lcd.print(rtcMinute);
      lcd.setCursor(0,1);
      lcd.print(rtcSecond);
      lcd.print("sec ");
      lcd.print("dived:");
      lcd.print(divedCount);
      break;
    case 1:
      break;
    case 2:
      lcd.clear();
      lcd.print(F("TMP:"));
      lcd.print(noramlTemp);
      lcd.setCursor(0,1);
      lcd.print(F("DepthTMP:"));
      lcd.print(prsExternalTmp);
      break;
    case 3:
      lcd.clear();
      lcd.print(F("PrsExt:"));
      lcd.print(prsExternal);
      lcd.setCursor(0,1);
      lcd.print(F("PrsInt:"));
      lcd.print(prsInternalMbar * 68.94 + 1013.25);
      break;
    default:
      lcd.noBacklight();
      break;
  }
}