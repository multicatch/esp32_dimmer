#include <esp32-hal-ledc.h>
#include <Matter.h>
#include <WiFi.h>

#define LED_BUILTIN 2

//// WIFI settings
const char *ssid = "ssid";          // Change this to your WiFi SSID
const char *password = "pass";      // Change this to your WiFi password

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
//const int maxLevel = 255;
//const int minLevel = 0;

bool onState = false;
uint8_t brightness = 255;

//// MATTER 
MatterDimmableLight DimmableLight;

// Matter Callbacks
bool setLightOnOff(bool newState) {
  Serial.printf("User Callback :: Switch Light = %s\r\n", newState ? "ON" : "OFF");
  if (newState != onState) {
    if (newState) {
      ledcWrite(ledPin, brightness);
    } else {
      ledcWrite(ledPin, 0);
    }
  }
  onState = newState;
  
  return true;
}

bool setBrightness(uint8_t newBrightness) {
  Serial.printf("User Callback :: Brightness = %d\r\n", newBrightness);

  brightness = newBrightness;
  if (!onState) {
    ledcWrite(ledPin, brightness);
  }

  return true;
}

bool setLight(bool newState, uint8_t newBrightness) {
  setBrightness(newBrightness);
  setLightOnOff(newState);
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
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      delay(450);
      if ((timeCount++ % 50) == 0) {  // 50*600ms = 30 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }

    DimmableLight.updateAccessory();
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
}

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

// Setup
void setup() {
  // initial setup
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  ledcAttachChannel(ledPin, pwmFreq, pwmRes, pwmChannel);

  setupWifi();

  happyBlink();

  // Matter
  DimmableLight.begin(onState, brightness);
  DimmableLight.onChange(setLight);
  DimmableLight.onChangeOnOff(setLightOnOff);
  DimmableLight.onChangeBrightness(setBrightness);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    DimmableLight.updateAccessory(); 
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

  // refresh ledc
  if (onState) {
    ledcWrite(ledPin, brightness);
  } else {
    ledcWrite(ledPin, 0);
  }
}
