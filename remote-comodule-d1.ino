#include <ESP8266WiFi.h>
#include <IRremote.hpp>
#include <MQTT.h>

#include "secrets.h"

#define IR_RECEIVE_PIN D7
#define HID_INTERCEPT_PIN D4

WiFiClient net;
MQTTClient client;

void connectMqtt() {
  while (!client.connect("remote-comodule-d1", MQTT_USER, MQTT_PASS)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Connected");
}

void setup() {
  pinMode(HID_INTERCEPT_PIN, OUTPUT);
  digitalWrite(HID_INTERCEPT_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

	Serial.begin(9600);
  pinMode(IR_RECEIVE_PIN, INPUT_PULLUP);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Connecting to MQTT");
  client.begin(MQTT_HOST, net);
  connectMqtt();
}

unsigned long lastPwrSignal = 0;
bool intercepting = false;

void loop() { 
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.decodedRawData == 0x854) {
      if (millis() - lastPwrSignal > 500) {
        Serial.println("PWR");
        intercepting = !intercepting;
        digitalWrite(HID_INTERCEPT_PIN, intercepting ? HIGH : LOW);
        if (client.connected()) {
          client.publish("/testing-remote", "PWR");
        }
      }
      lastPwrSignal = millis();
    }
    IrReceiver.resume();
  }

  client.loop();
  if (!client.connected()) {
    connectMqtt();
  }

  delay(1);
}