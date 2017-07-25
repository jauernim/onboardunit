
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
//#include <Adafruit_PN532.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

#define OLED_RESET 0 // not used
#define LED D4
#define UDP_PORT 1860
#define ICON_UPDATE_BUFFER_LENGTH (SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8)
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
//Adafruit_PN532 nfc(D6, D7);
PN532_I2C pn532_i2c(Wire);
PN532 nfc(pn532_i2c);

char iconUpdateBuffer[ICON_UPDATE_BUFFER_LENGTH];
boolean isCheckedIn = false;

void handleIconUpdate() {
  if (WiFi.status() == WL_CONNECTED) {
    int packetSize = Udp.parsePacket();
    if (packetSize && isCheckedIn) {
      int len = Udp.read(iconUpdateBuffer, sizeof(iconUpdateBuffer));
      if (len > 0) {
        iconUpdateBuffer[len] = 0;
      }
      Serial.println("Icon update received.");
      showIcon(iconUpdateBuffer);
    }
  }
}

void showIcon(String icon) {
  display.clearDisplay();
/*
  display.setFont(&FreeSans24pt7b);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(12, 48);
  display.print(icon);
*/
  display.drawBitmap(0, 0, (const unsigned char*) iconUpdateBuffer, 64, 64, WHITE);
  display.display();
}

void doTransaction(void) {
  digitalWrite(LED, LOW);
  display.fillCircle(display.width() - 5, 5, 4, WHITE);
  display.display();
  
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
  display.fillCircle(display.width() - 5, 5, 4, BLACK);
  display.display();
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
  delay(20);
  Wire.beginTransmission(0x48 >> 1);
  if (Wire.endTransmission() == 0) {
    Serial.println("NFC found on I2C");
  } else {
    Serial.println("NFC not found on I2C");
  }
}

inline void nfcOff() {
  digitalWrite(NFC_PIN, LOW);  
}

inline void setupNfc(void) {
  int version = 0;
  nfc.begin();
  version = nfc.getFirmwareVersion();
  if (version == 0) {
    Serial.println("Didn't find PN53x board");
    nfcOff();
    checkinEvent();
  } else {
    Serial.println("PN53x board found.");
    nfc.SAMConfig();
    nfc.setPassiveActivationRetries(16);
  }
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

  transactionTimer.setInterval(2000L, doTransaction);
  transactionTimer.disable(0);

  nfcOn();
  delay(500);
  setupNfc();
}

void loop(void) {
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

