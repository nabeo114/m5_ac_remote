/*
    note: need add library Adafruit_SHT31 from library manage
    Github: https://github.com/adafruit/Adafruit_SHT31
    
    note: need add library Adafruit_BMP280 from library manage
    Github: https://github.com/adafruit/Adafruit_BMP280_Library
*/

#include <M5StickC.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Mitsubishi.h>
#define USE_ENV2 // Use ENV II
#ifdef USE_ENV2
  #include <Adafruit_SHT31.h>
#else
#include "DHT12.h"
#endif
#include <Adafruit_BMP280.h>
#include "m5_ac_remote.h"

WiFiClientSecure httpsClient;
PubSubClient mqttClient(httpsClient);
char pubMessage[1024];

const uint16_t kIrLed = 9;  // M5StickC IR
IRMitsubishiAC ac(kIrLed);  // Set the GPIO used for sending messages.

#ifdef USE_ENV2
Adafruit_SHT31 sht31;
#else
DHT12 dht12;
#endif
Adafruit_BMP280 bme;

void setup_wifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid, password);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_mqtt() {
  httpsClient.setCACert(rootCA);
  httpsClient.setCertificate(certificate);
  httpsClient.setPrivateKey(privateKey);
  mqttClient.setServer(endpoint, mqttPort);
  mqttClient.setCallback(mqttCallback);
}

void connect_mqtt() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe(subTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0; i<length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

//  StaticJsonDocument<500> doc;
  const size_t capacity = 3*JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + 60;
  DynamicJsonDocument doc(capacity);

  deserializeJson(doc, payload);

  JsonObject state = doc["state"];

  if (state.containsKey("power")) {
    const bool state_power = state["power"];
    ac.setPower(state_power);
  }
  if (state.containsKey("mode")) {
    const uint8_t state_mode = state["mode"];
    ac.setMode(state_mode);
  }
  if (state.containsKey("temp")) {
    const uint8_t state_temp = state["temp"];
    ac.setTemp(state_temp);
  }

  ac.send();
  printState();
  displayACRemote();
  publishDeviceShadowReported();
}

void mqttPublish(const char* topic, const char* payload) {
  Serial.print("Publish message: ");
  Serial.println(topic);
  Serial.println(payload);
  mqttClient.publish(topic, payload);
  Serial.println("Published.");
}

void publishDeviceShadowReported() {
  sprintf(pubMessage, "{\"state\": {\"reported\": {\"power\": %d,\"mode\": %d,\"temp\": %d}}}", ac.getPower(), ac.getMode(), ac.getTemp());
  mqttPublish(pubTopic, pubMessage);
}

void publishDeviceShadowDesired() {
  sprintf(pubMessage, "{\"state\": {\"desired\": {\"power\": %d,\"mode\": %d,\"temp\": %d}}}", ac.getPower(), ac.getMode(), ac.getTemp());
  mqttPublish(pubTopic, pubMessage);
}

void printState() {
  // Display the settings.
  Serial.println("Mitsubishi A/C remote is in the following state:");
  Serial.printf("  %s\n", ac.toString().c_str());
  // Display the encoded IR sequence.
  unsigned char* ir_code = ac.getRaw();
  Serial.print("IR Code: 0x");
  for (uint8_t i = 0; i < kMitsubishiACStateLength; i++)
    Serial.printf("%02X", ir_code[i]);
  Serial.println();
}

int disp_mode = 0; // AC Remote

void displayACRemote() {
  if (disp_mode != 0) {
    return;
  }
  
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.println("M5 AC Remote");

  bool power = ac.getPower();
  M5.Lcd.setCursor(0, 20, 2);
  M5.Lcd.printf("Power: %s", power ? "ON" : "OFF");

  char *mode;
  switch (ac.getMode()) {
  case kMitsubishiAcCool:
    mode = "Cool";
    break;
  case kMitsubishiAcDry:
    mode = "Dry";
    break;
  case kMitsubishiAcHeat:
    mode = "Heat";
    break;
  default:
    mode = "Err";
    break;
  }
  M5.Lcd.setCursor(0, 40, 2);
  M5.Lcd.printf("Mode: %s", mode);

  uint8_t temp = ac.getTemp();
  M5.Lcd.setCursor(0, 60, 2);
  M5.Lcd.printf("Temperature: %2d C", temp);
}

void setup_time() {
  configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
}

