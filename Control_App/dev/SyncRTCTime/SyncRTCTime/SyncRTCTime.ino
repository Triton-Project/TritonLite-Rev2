#include <TimeLib.h>
#include <Wire.h>
#include <RTC_RX8025NB.h>

RTC_RX8025NB rtc;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  while (!Serial) {
    ;  // シリアルポートが接続されるまで待機（ネイティブUSBポートのみ必要）
  }
}

void syncRTCTime() {
  if (Serial.available() > 0) {
    // 受信データを読み取る
    String receivedData = Serial.readStringUntil('\n');
    int year, month, day, hour, minute, second;
    sscanf(receivedData.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    // 受信データでRTCを設定
    rtc.setDateTime(year, month, day, hour, minute, second);
  }

  // RTCから現在の時刻を読み取る
  tmElements_t tm = rtc.read();
}

void loop() {
  syncRTCTime();

  delay(1000);
  tmElements_t tm = rtc.read();
  char s[23];
  sprintf(s, "%d/%d/%d %d:%d:%d", tmYearToCalendar(tm.Year), tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
  Serial.println(s);
  delay(10000);
}