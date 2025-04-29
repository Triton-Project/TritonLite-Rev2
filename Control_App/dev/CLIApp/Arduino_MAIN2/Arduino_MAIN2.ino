#include <TimeLib.h>
#include <Wire.h>
#include <RTC_RX8025NB.h>
#include <LiquidCrystal_I2C.h>

RTC_RX8025NB rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCDのI2Cアドレス、16x2サイズ

// 電磁弁制御用のピン定義
constexpr uint8_t VALVE0_PIN = 7;  // 吸気バルブ制御ピン
constexpr uint8_t VALVE1_PIN = 8;  // 排気バルブ制御ピン

// イベント管理用の変数
uint16_t sup_start = 0;
uint16_t sup_stop = 0;
uint16_t exh_start = 0;
uint16_t exh_stop = 0;
uint8_t lcd_mode = 0;
uint8_t log_mode = 0;

// 状態管理用の変数
enum EventState {
  IDLE,
  SUP_START,
  SUP_STOP,
  EXH_START,
  EXH_STOP
};
EventState currentState = IDLE;
unsigned long stateStartTime = 0;
uint8_t isControling = 0;
uint8_t V0 = 0;  // バルブ0の状態
uint8_t V1 = 0;  // バルブ1の状態
uint8_t buoyState = 0;  // 浮沈状態 (0:待機, 1:浮上中, 2:沈降中)

// LCD更新間隔制御用
unsigned long lastLCDUpdate = 0;
constexpr unsigned long LCD_UPDATE_INTERVAL = 500;  // LCD更新間隔(ms)

void setup() {
  // シリアル通信の初期化
  Serial.begin(9600);
  while (!Serial) {
    ;  // シリアルポートが接続されるまで待つ
  }

  Wire.begin();
  
  // 電磁弁制御ピンの初期化
  pinMode(VALVE0_PIN, OUTPUT);
  pinMode(VALVE1_PIN, OUTPUT);
  digitalWrite(VALVE0_PIN, LOW);
  digitalWrite(VALVE1_PIN, LOW);
  
  // LCDの初期化
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Triton-Lite");
  lcd.setCursor(0, 1);
  lcd.print("Waiting...");

  delay(300);  // LCD表示の安定化待ち
  
  Serial.println("Triton-Lite initialized");
  Serial.println("Waiting for control data...");
}

void loop() {
  // シリアルデータの受信
  if (Serial.available() > 0) {
    String receivedData = Serial.readStringUntil('\n');
    Serial.print("Received: ");
    Serial.println(receivedData);

    // デコード処理
    decode_data(receivedData.c_str());
    
    // 最初のイベント(SUP_START)を開始
    startEvent(SUP_START);
    Serial.println("Starting control sequence");
  }
  
  // メイン機能: 電磁弁制御処理
  controlValves();
  
  // サブ機能: LCD表示の更新（一定間隔で実行して負荷軽減）
  updateLCDDisplay();
}

/**
 * @brief 電磁弁制御関数 - メイン機能
 * 状態に応じて電磁弁を制御する
 */
void controlValves() {
  if (currentState == IDLE) {
    return;  // アイドル状態なら何もしない
  }
  
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - stateStartTime;
  
  // 経過時間に応じて状態遷移
  switch (currentState) {
    case SUP_START:
      // 吸気開始状態
      if (elapsedTime == 0 && !isControling) {
        // 初回実行時のみ
        digitalWrite(VALVE0_PIN, HIGH);
        V0 = 1;
        isControling = 1;
        buoyState = 1;  // 浮上中
        Serial.println("SUP_START: Valve0 ON - Floating");
      }
      
      if (elapsedTime >= sup_start * 1000UL) {
        // 次の状態に遷移
        startEvent(SUP_STOP);
      }
      break;
      
    case SUP_STOP:
      // 吸気停止状態
      if (elapsedTime == 0 && isControling) {
        // 初回実行時のみ
        digitalWrite(VALVE0_PIN, LOW);
        V0 = 0;
        isControling = 1;
        buoyState = 1;  // 浮上中
        Serial.println("SUP_STOP: Valve0 OFF - Still floating");
      }
      
      if (elapsedTime >= sup_stop * 1000UL) {
        // 次の状態に遷移
        startEvent(EXH_START);
      }
      break;
      
    case EXH_START:
      // 排気開始状態
      if (elapsedTime == 0 && isControling) {
        // 初回実行時のみ
        digitalWrite(VALVE1_PIN, HIGH);
        V1 = 1;
        isControling = 1;
        buoyState = 2;  // 沈降中
        Serial.println("EXH_START: Valve1 ON - Sinking");
      }
      
      if (elapsedTime >= exh_start * 1000UL) {
        // 次の状態に遷移
        startEvent(EXH_STOP);
      }
      break;
      
    case EXH_STOP:
      // 排気停止状態
      if (elapsedTime == 0 && isControling) {
        // 初回実行時のみ
        digitalWrite(VALVE1_PIN, LOW);
        V1 = 0;
        isControling = 1;
        buoyState = 2;  // 沈降中
        Serial.println("EXH_STOP: Valve1 OFF - Still sinking");
      }
      
      if (elapsedTime >= exh_stop * 1000UL) {
        // サイクルを繰り返す
        startEvent(SUP_START);
      }
      break;
      
    default:
      break;
  }
}

