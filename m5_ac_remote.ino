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
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
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

WiFiMulti wifiMulti;
// InfluxDB client instance
InfluxDBClient influxdbClient;
//InfluxDBClient influxdbClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
// Data point
Point acRemote("ac_remote");

const uint16_t kIrLed = 9;  // M5StickC IR
IRMitsubishiAC ac(kIrLed);  // Set the GPIO used for sending messages.

#ifdef USE_ENV2
Adafruit_SHT31 sht31;
#else
DHT12 dht12;
#endif
Adafruit_BMP280 bme;

void setupWifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void checkWifiReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    setupWifi();
  }
}

void setupMqtt() {
  httpsClient.setCACert(ROOT_CA);
  httpsClient.setCertificate(CLIENT_CERTIFICATE);
  httpsClient.setPrivateKey(CLIENT_PRIVATE_KEY);
  mqttClient.setServer(MQTT_ENDPOINT, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void connectMqtt() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    uint64_t chipId = ESP.getEfuseMac();
    String clientId = "ESP32Client-" + String((uint32_t)(chipId & 0xFFFFFFFF), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      mqttClient.publish("outTopic", "hello world");
      mqttClient.subscribe(MQTT_SHADOW_UPDATE_DELTA_TOPIC);
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Will retry in 5 seconds");
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived at ");
  Serial.print(topic);
  Serial.print(": ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  const size_t capacity = 3*JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + 60;
  DynamicJsonDocument doc(capacity);

  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("JSON deserialization failed: ");
    Serial.println(error.f_str());
    return;
  }

  JsonObject state = doc["state"];

  if (state.containsKey("power")) {
    ac.setPower(state["power"]);
  }
  if (state.containsKey("mode")) {
    ac.setMode(state["mode"]);
  }
  if (state.containsKey("temp")) {
    ac.setTemp(state["temp"]);
  }

  ac.send();
  printState();
  displayACRemote();
  publishDeviceShadowReported();
}

void mqttPublish(const char* topic, const char* payload) {
  Serial.print("Publish message to ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(payload);
  if (mqttClient.publish(topic, payload)) {
    Serial.println("Published successfully.");
  } else {
    Serial.println("Publish failed.");
  }
}

void publishDeviceShadowReported() {
  sprintf(pubMessage, "{\"state\": {\"reported\": {\"power\": %d,\"mode\": %d,\"temp\": %d}}}", ac.getPower(), ac.getMode(), ac.getTemp());
  mqttPublish(MQTT_SHADOW_UPDATE_TOPIC, pubMessage);
}

void publishDeviceShadowDesired() {
  sprintf(pubMessage, "{\"state\": {\"desired\": {\"power\": %d,\"mode\": %d,\"temp\": %d}}}", ac.getPower(), ac.getMode(), ac.getTemp());
  mqttPublish(MQTT_SHADOW_UPDATE_TOPIC, pubMessage);
}

void printState() {
  // Display the settings.
  Serial.println("Mitsubishi A/C remote is in the following state:");
  Serial.printf("  %s\n", ac.toString().c_str());

  // Display the encoded IR sequence.
  unsigned char* irCode = ac.getRaw();
  Serial.print("IR Code: 0x");
  for (uint8_t i = 0; i < kMitsubishiACStateLength; i++)
    Serial.printf("%02X", irCode[i]);
  Serial.println();
}

int dispMode = 0; // Display mode for AC Remote

void displayACRemote() {
  if (dispMode != 0) {
    return; // Exit if display mode is not set to AC Remote
  }
  
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.println("M5 AC Remote");

  // Display power status
  bool power = ac.getPower();
  M5.Lcd.setCursor(0, 20, 2);
  M5.Lcd.printf("Power: %s", power ? "ON" : "OFF");

  // Display mode setting
  char *mode;
  switch (ac.getMode()) {
  case kMitsubishiAcCool: // Cool mode
    mode = "Cool";
    break;
  case kMitsubishiAcDry: // Dry mode
    mode = "Dry";
    break;
  case kMitsubishiAcHeat: // Heat mode
    mode = "Heat";
    break;
  default:
    mode = "Unknown";
    break;
  }
  M5.Lcd.setCursor(0, 40, 2);
  M5.Lcd.printf("Mode: %s", mode);

  // Display temperature setting
  uint8_t temp = ac.getTemp();
  M5.Lcd.setCursor(0, 60, 2);
  M5.Lcd.printf("Temperature: %2d C", temp);
}

void setupTime() {
  configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
}

void setupInfluxDB() {
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  influxdbClient.setConnectionParams(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

  uint64_t chipId = ESP.getEfuseMac();
  String clientId = "ESP32Client-" + String((uint32_t)(chipId & 0xFFFFFFFF), HEX);

  // Add tags
  acRemote.addTag("client_id", clientId);
}

void connectInfluxDB() {
  if (influxdbClient.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(influxdbClient.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(influxdbClient.getLastErrorMessage());
  }
}

void setup() {
  // put your setup code here, to run once:
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);

  Serial.begin(115200);
  delay(200);

  // Initialize the AC remote
  ac.begin();

  // Initialize I2C for sensors
  Wire.begin(32,33);

#ifdef USE_ENV2
  // Initialize the SHT31 sensor if defined
  if (!sht31.begin(0x44)) {
    Serial.println("Could not find a valid SHT31 sensor, check wiring!");
    while (1);
  }
#endif

  // Initialize the BMP280 sensor
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1);
  }

  // Setup WiFi, MQTT, and Time
  setupWifi();
  setupMqtt();
  setupInfluxDB();
  setupTime();

  // Initialize the A/C remote to a default state
  Serial.println("Default state of the remote.");
  printState();

  Serial.println("Setting desired state for A/C.");
  unsigned char* irCode = ac.getRaw();
  irCode[14] = 0x04;  // Set internal clean mode
  ac.setRaw(irCode);
  ac.off();
  ac.setFan(kMitsubishiAcFanAuto);
  ac.setMode(kMitsubishiAcCool);
  ac.setTemp(28);
  ac.setVane(kMitsubishiAcVaneAuto);
  ac.setWideVane(kMitsubishiAcWideVaneAuto);

  printState();
  displayACRemote();

  // Connect to MQTT and publish device shadow
  connectMqtt();
  delay(500); // Wait for a while after subscribing to "/update/delta" before publishing "/update"
  publishDeviceShadowReported();

  connectInfluxDB();
}

float temperature;
float humidity;
float pressure;
const unsigned long DETECT_INTERVAL = 1000; // 1 second
const unsigned long UPLOAD_INTERVAL = 600000; // 10 minutes
unsigned long currentTime = 0;
unsigned long detectTime = 0;
unsigned long uploadTime = 0;

void displayEnvMonitor() {
  if (dispMode == 0) {
    return; // Exit if display mode is not set to ENV Monitor
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

  // Check Wi-Fi connectivity and reconnect if necessary
  checkWifiReconnect();

  // Ensure MQTT client is connected
  if (!mqttClient.connected()) {
    connectMqtt();
  }
  mqttClient.loop(); // Process incoming messages

  // Handle AC power toggle with Button A
  if (M5.BtnA.wasPressed()) {
    if (ac.getPower()) {
      ac.off();
    } else {
      ac.on();
    }
    publishDeviceShadowDesired();
  }

  // Handle display mode toggle with Button B
  if (M5.BtnB.wasPressed()) {
    M5.Lcd.fillScreen(BLACK);
    dispMode = (dispMode == 0) ? 1 : 0; // Toggle between Env Monitor and AC Remote
    if (dispMode == 0) {
      displayACRemote();
    } else {
      displayEnvMonitor();
    }
  }

  // Read environmental data at specified intervals
  currentTime = millis();
  if ((long)(detectTime - currentTime) < 0) {
#ifdef USE_ENV2
    temperature = sht31.readTemperature();
    humidity = sht31.readHumidity();
#else
    temperature = dht12.readTemperature();
    humidity = dht12.readHumidity();
#endif
    pressure = bme.readPressure() / 100.0F;

    displayEnvMonitor(); // Update the display with environmental data

    detectTime = currentTime + DETECT_INTERVAL;

    // Check if it's time to upload data
    if ((long)(uploadTime - currentTime) < 0) {
      struct tm timeInfo;
      getLocalTime(&timeInfo); // Get current time

      char time[50];
      sprintf(time, "%04d/%02d/%02d %02d:%02d:%02d",
        timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
        timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

      Serial.printf("Time: %s\n", time);
      Serial.printf("Temperature: %2.1f C\n", temperature);
      Serial.printf("Humidity: %2.0f %%\n", humidity);
      Serial.printf("Pressure: %4.1f hPa\n", pressure);

      // Prepare JSON data
//      const size_t capacity = JSON_OBJECT_SIZE(4);
//      DynamicJsonDocument doc(capacity);
      DynamicJsonDocument doc(192);
      doc["time"] = time;
      doc["temperature"] = temperature;
      doc["humidity"] = humidity;
      doc["pressure"] = pressure;
      doc["ac_power"] = (int)ac.getPower();
      doc["ac_mode"] = ac.getMode();
      doc["ac_temp"] = ac.getTemp();

      String message_body;
      serializeJson(doc, message_body);
      Serial.println(message_body);
      mqttPublish(MQTT_SENSOR_DATA_TOPIC, message_body.c_str()); // Publish data via MQTT

      acRemote.clearFields();
      acRemote.addField("temperature", temperature);
      acRemote.addField("humidity", humidity);
      acRemote.addField("pressure", pressure);
      acRemote.addField("ac_power", (int)ac.getPower());
      acRemote.addField("ac_mode", ac.getMode());
      acRemote.addField("ac_temp", ac.getTemp());
      Serial.print("Writing: ");
      Serial.println(influxdbClient.pointToLineProtocol(acRemote));
      // If no Wifi signal, try to reconnect it
      if (wifiMulti.run() != WL_CONNECTED) {
        Serial.println("Wifi connection lost");
      }
      // Write point
      if (!influxdbClient.writePoint(acRemote)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(influxdbClient.getLastErrorMessage());
      }

      uploadTime = currentTime + UPLOAD_INTERVAL;
    }
  }

  delay(1);

  // Now send the IR signal.
#if SEND_MITSUBISHI_AC
//  Serial.println("Sending IR command to A/C ...");
//  ac.send();
#endif  // SEND_MITSUBISHI_AC
}
