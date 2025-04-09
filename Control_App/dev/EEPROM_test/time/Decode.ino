//時刻をデコードする例

#include <TimeLib.h>

// Function to decode the hex string to tmElements_t
void decodeDateTime(const char* hexStr, tmElements_t &tm) {
  tm.Year = strtol(String(hexStr).substring(0, 2).c_str(), NULL, 16) + 30; // Offset from 1970
  tm.Month = strtol(String(hexStr).substring(2, 4).c_str(), NULL, 16);
  tm.Day = strtol(String(hexStr).substring(4, 6).c_str(), NULL, 16);
  tm.Hour = strtol(String(hexStr).substring(6, 8).c_str(), NULL, 16);
  tm.Minute = strtol(String(hexStr).substring(8, 10).c_str(), NULL, 16);
  tm.Second = strtol(String(hexStr).substring(10, 12).c_str(), NULL, 16);
}

void setup() {
  Serial.begin(9600);

  // Example hex string
  const char* hexStr = "190107150e35";

  // Decode the hex string
  tmElements_t tm;
  decodeDateTime(hexStr, tm);

  // Print the decoded date and time
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
  // Nothing to do here
}