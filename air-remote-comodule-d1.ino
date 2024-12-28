#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <IRremote.hpp>
#include <MQTT.h>
#include <Wire.h>

#include "secrets.h"

/*
 * Event types to receive:
 * A - ASCII key
 * C - Consumer control scan code
 * K - Keyboard scan code (for non-ASCII keys)
 * N - Network connected
 * O - OK button
 * U - USB connection state change
 * W - Power button
 */

#define IR_RECEIVE_PIN D0
#define TX_PIN D4
#define RX_PIN D7
#define UDP_PORT 7765

#define PASSTHRU_SETTING_TOPIC "air-remote/passthru-setting"
#define USB_POWER_ON_TOPIC "air-remote/usb-power-on"

WiFiClient net;
WiFiUDP Udp;
MQTTClient client;

bool currentPassthruFlag = true;
bool intendedPassthruFlag = true;
unsigned long lastPwrSignal = 0;
char outputBuf[64];
char udpPacketBuf[255];
char pendingUsbPowerOn = false;

void writePassthru(bool value) {
  Wire.beginTransmission(0x05);
  Wire.write(value ? 'P' : 'p');
  Wire.endTransmission();
}

void writeUsbPowerOn() {
  Wire.beginTransmission(0x05);
  Wire.write('R');
  Wire.endTransmission();
}

void writeGamepad(char* data, int len) {
  Wire.beginTransmission(0x05);
  Wire.write('G');
  for (int i = 0; i < len; ++i) {
    Wire.write(data[i]);
  }
  Wire.endTransmission();
}

void connectMqtt() {
  // If we lose network connection, fall back to passthru mode
  intendedPassthruFlag = true;
  writePassthru(true);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  while (!client.connect("remote-comodule-d1", MQTT_USER, MQTT_PASS)) {
    delay(500);
  }

  client.subscribe(PASSTHRU_SETTING_TOPIC);
  client.subscribe(USB_POWER_ON_TOPIC);
  client.publish("air-remote/events", "{ \"event\": \"N\" }");
}

void messageReceived(String &topic, String &payload) {
  if (topic == PASSTHRU_SETTING_TOPIC) {
    intendedPassthruFlag = payload == "ON";
  } else if (topic == USB_POWER_ON_TOPIC) {
    pendingUsbPowerOn = true;
  }
}

void setup() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);

	Serial.begin(19200);
  Wire.begin();

  pinMode(IR_RECEIVE_PIN, INPUT_PULLUP);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  client.begin(MQTT_HOST, net);
  client.onMessage(messageReceived);

  Udp.begin(UDP_PORT);
}

void loop() {
  client.loop();
  delay(10);

  if (!client.connected()) {
    connectMqtt();
  }

  if (currentPassthruFlag != intendedPassthruFlag) {
    writePassthru(intendedPassthruFlag);
    currentPassthruFlag = intendedPassthruFlag;
  }

  if (pendingUsbPowerOn) {
    writeUsbPowerOn();
    pendingUsbPowerOn = false;
  }

  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.decodedRawData == 0x854) {
      if (millis() - lastPwrSignal > 500 && client.connected()) {
        client.publish("air-remote/events", "{ \"event\": \"W\" }");
      }
      lastPwrSignal = millis();
    }
    IrReceiver.resume();
  }

  Wire.requestFrom(0x05, 2);
  if (Wire.available()) {
    uint8_t kind = Wire.read();
    if (Wire.available()) {
      uint8_t data = Wire.read();
      if (kind >= 'A' && kind <= 'Z' && client.connected()) {
        int len = sprintf(outputBuf, "{ \"event\": \"%c\", \"data\": \"0x%02X\" }", kind, data);
        client.publish("air-remote/events", outputBuf, len);
      }
    }
  }

  int packetSize = Udp.parsePacket();
  if (packetSize) {
    int len = Udp.read(udpPacketBuf, 255);
    writeGamepad(udpPacketBuf, len);
  }
}