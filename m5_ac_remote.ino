#include <M5StickC.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Mitsubishi.h>
#include "m5_ac_remote.h"

WiFiClientSecure httpsClient;
PubSubClient mqttClient(httpsClient);
char pubMessage[1024];

const uint16_t kIrLed = 9;  // M5StickC IR
IRMitsubishiAC ac(kIrLed);  // Set the GPIO used for sending messages.

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

void displayACRemote() {
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

void setup() {
  // put your setup code here, to run once:
  M5.begin();
  Wire.begin(0,26);
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.println("M5 AC Remote");

  ac.begin();
  Serial.begin(115200);
  delay(200);

  setup_wifi();
  setup_mqtt();

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

  delay(1);

  // Now send the IR signal.
#if SEND_MITSUBISHI_AC
//  Serial.println("Sending IR command to A/C ...");
//  ac.send();
#endif  // SEND_MITSUBISHI_AC
//  printState();
//  delay(5000);
}