/**
 * @brief イベント開始関数
 * 新しいイベントを開始し、タイマーをリセットする
 * @param newState 開始する新しい状態
 */
void startEvent(EventState newState) {
  currentState = newState;
  stateStartTime = millis();
  isControling = 0;  // 新しい状態の初回制御フラグをリセット
  
  // 現在の状態をシリアル出力
  Serial.print("State changed to: ");
  Serial.println(getStateName(currentState));
  Serial.print("Timer started: ");
  
  // 状態に応じたタイマー値を出力
  switch (currentState) {
    case SUP_START:
      Serial.print(sup_start);
      break;
    case SUP_STOP:
      Serial.print(sup_stop);
      break;
    case EXH_START:
      Serial.print(exh_start);
      break;
    case EXH_STOP:
      Serial.print(exh_stop);
      break;
    default:
      Serial.print(0);
      break;
  }
  Serial.println(" seconds");
}

/**
 * @brief 状態名取得関数
 * 状態enumを人間が読める文字列に変換する
 * @param state 変換する状態
 * @return const char* 状態の文字列表現
 */
const char* getStateName(EventState state) {
  switch (state) {
    case IDLE:      return "IDLE";
    case SUP_START: return "SUP_START";
    case SUP_STOP:  return "SUP_STOP";
    case EXH_START: return "EXH_START";
    case EXH_STOP:  return "EXH_STOP";
    default:        return "UNKNOWN";
  }
}

/**
 * @brief LCD表示更新関数 - サブ機能
 * 現在時刻とイベント状態をLCDに表示する（一定間隔で実行）
 */
void updateLCDDisplay() {
    unsigned long currentMillis = millis();
    
    // 一定間隔でのみLCD更新（頻繁な更新による負荷軽減）
    if (currentMillis - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
      lastLCDUpdate = currentMillis;
      
      // 現在時刻を取得
      tmElements_t tm = rtc.read();
      
      // 1行目: 時刻と浮沈状態
      lcd.clear();
      lcd.setCursor(0, 0);
      char timeStr[17];
      sprintf(timeStr, "%02d:%02d:%02d", tm.Hour, tm.Minute, tm.Second);
      lcd.print(timeStr);
      
      // 浮沈状態を表示
      lcd.print(" ");
      lcd.print(getStateName(currentState));
      
      // 2行目: 次のイベント名と残り時間をhh:mm:ss形式で表示
      lcd.setCursor(0, 1);
      
      // 現在のイベント名を表示
      //lcd.print(getStateName(currentState));
      lcd.print("T - ");
      
      // 残り時間（アイドル以外の場合）
      if (currentState != IDLE) {
        unsigned long elapsedTime = currentMillis - stateStartTime;
        unsigned long durationMs = 0;
        
        // 現在の状態の持続時間を取得
        switch (currentState) {
          case SUP_START:
            durationMs = sup_start * 1000UL;
            break;
          case SUP_STOP:
            durationMs = sup_stop * 1000UL;
            break;
          case EXH_START:
            durationMs = exh_start * 1000UL;
            break;
          case EXH_STOP:
            durationMs = exh_stop * 1000UL;
            break;
          default:
            durationMs = 0;
            break;
        }
        
        // 残り時間を計算して時:分:秒形式で表示
        if (elapsedTime < durationMs) {
          unsigned long remainingMs = durationMs - elapsedTime;
          unsigned int remainingSec = remainingMs / 1000;
          
          // 時:分:秒に変換
          unsigned int hours = remainingSec / 3600;
          unsigned int minutes = (remainingSec % 3600) / 60;
          unsigned int seconds = remainingSec % 60;
          
          char timeStr[9]; // "hh:mm:ss\0" の9バイト
          sprintf(timeStr, "%02u:%02u:%02u", hours, minutes, seconds);
          lcd.print(timeStr);
        } else {
          lcd.print("00:00:00");  // 残り時間がない場合
        }
      } else {
        lcd.print("--:--:--");  // アイドル状態の場合
      }
    }
  }

