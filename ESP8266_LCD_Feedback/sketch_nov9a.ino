#include <ESP8266WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid ="ssid";
const char* password ="some pass";

WiFiServer server(80);

#define BUTTON_PIN D2
#define GREEN_LED  D5
#define RED_LED    D6

String currentTitle = "";
String currentDesc  = "";
String currentFlag  = "";
bool newDataReceived = false;
bool initialDisplayDone = false;

void displayMessage(String msg) {
  msg.replace("%20", " ");
  msg.replace("%A0", " ");
  lcd.clear();
  if (msg.length() <= 16) {
    lcd.setCursor(0, 0);
    lcd.print(msg);
  } else {
    for (int i = 0; i < msg.length() - 15; i++) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(msg.substring(i, i + 16));
      delay(300);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(D4, D3);
  lcd.init();
  lcd.backlight();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  displayMessage("Connecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    displayMessage("Retrying WiFi...");
  }

  lcd.clear();
  lcd.print("IP:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  server.begin();
  Serial.println("Server ready");
}

void handleNewMessage(String message) {
  message.replace("%20", " ");
  message.replace("%A0", " ");
  message.replace("%7C", "|");

  Serial.println("Full message received:");
  Serial.println(message);

  currentTitle = "";
  currentDesc  = "";
  currentFlag  = "";
  initialDisplayDone = false;

  // Show 4 main messages once
  int start = 0, count = 0;
  while (count < 4 && start < message.length()) {
    int sep = message.indexOf('|', start);
    String part = (sep == -1) ? message.substring(start) : message.substring(start, sep);
    part.trim();
    if (part.length() > 0) {
      displayMessage(part);
      delay(2000);
      count++;
    }
    if (sep == -1) break;
    start = sep + 1;
  }

  // Extract Product, Flag, Desc
  if (message.indexOf("Product:") != -1) {
    currentTitle = message.substring(message.indexOf("Product:") + 8);
    int nextPipe = currentTitle.indexOf('|');
    if (nextPipe != -1) currentTitle = currentTitle.substring(0, nextPipe);
    currentTitle.trim();
  }
  if (message.indexOf("Flag:") != -1) {
    currentFlag = message.substring(message.indexOf("Flag:") + 5);
    int nextPipe = currentFlag.indexOf('|');
    if (nextPipe != -1) currentFlag = currentFlag.substring(0, nextPipe);
    currentFlag.trim();
  }
  if (message.indexOf("Desc:") != -1) {
    currentDesc = message.substring(message.indexOf("Desc:") + 5);
    int nextPipe = currentDesc.indexOf('|');
    if (nextPipe != -1) currentDesc = currentDesc.substring(0, nextPipe);
    currentDesc.trim();
  }

  // LED Control
  String flagLower = currentFlag;
  flagLower.toLowerCase();
  if (flagLower == "safe") {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);

  } else if (flagLower == "unsafe") {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  } else {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
  }

  newDataReceived = true;
  initialDisplayDone = true;
}

void handleViewMode() {
  if (!initialDisplayDone) return;

  displayMessage("Flag: " + currentFlag);
  delay(2000);

  // Loop until new data comes
  while (newDataReceived && WiFi.status() == WL_CONNECTED) {
    // 1️ Ask for Title
    displayMessage("View Title?");
    unsigned long t0 = millis();
    bool pressed = false;
    while (millis() - t0 < 8000) { // wait up to 8s
      if (digitalRead(BUTTON_PIN) == LOW) {
        pressed = true;
        break;
      }
      delay(50);
    }
    if (pressed) {
      displayMessage(currentTitle);
      delay(2500);
    }

    // 2️ Ask for Desc
    displayMessage("View Desc?");
    t0 = millis();
    pressed = false;
    while (millis() - t0 < 8000) {
      if (digitalRead(BUTTON_PIN) == LOW) {
        pressed = true;
        break;
      }
      delay(50);
    }
    if (pressed) {
      displayMessage(currentDesc);
      delay(2500);
    }

    // Flag + LED show again before looping
    displayMessage("Flag: " + currentFlag);
    delay(1500);
  }
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    int idx = request.indexOf("/msg=");
    if (idx != -1) {
      String message = request.substring(idx + 5);
      handleNewMessage(message);
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<h2>Message received</h2>");
    delay(1);
    client.stop();
  }

  handleViewMode();
}