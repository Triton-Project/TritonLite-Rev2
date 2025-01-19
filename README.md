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
- **定数:** `UPPER_SNAKE_CASE`を使用。
  - 使用例: `MAX_RETRIES`, `LED_PIN`
- **関数名:** `camelCase`を使用し、動詞から始める。
  - 使用例: `initializeSensor`, `readTemperature`

## コード構造
- **関数の分割:** 各関数は単一の責任を持つように設計する。
  - 使用例:
    ```cpp
    void initializeSensor();
    int16_t readSensorData();
    void displayData(int16_t data);
    ```
- **ヘッダファイルと実装ファイルを分離:** 複数のファイルに分割してモジュール化を促進する。
  - 使用例:
    - `Sensor.h`
    - `Sensor.cpp`

## エラー処理
- **戻り値の確認:** 関数の戻り値を常に確認する。
  - 使用例:
    ```cpp
    if (!initializeSensor()) {
        // エラー処理
    }
    ```
- **タイムアウトの設定:** 永久ループを避け、適切にタイムアウトを設定する。
  - 使用例:
    ```cpp
    unsigned long startTime = millis();
    while (!dataReady()) {
        if (millis() - startTime > TIMEOUT_MS) {
            // エラー処理
            break;
        }
    }
    ```

## コードスタイル
- **インデント:** スペース2つまたは4つで統一する。
- **コメント:** 必要十分なコメントを記述する。
  - 使用例:
    ```cpp
    // センサーを初期化
    initializeSensor();
    ```

## ハードウェアアクセス
- **ピン番号の定義:** ハードウェアリソースは定数として定義する。
  - 使用例:
    ```cpp
    const uint8_t LED_PIN = 13;
    ```
- **直接操作を避ける:** 可能な限りArduino標準ライブラリを使用する。

## メモリ管理
- **グローバル変数の最小化:** グローバル変数は必要最小限に抑える。
- **動的メモリの注意:** `malloc`や`new`の使用は慎重に行い、メモリリークを防ぐ。

## セキュリティ
- **入力検証:** 外部入力は必ず検証する。
  - 使用例:
    ```cpp
    if (inputValue < MIN_VALUE || inputValue > MAX_VALUE) {
        // エラー処理
    }
    ```
- **デフォルトケースの設定:** `switch`文では必ず`default`ケースを設定する。
  - 使用例:
    ```cpp
    switch (command) {
        case CMD_START:
            startProcess();
            break;
        case CMD_STOP:
            stopProcess();
            break;
        default:
            // エラー処理
            break;
    }
    ```

---