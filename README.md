# ESP32 SwitchBot API & IR/RF Receiver

ESP32を使用して **SwitchBot API v1.1** と通信し、デバイスリストを取得するほか、**赤外線 (IR)** および **RF (433MHz)** リモコン信号を受信・解析する多機能IoTハブのファームウェアです。

SwitchBot API v1.1 で必須となる署名（Signature）生成プロセスを、外部ライブラリに頼らず `mbedtls` を使用して実装しているため、署名エラーに悩まされている方の参考実装としても利用できます。

## 機能 (Features)

* **SwitchBot API v1.1 対応**:
    * `mbedtls` を使用した HMAC-SHA256 署名の生成。
    * デバイスリスト（物理デバイスおよび赤外線リモコン）の取得。
    * NTPサーバー (`ntp.nict.jp`) による正確な時刻同期（API認証に必須）。
* **赤外線 (IR) 受信**:
    * `IRremote` ライブラリを使用し、家電リモコンの信号を受信・デコード。
* **RF (433MHz) 受信**:
    * `rc-switch` ライブラリを使用し、RFリモコンやセンサーの信号を受信。
* **シリアル出力**:
    * 取得したデバイス情報や受信した信号データをシリアルモニタに表示。

## 必要なハードウェア (Hardware Requirements)

* **ESP32 Development Board** (ESP32-WROOM-32など)
* **赤外線受信モジュール** (IR Receiver)
    * データピン: GPIO 27
* **RF受信モジュール** (433MHz Receiver)
    * データピン: GPIO 16
* ジャンパーワイヤ、ブレッドボード等

## 必要なライブラリ (Dependencies)

Arduino IDEのライブラリマネージャから以下のライブラリをインストールしてください。

1.  **ArduinoJson** (by Benoit Blanchon)
2.  **IRremote** (by Armin Joachimsmeyer)
3.  **rc-switch** (by Sui77)

※ `WiFi`, `WiFiClientSecure`, `mbedtls` はESP32ボードパッケージに含まれているため、追加インストールは不要です。

## セットアップ (Setup)

### 1. 認証情報の入力

ソースコード冒頭の定数部分に、ご自身の環境の情報を入力してください。

```cpp
const char* ssid = "YOUR_WIFI_SSID";             // Wi-FiのSSID
const char* password = "YOUR_WIFI_PASSWORD";     // Wi-Fiのパスワード
const char* switchbot_token = "YOUR_TOKEN";      // SwitchBotアプリから取得
const char* switchbot_secret = "YOUR_SECRET";    // SwitchBotアプリから取得
````

> **SwitchBot Token/Secretの取得方法:**
> SwitchBotアプリを開き、プロフィール \> 設定 \> アプリバージョンを10回タップして「開発者向けオプション」を表示させると取得できます。

### 2\. ネットワーク設定

環境に合わせてDNS設定を変更・削除してください。固定DNSが不要な場合は、`WiFi.config` の行をコメントアウトしてください。

```cpp
IPAddress primaryDNS(192, 168, 200, 1);
IPAddress secondaryDNS(8, 8, 8, 8);
// 不要な場合はコメントアウト
// if (!WiFi.config(...)) { ... }
```

### 3\. 書き込み

ESP32ボードを選択し、Arduino IDEから書き込みを行ってください。

## 使い方 (Usage)

1.  書き込み完了後、シリアルモニタを **115200 bps** で開きます。
2.  Wi-Fi接続後、NTPサーバーから時刻を取得します。
3.  自動的に SwitchBot API へリクエストを送信し、登録されているデバイス一覧（デバイスIDなど）を表示します。
4.  その後、待機状態になります。
      * **赤外線リモコン**をモジュールに向かって押すと、信号データが表示されます。
      * **RFリモコン**を押すと、受信コードやビット長が表示されます。

## API実装について (Technical Note)

SwitchBot API v1.1 では、リクエストヘッダーに `sign` (署名) が必要です。このコードでは以下の手順で生成しています。

1.  `token` + `time` + `nonce` を結合。
2.  `secret` をキーとして HMAC-SHA256 ハッシュを生成。
3.  ハッシュを Base64 エンコード。

これらをESP32標準の `mbedtls` ライブラリを用いて実装しています。

## License

MIT License