void send_post_request(const char *url, const String &body) {
  HTTPClient http;
  
  Serial.print("[HTTP] begin...\n");
  // configure traged server and url
  http.begin(url); //HTTP

  const char* headerNames[] = {"Location"};
  http.collectHeaders(headerNames, sizeof(headerNames) / sizeof(headerNames[0]));
  
  Serial.print("[HTTP] POST...\n");
  // start connection and send HTTP header
  int httpCode = http.POST(body);
  
  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);
   
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
//      Serial.println(payload);
    }
    else if (httpCode == HTTP_CODE_FOUND) {
      String payload = http.getString();
//      Serial.println(payload);
      
      Serial.printf("[HTTP] POST... Location: %s\n", http.header("Location").c_str());

      String message_body;
//      send_get_request(http.header("Location").c_str(), message_body);
//      Serial.println(message_body);
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

void setup() {
  // put your setup code here, to run once:
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);

  Serial.begin(115200);
  delay(200);

  ac.begin();

  Wire.begin(32,33);

#ifdef USE_ENV2
  if (!sht31.begin(0x44)) {
    Serial.println("Could not find a valid SHT31 sensor, check wiring!");
    while (1);
  }
#endif

  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1);
  }

  setup_wifi();
  setup_mqtt();
  setup_time();

  // Set up what we want to send. See ir_Mitsubishi.cpp for all the options.
  Serial.println("Default state of the remote.");
  printState();
  Serial.println("Setting desired state for A/C.");
  unsigned char* ir_code = ac.getRaw();
  ir_code[14] = 0x04;  // 内部クリーン
  ac.setRaw(ir_code);
  ac.off();
  ac.setFan(kMitsubishiAcFanAuto);
  ac.setMode(kMitsubishiAcCool);
  ac.setTemp(28);
  ac.setVane(kMitsubishiAcVaneAuto);
  ac.setWideVane(kMitsubishiAcWideVaneAuto);
  printState();
  displayACRemote();

  connect_mqtt();
  delay(500); // Wait for a while after subscribe "/update/delta" before publish "/update".
  publishDeviceShadowReported();
}

float temperature;
float humidity;
float pressure;
const unsigned long detect_interval = 1000; // 1sec
const unsigned long upload_interval = 300000; // 5min
unsigned long current_time = 0;
unsigned long detect_time = 0;
unsigned long upload_time = 0;

void displayEnvMonitor() {
  if (disp_mode == 0) {
    return;
  }
  
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.println("M5 ENV Monitor");

  M5.Lcd.setCursor(0, 20, 2);
  M5.Lcd.printf("Temperature: %2.1f C", temperature);
  M5.Lcd.setCursor(0, 40, 2);
  M5.Lcd.printf("Humidity: %2.0f %%", humidity);
  M5.Lcd.setCursor(0, 60, 2);
  M5.Lcd.printf("Pressure: %4.1f hPa", pressure);
}

void loop() {
  // put your main code here, to run repeatedly:
  M5.update();

  if (!mqttClient.connected()) {
    connect_mqtt();
  }
  mqttClient.loop();

  if (M5.BtnA.wasPressed()) {
    if (ac.getPower()) {
      ac.off();
    } else {
      ac.on();
    }
    publishDeviceShadowDesired();
  }

  if (M5.BtnB.wasPressed()) {
    M5.Lcd.fillScreen(BLACK);
    if (disp_mode == 0) {
      disp_mode = 1; // Env Monitor
      displayEnvMonitor();
    } else {
      disp_mode = 0; // AC Remote
      displayACRemote();
    }
  }

  current_time = millis();
  if ((long)(detect_time - current_time) < 0) {
#ifdef USE_ENV2
    temperature = sht31.readTemperature();
    humidity = sht31.readHumidity();
#else
    temperature = dht12.readTemperature();
    humidity = dht12.readHumidity();
#endif
    pressure = bme.readPressure() / 100.0F;

    displayEnvMonitor();

    detect_time = current_time + detect_interval;

    if ((long)(upload_time - current_time) < 0) {
      struct tm timeInfo;
      getLocalTime(&timeInfo); // 時刻を取得

      char time[50];
      sprintf(time, "%04d/%02d/%02d %02d:%02d:%02d",
        timeInfo.tm_year+1900, timeInfo.tm_mon+1, timeInfo.tm_mday,
        timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

      Serial.printf("Time: %s\n", time);
      Serial.printf("Temperature: %2.1f C\n", temperature);
      Serial.printf("Humidity: %2.0f %%\n", humidity);
      Serial.printf("Pressure: %4.1f hPa\n", pressure);

      //jsonデータ作成
//      StaticJsonDocument<500> doc;
//      const size_t capacity = JSON_OBJECT_SIZE(4);
//      DynamicJsonDocument doc(capacity);
      DynamicJsonDocument doc(128);
      doc["time"] = time;
      doc["temperature"] = temperature;
      doc["humidity"] = humidity;
      doc["pressure"] = pressure;

      String message_body;

      serializeJson(doc, message_body);

      Serial.println(message_body);

      send_post_request(host, message_body);
      mqttPublish(pubTopicDB, message_body.c_str());
 
      upload_time = current_time + upload_interval;
    }
  }

  delay(1);

  // Now send the IR signal.
#if SEND_MITSUBISHI_AC
//  Serial.println("Sending IR command to A/C ...");
//  ac.send();
#endif  // SEND_MITSUBISHI_AC
//  printState();
//  delay(5000);
}
