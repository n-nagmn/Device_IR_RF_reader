#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include <IRremote.hpp>
#include <RCSwitch.h>

#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <string.h> 
#include <time.h>
#include <sys/time.h>

const uint16_t kIrRecvPin = 27;
const uint16_t kRfRecvPin = 16;

RCSwitch mySwitch = RCSwitch();

const char* ssid = "";
const char* password = "";
const char* switchbot_token = "";
const char* switchbot_secret = "";

void getDeviceList();
void hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t *output);

void setup() {
  Serial.begin(115200);
  delay(100);

  IrReceiver.begin(kIrRecvPin, ENABLE_LED_FEEDBACK);
  mySwitch.enableReceive(digitalPinToInterrupt(kRfRecvPin));
  
  Serial.println("IR/RF受信ピンを初期化しました。");
  Serial.println("API接続を開始します... (この間、IR/RF受信はできません)");

  Serial.print("Connecting to ");
  Serial.println(ssid);

  IPAddress primaryDNS(192, 168, 200, 1);
  IPAddress secondaryDNS(8, 8, 8, 8);
  if (!WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0), primaryDNS, secondaryDNS)) {
    Serial.println("Warning: Failed to set custom DNS.");
  }
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());

  configTime(3600 * 9, 0, "ntp.nict.jp", "time.google.com");
  Serial.println("Waiting for NTP time...");
  time_t now = time(nullptr);
  while (now < 100000) { 
      delay(500);
      Serial.print("*");
      now = time(nullptr);
  }
  Serial.println("\nTime synchronized.");
  
  getDeviceList();

  Serial.println("\n========================================");
  Serial.println("API処理完了。IR と RF の同時受信待機中...");
  Serial.println("========================================");
}

void loop() {
  if (IrReceiver.decode()) {
    Serial.println("\n--- IR受信 ---");
    IrReceiver.printIRResultShort(&Serial, true);
    IrReceiver.printIRSendUsage(&Serial);
    IrReceiver.printIRResultRawFormatted(&Serial, true);
    Serial.println("----------------");
    IrReceiver.resume();
  }

  if (mySwitch.available()) {
    Serial.println("\n--- RF受信 ---");
    Serial.print("データ: ");
    Serial.print(mySwitch.getReceivedValue());
    Serial.print(" / ビット長: ");
    Serial.print(mySwitch.getReceivedBitlength());
    Serial.print(" / プロトコル: ");
    Serial.println(mySwitch.getReceivedProtocol());
    Serial.println("----------------");
    mySwitch.resetAvailable();
  }
}

void getDeviceList() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }
  
  WiFiClientSecure client;
  client.setInsecure();

  const char* api_host = "api.switch-bot.com";
  const char* api_path = "/v1.1/devices";
  const int api_port = 443; 

  struct timeval tv;
  gettimeofday(&tv, NULL);
  unsigned long long now_ms = (unsigned long long)tv.tv_sec * 1000ULL + (unsigned long long)tv.tv_usec / 1000ULL;
  String t = String(now_ms);
  String nonce = "RequestID-" + t;
  String stringToSign = String(switchbot_token) + t + nonce;
  
  unsigned char hmacResult[32];
  hmac_sha256(
      (const uint8_t*)switchbot_secret, strlen(switchbot_secret), 
      (const uint8_t*)stringToSign.c_str(), stringToSign.length(), hmacResult
  );
  
  char base64EncodedSign[45]; 
  size_t encodedLen = 0; 
  mbedtls_base64_encode((unsigned char*)base64EncodedSign, sizeof(base64EncodedSign), &encodedLen, hmacResult, 32);
  base64EncodedSign[encodedLen] = '\0'; 
  
  if (!client.connect(api_host, api_port)) {
    Serial.println("HTTPS connection failed.");
    return;
  }

  String request = "GET " + String(api_path) + " HTTP/1.1\r\n" +
                   "Host: " + String(api_host) + "\r\n" +
                   "Authorization: " + String(switchbot_token) + "\r\n" +
                   "sign: " + String(base64EncodedSign) + "\r\n" +
                   "nonce: " + String(nonce) + "\r\n" +
                   "t: " + t + "\r\n" +
                   "Content-Type: application/json; charset=utf8\r\n" +
                   "Connection: close\r\n\r\n";
  
  Serial.println("\nSending GET request to " + String(api_host) + "...");
  client.print(request);
  
  unsigned long timeout = millis();
  while (client.connected() && !client.available()) {
    if (millis() - timeout > 10000) { 
      Serial.println("Client Timeout");
      client.stop();
      return;
    }
  }

  String response = "";
  int httpResponseCode = 0;
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (line.startsWith("HTTP/1.1")) {
        httpResponseCode = line.substring(9, 12).toInt();
      } else if (line.length() == 0) {
        break;
      }
    }
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);
  client.stop();
  
  if (error) {
    Serial.print("JSON deserialize failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  Serial.println("\n--- Response Received ---");
  Serial.println("HTTP Response code: " + String(httpResponseCode));
  serializeJsonPretty(doc, Serial);
  Serial.println("\n-------------------------");

  if (doc["statusCode"] == 100) {
    Serial.println("\n=== デバイスリスト ===");
    JsonArray deviceList = doc["body"]["deviceList"];
    for (JsonObject device : deviceList) {
        Serial.print("デバイス名: ");
        Serial.print(device["deviceName"].as<const char*>());
        Serial.print("  |  デバイスID: ");
        Serial.println(device["deviceId"].as<const char*>());
    }
    
    Serial.println("\n=== 赤外線リモコンリスト ===");
    JsonArray remoteList = doc["body"]["infraredRemoteList"];
    for (JsonObject remote : remoteList) {
        Serial.print("リモコン名: ");
        Serial.print(remote["deviceName"].as<const char*>());
        Serial.print("  |  デバイスID: ");
        Serial.println(remote["deviceId"].as<const char*>());
    }
    Serial.println("\n======================");
  } else {
    Serial.print("API Error: ");
    Serial.println(doc["message"].as<const char*>());
  }
}

void hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t *output) {
    mbedtls_sha256_context ctx;
    uint8_t k_ipad[64]; 
    uint8_t k_opad[64]; 
    uint8_t inner_hash[32]; 

    if (key_len > 64) {
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts(&ctx, 0); 
        mbedtls_sha256_update(&ctx, key, key_len);
        mbedtls_sha256_finish(&ctx, (unsigned char *)k_ipad);
        key = k_ipad;
        key_len = 32;
    }

    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5c, 64);
    for (size_t i = 0; i < key_len; i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_ipad, 64);
    mbedtls_sha256_update(&ctx, data, data_len);
    mbedtls_sha256_finish(&ctx, inner_hash);

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, k_opad, 64);
    mbedtls_sha256_update(&ctx, inner_hash, 32);
    mbedtls_sha256_finish(&ctx, output);
}