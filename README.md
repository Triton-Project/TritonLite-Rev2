# Arduinoコーディング規約

## 基本原則

1. **可読性重視**: コードは他の開発者が容易に理解できるように記述する
2. **一貫性の維持**: 命名規則やコードスタイルを統一する
3. **堅牢性の確保**: 適切なエラー処理と予期せぬ状況への対応
4. **リソース効率**: Arduinoの限られたメモリと処理能力を効率的に使用する

## 命名規則

### 変数

#### グローバル変数
- `g_` プレフィックスを使用し、`camelCase` で記述
  ```cpp
  uint16_t g_systemUptime;
  bool g_isSystemInitialized;
  SensorData g_lastSensorReading;
  ```

#### ローカル変数
- プレフィックスなしの `camelCase` で記述
  ```cpp
  void processData() {
    int sensorValue = analogRead(PIN_SENSOR);
    float convertedValue = sensorValue * 0.0048;
    bool isValidReading = (convertedValue > MIN_THRESHOLD);
  }
  ```

#### メンバ変数
- アンダースコアプレフィックス `_` を使用し、`camelCase` で記述
  ```cpp
  class Sensor {
  private:
    int _pin;
    long _lastReadTime;
    float _calibrationFactor;
  };
  ```

#### 静的変数
- `s_` プレフィックスを使用し、`camelCase` で記述
  ```cpp
  static uint8_t s_errorCount = 0;
  ```

- **ブール変数**: 全ての種類の変数において、`is`, `has`, `can` などの接頭辞を付ける
  ```cpp
  bool g_isConnected;          // グローバル
  bool isDataReady;            // ローカル
  bool _hasNewReading;         // メンバ
  static bool s_canProceed;    // 静的
  ```

### 定数

- **定数**: `UPPER_SNAKE_CASE` で記述し、型を明示
  ```cpp
  const uint8_t LED_PIN = 13;
  constexpr uint16_t MAX_SENSOR_VALUE = 1023;
  ```

- **グローバル定数**: 型を明示したプレフィックス付きの `UPPER_SNAKE_CASE`
  ```cpp
  constexpr uint8_t U8_MAX_RETRIES = 3;
  const float F_CONVERSION_FACTOR = 0.0048;
  ```

- **ピン定義**: `PIN_` プレフィックスを使用
  ```cpp
  constexpr uint8_t PIN_LED = 13;
  constexpr uint8_t PIN_BUTTON = 2;
  ```

### 関数

- **関数名**: `camelCase` で記述し、動詞で開始
  ```cpp
  void initSystem();
  bool readSensor();
  float calculateAverage();
  ```

- **getter/setter**: `get`/`set` プレフィックスを使用
  ```cpp
  int getTemperature();
  void setThreshold(int value);
  ```

### 列挙型

- **enum class**: 名前は `PascalCase`、値は `UPPER_SNAKE_CASE`
  ```cpp
  enum class SensorState {
    NOT_INITIALIZED,
    INITIALIZING,
    READY,
    ERROR
  };
  ```

## データ型

- **固定幅整数型**: 必ず bit 数を明示
  ```cpp
  uint8_t counter;      // 0-255
  int16_t temperature;  // -32768 to 32767
  uint32_t timestamp;   // 大きな非負整数
  ```

- **浮動小数点**: 精度要件に応じて選択
  ```cpp
  float voltage;      // 一般的な用途
  double precision;   // 高精度が必要な場合（ただしメモリ使用量に注意）
  ```

- **文字列**: `String` クラスは避け、`char` 配列か `PGM_P`（プログラムメモリ文字列）を使用
  ```cpp
  // 避けるべき方法
  String message = "Status OK";  // メモリ断片化の原因になる

  // 推奨される方法
  char message[20] = "Status OK";
  
  // プログラムメモリに格納する方法（フラッシュメモリ節約）
  const char message[] PROGMEM = "Status OK";
  ```

## コード構造

### ファイル構成

- ヘッダファイル（`.h`）にはクラス/関数宣言を、実装ファイル（`.cpp`）には定義を配置
- 各ファイルの先頭には簡潔な説明を記載
- インクルードガードを必ず使用

```cpp
// Sensor.h
#ifndef SENSOR_H
#define SENSOR_H

class Sensor {
public:
  Sensor(uint8_t pin);
  bool initialize();
  int16_t readValue();

private:
  uint8_t _pin;
};

#endif // SENSOR_H
```

### 関数設計

- 各関数は単一責任の原則に従う
- 関数の長さは画面一画面（約30行）以内に収める
- 入力の検証を必ず行う

```cpp
bool setSensorThreshold(int16_t value) {
  // 入力値の検証
  if (value < MIN_THRESHOLD || value > MAX_THRESHOLD) {
    return false;
  }
  
  _threshold = value;
  return true;
}
```

## コメント

### 関数ドキュメント

- Doxygen形式でコメントを記述
- パラメータ、戻り値、例外を明記

```cpp
/**
 * @brief センサーから温度を読み取る
 * 
 * @param sensorPin センサーが接続されているアナログピン
 * @return float 摂氏温度。読み取りに失敗した場合はNAN
 * @note この関数は最大100msのブロッキング処理を含む
 */
float readTemperature(uint8_t sensorPin) {
  // 実装...
}
```

### インラインコメント

- 複雑なロジックには説明を追加
- **何を**ではなく**なぜ**を説明

```cpp
// 不適切なコメント
i++; // インクリメント

// 適切なコメント
delayCounter++; // 5回のサンプリングごとに平均値を計算するためのカウンタ
```

### コード領域の説明

- 機能ブロックには見出しコメントを付ける

