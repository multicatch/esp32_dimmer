#include <esp32-hal-ledc.h>
#include <Matter.h>
#include <WiFi.h>

#define LED_BUILTIN 2

// uncomment this if you use external file with wifi password
//#include "wifi_password.h"

#ifndef WIFI_PASSWORD_H
//// WIFI settings
// You can also use external "wifi_password.h" file with those constants
// Just remember to add #define WIFI_PASSWORD_H

const char *ssid = "ssid";          // Change this to your WiFi SSID
const char *password = "pass";      // Change this to your WiFi password
#endif

const int wifiReconnectInterval = 60000; // 1min
unsigned long lastWifiConnect = 0;

bool wifiConnected = false;

void setupWifi() {
  if (wifiConnected) {
    Serial.print("Disconnecting WiFi");
    WiFi.disconnect();
  }

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  lastWifiConnect = millis();
  while (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    for (int i = 0; i < 2; i++) {
      digitalWrite(LED_BUILTIN, i % 2 == 1 ? HIGH : LOW);
      delay(250);
    }
    digitalWrite(LED_BUILTIN, LOW);
    Serial.print(".");

    if ((millis() - lastWifiConnect) > wifiReconnectInterval) {
      return;
    }
  }

  Serial.println("\r\nWiFi connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  wifiConnected = true;
}

//// PWM dimming settings
const int ledPin = 23;
const int pwmChannel = 0;
const int pwmFreq = 5000;
const int pwmRes = 8;

// LED settings
const float dimCurve = 2.6f;
const int dimDelay = 4;

bool onState = false;
uint8_t brightness = 0;
uint8_t targetBrightness = 0;

//// LED helpers
uint8_t calculateAdjustedBrightness(const uint8_t brightness, const float dimCurve) {
  float xn = brightness / 255.0;
  float adjusted = 255.0 * pow(xn, dimCurve);
  uint8_t newBrightness = (uint8_t) adjusted;
  if (brightness > 0 && newBrightness == 0) {
    newBrightness = 1;
  }
  return newBrightness;
}

uint16_t currentlyAnimated = 0;
void ledcSmoothFade(uint8_t pin, uint8_t& brightness, const uint8_t& targetBrigthness) {
  uint16_t pinBit = 1 << pin;
  bool duringAnimation = currentlyAnimated & 1 << pin;
  bool isCorrectBrightness = targetBrightness == brightness;
  bool isOffAsRequested = !onState && brightness == 0;
  if (duringAnimation || isCorrectBrightness || isOffAsRequested) {
    return;
  }
  currentlyAnimated |= pinBit;

  Serial.printf("Smoothly fading brightness = %d..%d,, Adjusted Brightness = %d, Curve = %f\r\n", brightness, targetBrightness, calculateAdjustedBrightness(targetBrightness, dimCurve), dimCurve);
  Serial.printf("Fading... ");

  while ((onState && brightness != targetBrightness) || (!onState && brightness > 0)) {
    uint8_t nextStep = brightness;
    uint8_t target = onState ? targetBrightness : 0;
    if (target > nextStep) {
      nextStep += 1;
    } else {
      nextStep -= 1;
    }

    brightness = nextStep;
    //Serial.printf("%d -> %d, ", nextStep, target);

    uint8_t adjustedStep = calculateAdjustedBrightness(nextStep, dimCurve);
    ledcWrite(pin, adjustedStep);
    delay(dimDelay);

    if (brightness == 0 && !onState) {
      break;
    }
  }

  Serial.println("done!");
  currentlyAnimated &= (0xFFFF ^ pinBit);
}

//// MATTER 
MatterDimmableLight dimmableLight;

// Matter Callbacks
bool setLightOnOff(bool newState) {
  Serial.printf("User Callback :: Switch Light = %s\r\n", newState ? "ON" : "OFF");
  onState = newState;
  return true;
}

bool setBrightness(uint8_t newBrightness) {
  Serial.printf("User Callback :: Brightness = %d\r\n", newBrightness);
  targetBrightness = newBrightness;
  return true;
}

bool setLight(bool newState, uint8_t newBrightness) {
  Serial.printf("User Callback :: Switch = %d AND Brightness = %d\r\n", newState, newBrightness);
  // For some reason, when we turn on/off the light it will get newBrightness = 1
  // This behavior really breaks the smooth fading 
  if (newState != onState) {
    setLightOnOff(newState);
  } else {
    setBrightness(newBrightness);
  }
  return true;
}

void commissionMatter() {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());

    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      delay(50);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      delay(450);
      if ((timeCount++ % 50) == 0) {  // 50*600ms = 30 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }

    dimmableLight.updateAccessory();
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
}


// Setup
void happyBlink() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(75);
  }
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
  // initial setup
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  ledcAttachChannel(ledPin, pwmFreq, pwmRes, pwmChannel);
  ledcWrite(ledPin, 0);

  setupWifi();

  happyBlink();

  // Matter
  dimmableLight.begin(onState, brightness);
  dimmableLight.onChange(setLight);
  dimmableLight.onChangeOnOff(setLightOnOff);
  dimmableLight.onChangeBrightness(setBrightness);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    dimmableLight.updateAccessory(); 
  }
}

void loop() {
  if (!Matter.isDeviceCommissioned()) {
    commissionMatter();
  }

  boolean didWifiDisconnect = WiFi.status() != WL_CONNECTED 
        && (millis() - lastWifiConnect) > (wifiReconnectInterval + 100); // +100ms to prevent race condition with setupWifi()
  if (didWifiDisconnect) {
    Serial.println("Lost WiFi connection. Attempting a reconnect...");
    setupWifi();
  }

  ledcSmoothFade(ledPin, brightness, targetBrightness);
}
