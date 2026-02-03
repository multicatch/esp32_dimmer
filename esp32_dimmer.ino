#include <esp32-hal-ledc.h>
#include <Matter.h>
#include <WiFi.h>

//// WIFI settings
const char *ssid = "ssid";          // Change this to your WiFi SSID
const char *password = "pass";      // Change this to your WiFi password


#define LED_BUILTIN 2

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


// Setup
void setup() {
  // initial setup
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  ledcAttachChannel(ledPin, pwmFreq, pwmRes, pwmChannel);

  // wifi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < 2; i++) {
      digitalWrite(LED_BUILTIN, i % 2 == 1 ? HIGH : LOW);
      delay(250);
    }
    Serial.print(".");
  }

  Serial.println("\r\nWiFi connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(75);
  }
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);

  // matter

  DimmableLight.begin(onState, brightness);
  DimmableLight.onChange(setLight);
  DimmableLight.onChangeOnOff(setLightOnOff);
  DimmableLight.onChangeBrightness(setBrightness);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    DimmableLight.updateAccessory();  // configure the Light based on initial state
  }
}

void loop() {
  // Check Matter Light Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Light Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.println("Matter Node not commissioned yet. Waiting for commissioning.");
      }
    }
    DimmableLight.updateAccessory();  // configure the Light based on initial state
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  if (onState) {
    ledcWrite(ledPin, brightness);
  } else {
    ledcWrite(ledPin, 0);
  }
}
