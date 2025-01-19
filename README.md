# TritonLite-Rev2
Triton-Lite用のプログラム修正版


# Arduinoのコーディング規約

堅牢で信頼性の高いArduinoシステムを構築するために、以下のコーディング規約を遵守してください。この規約は、bit数の厳密な指定やエラー処理、モジュール化など、システムの堅牢性を最大化するための指針を提供します。

## 基本原則

1. **可読性を重視する:** コードは他の開発者が理解しやすいように記述する。
2. **一貫性を保つ:** 命名規則やインデントスタイルを統一する。
3. **堅牢性を優先:** エラー処理を徹底し、予期せぬ入力や動作に対する耐性を確保する。
4. **リソースの効率的利用:** メモリや処理能力を無駄にしない。

## データ型の使用

- **固定幅整数を使用:** bit数を明確に指定することで、プラットフォーム依存の問題を回避する。
    - 使用例:
        ```cpp
        uint8_t counter = 0;  // 8-bit unsigned integer
        int16_t temperature = -32768;  // 16-bit signed integer
        ```
- **bool型を活用:** 真偽値には`bool`型を使用する。

## 命名規則

- **変数名:** `camelCase`を使用し、意味のある名前を付ける。
    - 使用例: `sensorValue`, `ledState`
- **定数:** `UPPER_SNAKE_CASE`を使用。 `const`または`constexpr`キーワードと共に型を明示的に指定する。
    - 使用例: `const uint16_t MAX_RETRIES = 3;`, `constexpr uint8_t LED_PIN = 13;`
- **関数名:** `camelCase`を使用し、動詞から始める。
    - 使用例: `initializeSensor`, `readTemperature`

## コード構造

- **関数の分割:** 各関数は単一の責任を持つように設計する。
- **ヘッダファイルと実装ファイルを分離:** 複数のファイルに分割してモジュール化を促進する。
    - クラスの定義例:
        ```cpp
        // Sensor.h
        #ifndef SENSOR_H
        #define SENSOR_H

        class Sensor {
        public:
          Sensor(uint8_t pin);
          bool initialize();
          int16_t readData();

        private:
          uint8_t _pin;
        };

        #endif

        // Sensor.cpp
        #include "Sensor.h"

        Sensor::Sensor(uint8_t pin) : _pin(pin) {}

        bool Sensor::initialize() {
          // 初期化処理
          return true; // 成功したらtrue、失敗したらfalseを返す
        }

        int16_t Sensor::readData() {
          // データ読み込み処理
          return 0;
        }
        ```

## エラー処理

- **戻り値の確認:** 関数の戻り値を常に確認する。
- **エラーコードの定義:** enumクラスを用いてエラーコードを定義し、関数の戻り値として返す。
    ```cpp
    enum class ErrorCode : int {
        NoError = 0,
        SensorInitError = 1,
        TimeoutError = 2,
        // ...
    };
    ```
- **ログ出力:** `Serial.println()`を使用してエラーメッセージを出力する。フォーマットは`[エラーコード]: エラーメッセージ`とする。
    ```cpp
    ErrorCode errorCode = initializeSensor();
    if (errorCode != ErrorCode::NoError) {
        Serial.print("[");
        Serial.print(static_cast<int>(errorCode));
        Serial.print("]: Sensor initialization failed.");
    }
    ```
- **エラーLED:** エラー発生時に専用のLEDを点灯させる。
- **タイムアウトの設定:** 永久ループを避け、適切にタイムアウトを設定する。 定数としてタイムアウト時間を定義する。
    ```cpp
    constexpr unsigned long TIMEOUT_MS = 5000; // 5 seconds
    unsigned long startTime = millis();
    while (!dataReady()) {
        if (millis() - startTime > TIMEOUT_MS) {
            // タイムアウトエラー処理
            break;
        }
    }
    ```

## コメントの記述

- **関数コメント:** Doxygen形式で記述する。
    ```cpp
    /**
     * @brief センサーを初期化する
     * 
     * @return ErrorCode::NoError  初期化成功
     * @return ErrorCode::SensorInitError 初期化失敗
     */
    ErrorCode initializeSensor();
    ```
- **その他コメント:** コードの意図や動作を明確に説明するコメントを記述する。 何をしているかだけでなく、なぜそうしているのかを説明する。

## コードスタイル

- **インデント:** スペース4つを使用する。
- **中括弧:** 開き括弧は文と同じ行に、閉じ括弧は新しい行に配置する。

## ハードウェアアクセス

- **ピン番号の定義:** ハードウェアリソースは`constexpr`を用いて定数として定義する。
    ```cpp
    constexpr uint8_t LED_PIN = 13;
    ```
- **直接操作を避ける:** 可能な限りArduino標準ライブラリを使用する。

## メモリ管理

- **グローバル変数の最小化:** グローバル変数は必要最小限に抑える。
- **動的メモリの注意:** `malloc`や`new`の使用は慎重に行い、メモリリークを防ぐ。 使用する場合は、必ず`free`や`delete`で解放する。

## セキュリティ

- **入力検証:** 外部入力は必ず検証する。
    ```cpp
    constexpr int MIN_VALUE = 0;
    constexpr int MAX_VALUE = 100;
    if (inputValue < MIN_VALUE || inputValue > MAX_VALUE) {
        // エラー処理
    }
    ```
- **デフォルトケースの設定:** `switch`文では必ず`default`ケースを設定する。

## Arduino特有の注意点

- `setup()`関数: 初期化処理を記述する。一度だけ実行される。
- `loop()`関数: メインループ。繰り返し実行される。  `delay()`関数の使用は避け、`millis()`を使った非ブロッキングな処理を推奨する。
- 割り込み処理:  ISR(Interrupt Service Routine)は短く、高速に処理する。

## テスト

- 単体テストを記述する。
- テストフレームワークの使用を検討する。

## コードフォーマッタ

- clang-formatの使用を推奨する。 プロジェクトルートに`.clang-format`ファイルを配置する。

## 更新手順

- このコーディング規約は必要に応じて更新される。 更新履歴をREADMEなどに記録する。
