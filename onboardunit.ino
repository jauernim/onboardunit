
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Fonts\FreeSans18pt7b.h>
#include <Fonts\FreeSans24pt7b.h>
#include <SimpleTimer.h>
#include <ESP8266HTTPClient.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

#define OLED_RESET 0 // not used
#define LED D4
#define UDP_PORT 1860
#define ICON_UPDATE_BUFFER_LENGTH 5
#define NFC_PIN D8

//const char* ssid = "FRITZ!Box 6360 Cable";
//const char* password = "0425829337054564";
const char* ssid = "wayzNet";
const char* password = "12345678";

const char* transactionURL = "http://192.168.0.1:8080/";

Adafruit_SSD1306 display(OLED_RESET);
WiFiUDP Udp;
SimpleTimer transactionTimer;
HTTPClient http;
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

char iconUpdateBuffer[ICON_UPDATE_BUFFER_LENGTH];
boolean isCheckedIn;

void handleIconUpdate() {
  if (WiFi.status() == WL_CONNECTED) {
    int packetSize = Udp.parsePacket();
    if (packetSize && isCheckedIn) {
      int len = Udp.read(iconUpdateBuffer, sizeof(iconUpdateBuffer));
      if (len > 0) {
        iconUpdateBuffer[len] = 0;
      }
      Serial.printf("Icon update received: %s.\n", iconUpdateBuffer);
      showIcon(iconUpdateBuffer);
    }
  }
}

void showIcon(String icon) {
  display.clearDisplay();
  display.setFont(&FreeSans24pt7b);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(12, 48);
  display.print(icon);
  display.display();
}

void doTransaction(void) {
  digitalWrite(LED, LOW);
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(transactionURL);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Transaction triggered.");
    } else {
      Serial.printf("Triggering transaction failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    delay(50);
  }
  digitalWrite(LED, HIGH);
}

void checkinEvent(void) {
  isCheckedIn = !isCheckedIn;
  if (isCheckedIn) {
    transactionTimer.restartTimer(0);
    transactionTimer.enable(0);
  } else {
    transactionTimer.disable(0);
  }
  delay(50);
}

void displayLogo(void) {
    display.clearDisplay();
    display.setFont(&FreeSans18pt7b);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(14, 48);
    display.print("WAYZ");
    display.display();
}

inline void setupDisplay(void) {
  Wire.setClockStretchLimit(2000);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  displayLogo();  
}

inline void nfcOn(void) {
  digitalWrite(NFC_PIN, HIGH);
}

inline void nfcOff() {
  digitalWrite(NFC_PIN, LOW);  
}

inline void setupNfc(void) {
  delay(500);
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  } else {
    Serial.println("PN53x board found.");
  }
  nfc.SAMConfig();
  nfc.setPassiveActivationRetries(16);
}

inline void setupWifi(void) {
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  digitalWrite(LED, LOW);
  long wifiTimeout = millis() + 10000L;
  while ((WiFi.status() != WL_CONNECTED) && (millis() < wifiTimeout)) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED, HIGH);
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Not connected to WiFi.");
  }
  
}

void setup(void) {
  pinMode(A0, INPUT);
  
  pinMode(NFC_PIN, OUTPUT);
  nfcOff();

  setupDisplay();
  
  pinMode(LED, OUTPUT);
  Serial.begin(115200);

  setupWifi();

  if (WiFi.status() == WL_CONNECTED) {
    Udp.begin(UDP_PORT);
    Serial.println("UDP server started");
  }

  nfcOn();
  setupNfc();
  
  transactionTimer.setInterval(2000L, doTransaction);
  transactionTimer.disable(0);
}

void loop(void) {
  if (analogRead(A0) < 800) {
      ESP.deepSleep(10 * 1000000L, RF_DISABLED);
  }
  
  display.invertDisplay(isCheckedIn);
  if (!isCheckedIn) {
    displayLogo();
  }
  boolean isCheckinEvent = false;
  while (nfc.inListPassiveTarget() == true) {
    isCheckinEvent = true;
  }
  if (isCheckinEvent) {
    checkinEvent();
  }

  handleIconUpdate();
  transactionTimer.run();
}

