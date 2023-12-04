#include <ESP8266WiFi.h>
#include <IRremote.hpp>
#include <MQTT.h>
#include <Wire.h>

#include "secrets.h"

#define IR_RECEIVE_PIN D0
#define TX_PIN D4
#define RX_PIN D7

WiFiClient net;
MQTTClient client;

void connectMqtt() {
  while (!client.connect("remote-comodule-d1", MQTT_USER, MQTT_PASS)) {
    delay(500);
  }
  client.publish("/air-remote-debug", "CONNECTED");
}

void setup() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);

	Serial.begin(19200);
  Wire.begin();

  pinMode(IR_RECEIVE_PIN, INPUT_PULLUP);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  client.begin(MQTT_HOST, net);
  connectMqtt();
  client.publish("/air-remote-debug", "SETUP DONE");
}

unsigned long lastPwrSignal = 0;

char buf[128];

void loop() { 
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.decodedRawData == 0x854) {
      if (millis() - lastPwrSignal > 500) {
        if (client.connected()) {
          client.publish("/air-remote-debug", "PWR BTN");
        }
      }
      lastPwrSignal = millis();
    }
    IrReceiver.resume();
  }

  // if (Serial.available() > 0) {
  //   auto bytesAvail = Serial.available();
  //   //auto bytesRead = Serial.readBytesUntil('\n', serialBuf, sizeof(serialBuf));
  //   auto bytesRead = Serial.readBytes(serialBuf, sizeof(serialBuf));
  //   if (bytesRead > 0 && serialBuf[0] == 'H') {
  //     client.publish("/air-remote-debug", serialBuf, bytesRead);
  //   } else {
  //     char buffer[40];
  //     sprintf(buffer, "RX FAIL, AVAIL %u, READ %u, FIRST CHAR %u", bytesAvail, bytesRead, serialBuf[0]);
  //     client.publish("/air-remote-debug", buffer);
  //   }
  // }

  Wire.beginTransmission(1);
  Wire.write("PING", 4);
  Wire.endTransmission();

  Wire.requestFrom(0x05, 4);
  unsigned int lenRead = 0;
  while (Wire.available()) {
    if (lenRead >= 128) {
      lenRead = 0;
      Serial.println("OVERFLOW");
      client.publish("/air-remote-debug", "Buffer overflow");
      break;
    }
    char c = Wire.read();
    buf[lenRead] = c;
    lenRead++;
  }
  if (lenRead > 0) {
    buf[lenRead+1] = NULL;
    Serial.println(buf);
    client.publish("/air-remote-debug", buf, lenRead);
  }

  client.loop();
  if (!client.connected()) {
    Serial.println("NOT ONLINE?");
    connectMqtt();
  }

  delay(1000);
}