```cpp
//===== センサー初期化 =====
pinMode(PIN_SENSOR_POWER, OUTPUT);
digitalWrite(PIN_SENSOR_POWER, HIGH);
delay(100); // センサー起動待ち

//===== 通信設定 =====
Serial.begin(9600);
Wire.begin();
```

## エラー処理

### エラーコード

- エラー状態は列挙型で定義

```cpp
enum class ErrorCode : uint8_t {
  SUCCESS = 0,
  SENSOR_NOT_FOUND = 1,
  CALIBRATION_FAILED = 2,
  COMMUNICATION_ERROR = 3,
  TIMEOUT = 4
};
```

### エラー管理

- エラー発生時は早期リターン
- エラーログ記録の統一関数を用意

```cpp
ErrorCode result = initSensor();
if (result != ErrorCode::SUCCESS) {
  logError("Sensor init failed", result);
  return false;
}
```

### 例外処理

- 通常の例外処理は不用意にシステムを停止させるので避ける
- 重大なエラーは専用のエラー状態に遷移させる

```cpp
if (errorDetected) {
  systemState = SystemState::ERROR;
  activateErrorLed();
  // 必要に応じてWatchdogタイマーでリセット
}
```

## Arduino特有のガイドライン

### タイミング制御

- `delay()`関数の使用を最小限に抑える
- 非ブロッキング処理を実装

```cpp
// 非推奨
void loop() {
  readSensors();
  delay(1000);
}

// 推奨
unsigned long previousMillis = 0;
const unsigned long interval = 1000;

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    readSensors();
  }
  
  // その他の処理を継続して実行できる
}
```

### メモリ管理

- 動的メモリ割り当て（`new`/`malloc`）は避ける
- グローバル変数を最小限に抑える
- 大きな配列は `PROGMEM` に格納

```cpp
// メモリ効率の良い文字列定義
const char MESSAGE_READY[] PROGMEM = "System ready";
const char MESSAGE_ERROR[] PROGMEM = "Error detected";

// 使用時
char buffer[32];
strcpy_P(buffer, (PGM_P)pgm_read_word(&MESSAGE_READY));
Serial.println(buffer);
```

### 割り込み処理

- 割り込みサービスルーチン（ISR）は短く、シンプルに
- ISR内では`Serial`や複雑な計算を避ける
- 共有変数には`volatile`修飾子を使用

```cpp
volatile bool g_buttonPressed = false;

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), buttonISR, FALLING);
}

void buttonISR() {
  g_buttonPressed = true;  // フラグのみ設定し、実際の処理はloop()で行う
}

void loop() {
  if (g_buttonPressed) {
    handleButtonPress();
    g_buttonPressed = false;
  }
}
```

### シリアル通信

- シリアル通信のフォーマットを統一
- データ送信のプロトコルを明確に定義

```cpp
// JSON形式でのデータ送信例
void sendSensorData(float temperature, uint16_t light) {
  Serial.print("{\"temp\":");
  Serial.print(temperature, 1);  // 小数点1桁
  Serial.print(",\"light\":");
  Serial.print(light);
  Serial.println("}");
}
```

## コードスタイル

### インデント・フォーマット

- インデントはスペース2個または4個を一貫して使用
- 波括弧は開始は同じ行に、終了は新しい行に配置

```cpp
if (condition) {
  doSomething();
} else {
  doSomethingElse();
}
```

### 式と文

- 複雑な条件式は括弧で明確に
- マジックナンバーの使用を避け、定数を定義

```cpp
// 不適切
if (value > 500 && value < 1000) {
  // ...
}

// 適切
constexpr int16_t THRESHOLD_MIN = 500;
constexpr int16_t THRESHOLD_MAX = 1000;

if (value > THRESHOLD_MIN && value < THRESHOLD_MAX) {
  // ...
}
```

## ハードウェア制御

### ピン設定

- ピン設定は`setup()`でまとめて行う
- 入力ピンには適切なプルアップ/プルダウンを設定

```cpp
void setup() {
  // 出力ピン
  pinMode(PIN_LED_STATUS, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  
  // 入力ピン
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SENSOR, INPUT);
  
  // 初期状態設定
  digitalWrite(PIN_LED_STATUS, LOW);
  digitalWrite(PIN_RELAY, LOW);
}
```

### 省電力設計

- 電池駆動の場合、スリープモードを活用
- 不要なハードウェアは無効化

```cpp
#include <avr/sleep.h>
#include <avr/power.h>

void enterSleepMode() {
  // 不要な周辺機能を無効化
  power_adc_disable();
  power_spi_disable();
  
  // スリープモード設定
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();
  
  // ここから先は割り込み発生後に実行される
  sleep_disable();
  
  // 必要な周辺機能を再有効化
  power_adc_enable();
  power_spi_enable();
}
```

## テスト

### 単体テスト

- テスト用スケッチを作成
- モジュールごとにテスト関数を用意

```cpp
void testTemperatureSensor() {
  Serial.println(F("Testing temperature sensor..."));
  
  float temp = readTemperature();
  
  Serial.print(F("Temperature: "));
  Serial.println(temp);
  
  if (isnan(temp) || temp < -50.0 || temp > 100.0) {
    Serial.println(F("TEST FAILED: Temperature out of range"));
  } else {
    Serial.println(F("TEST PASSED"));
  }
}
```

### デバッグ支援

- デバッグ用のプリプロセッサマクロを定義

```cpp
// デバッグモード定義
#define DEBUG 1

// デバッグ出力マクロ
#if DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// 使用例
DEBUG_PRINTLN(F("Initializing..."));
```

## バージョン管理とドキュメント

### コードヘッダ

- 各ファイルの先頭にメタ情報を記載

```cpp
/**
 * @file SensorModule.cpp
 * @brief センサーモジュールの実装
 * @author 開発者名
 * @date 2025-04-09
 * @version 1.2.0
 */
```