/**
 * @brief チェックサム計算関数
 * @param data_bytes チェックサム計算対象のバイト配列
 * @param length バイト配列の長さ
 * @return uint8_t 計算されたチェックサム値
 */
uint8_t calculate_checksum(uint8_t* data_bytes, size_t length) {
  uint8_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data_bytes[i];
  }
  return sum & 0xFF;
}

/**
 * @brief データデコード関数
 * 受信したHEX文字列をデコードして処理する
 * @param encoded_string デコード対象のHEX文字列
 */
void decode_data(const char* encoded_string) {
  size_t length = strlen(encoded_string) / 2;
  uint8_t data_bytes[length];

  // Convert hex string to bytes
  for (size_t i = 0; i < length; i++) {
    sscanf(encoded_string + 2 * i, "%2hhx", &data_bytes[i]);
  }

  rtc.setDateTime(2025, 1, 1, 1, 0, 0);

  // Extract the footer and checksum
  uint8_t footer = data_bytes[length - 1];
  uint8_t checksum = data_bytes[length - 2];
  length -= 2;

  // Extract data fields
  uint8_t header = data_bytes[0];
  if (header != 0x24) {
    Serial.println("Invalid header");
    return;
  }

  // Verify footer
  if (footer != 0x3B) {
    Serial.println("Invalid footer");
    return;
  }

  // Verify checksum
  uint8_t calculated_checksum = calculate_checksum(data_bytes, length);
  if (calculated_checksum != checksum) {
    Serial.println("Checksum does not match");
    return;
  }

  uint8_t year_offset = data_bytes[1];
  uint16_t year = 2000 + year_offset;
  uint8_t month = data_bytes[2];
  uint8_t day = data_bytes[3];
  uint8_t hour = data_bytes[4];
  uint8_t minute = data_bytes[5];
  uint8_t second = data_bytes[6];

  // グローバル変数に格納
  sup_start = (data_bytes[7] << 8) | data_bytes[8];
  sup_stop = (data_bytes[9] << 8) | data_bytes[10];
  exh_start = (data_bytes[11] << 8) | data_bytes[12];
  exh_stop = (data_bytes[13] << 8) | data_bytes[14];

  dive_count = data_bytes[15];
  press_threshold = data_bytes[16];

  uint8_t mode_byte = (data_bytes[17] << 8) | data_bytes[18];
  lcd_mode = (mode_byte >> 4) & 0x0F;
  log_mode = mode_byte & 0x0F;

  // 通信遅延を補正（現在時刻 + 1秒を設定）
  second += 1;
  if (second >= 60) {
    second = 0;
    minute += 1;
    if (minute >= 60) {
      minute = 0;
      hour += 1;
      if (hour >= 24) {
        hour = 0;
        day += 1;
        // 月と年の処理は省略（普通は不要）
      }
    }
  }

  // デコードした時刻にRTCを設定
  rtc.setDateTime(year, month, day, hour, minute, second);

  // デコードデータをシリアル出力
  Serial.print("Year: ");
  Serial.println(year);
  Serial.print("Month: ");
  Serial.println(month);
  Serial.print("Day: ");
  Serial.println(day);
  Serial.print("Hour: ");
  Serial.println(hour);
  Serial.print("Minute: ");
  Serial.println(minute);
  Serial.print("Second: ");
  Serial.println(second);
  Serial.print("Sup Start: ");
  Serial.println(sup_start);
  Serial.print("Sup Stop: ");
  Serial.println(sup_stop);
  Serial.print("Exh Start: ");
  Serial.println(exh_start);
  Serial.print("Exh Stop: ");
  Serial.println(exh_stop);
  Serial.print("Dive Count: ");
  Serial.println(dive_count);
  Serial.print("Press_threshold: ");
  Serial.println(press_threshold);
  Serial.print("LCD Mode: ");
  Serial.println(lcd_mode);
  Serial.print("Log Mode: ");
  Serial.println(log_mode);
  Serial.println("Checksum valid: true");

  // RTC時刻を読み取って表示
  tmElements_t tm = rtc.read();
  char timeStr[30];
  sprintf(timeStr, "%d/%d/%d %d:%d:%d", 
          tmYearToCalendar(tm.Year), tm.Month, tm.Day, 
          tm.Hour, tm.Minute, tm.Second);
  Serial.print("RTC Time set to: ");
  Serial.println(timeStr);
}