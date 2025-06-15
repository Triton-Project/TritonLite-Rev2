#include <IRremote.hpp>

bool g_isSensingMode = false;

#define IR_CMD_ENTER_SENSING_MODE 0x40
#define IR_CMD_ENTER_IDLE_MODE    0x44
#define PIN_LED_GREEN 8
#define PIN_LED_RED 9

void setup() {
    // Serial.begin(9600);
    IrReceiver.begin(14, true);
    while(!Serial);
    Serial.println("Serial begin...");
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, HIGH);
    delay(1000);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, LOW);
}

void loop() {
  // 遷移モードの受信
  if (IrReceiver.decode()) {
    IrReceiver.printIRResultShort(&Serial);
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
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, HIGH);
  }

  // 
  if (g_isSensingMode) {
    // センシングモードの処理
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
  } else {
    // 待機モードの処理
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
  }
}