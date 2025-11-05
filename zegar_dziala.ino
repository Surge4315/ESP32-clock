#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

// Piny do I2C OLED
#define SCREEN_SDA 21
#define SCREEN_SCL 22

const int buttonPin = 18;
int hour = 3600;
int offset = 2 * hour;

volatile int clockCounter = 0;

volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 200; // 200 ms


// WiFi
const char* ssid = "";
const char* password = "";

// OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCREEN_SCL, SCREEN_SDA);

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 3600000);

// Przerwanie
void IRAM_ATTR handleButtonPress() {
    unsigned long currentTime = millis();
    if (currentTime - lastInterruptTime > debounceDelay) {
        buttonPressed = true;
        lastInterruptTime = currentTime;
    }
}


void setup() {
    Serial.begin(115200);
    Wire.begin(SCREEN_SDA, SCREEN_SCL);
    pinMode(buttonPin, INPUT_PULLUP);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    u8g2.begin();
    timeClient.begin();

    attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonPress, RISING);
}

void loop() {
    if (buttonPressed) {
    buttonPressed = false;  // zresetuj flagę

    clockCounter = (clockCounter + 1) % 7;

    switch (clockCounter) {
        case 0: offset = 2 * hour; break;       // Cracow
        case 1: offset = 3 * hour; break;       // Pitesti
        case 2: offset = 8 * hour; break;       // Manila
        case 3: offset = 9 * hour; break;       // Tokyo
        case 4: offset = -7 * hour; break;      // Los Angeles
        case 5: offset = -5 * hour; break;      // New Orleans
        case 6: offset = hour; break;           // London
        default: offset = 0; break;             // Greenwich
    }
}

    timeClient.setTimeOffset(offset);
    timeClient.update();

    unsigned long currentEpoch = timeClient.getEpochTime();
    time_t currentTime = (time_t)currentEpoch;
    struct tm* timeInfo = localtime(&currentTime);

    int hours = timeInfo->tm_hour;
    int minutes = timeInfo->tm_min;
    int seconds = timeInfo->tm_sec;
    int day = timeInfo->tm_mday;
    int month = timeInfo->tm_mon + 1;
    int year = timeInfo->tm_year + 1900;

    String daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    String locations[] = {"Cracow", "Pitesti", "Manila", "Tokyo", "Los Angeles", "New Orleans", "London"};
    String dayOfWeek = daysOfWeek[timeInfo->tm_wday];
    String location = locations[clockCounter];

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB18_tr);
    String timeStr = String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);
    int textWidth = u8g2.getStrWidth(timeStr.c_str());
    u8g2.setCursor((128 - textWidth) / 2, 24);
    u8g2.print(timeStr);

    u8g2.setFont(u8g2_font_ncenB10_tr);
    String dateStr = (day < 10 ? "0" : "") + String(day) + "/" + (month < 10 ? "0" : "") + String(month) + "/" + String(year) + " " + dayOfWeek;
    int dateTextWidth = u8g2.getStrWidth(dateStr.c_str());
    u8g2.setCursor((128 - dateTextWidth) / 2, 40);
    u8g2.print(dateStr);

    int cityWidth = u8g2.getStrWidth(location.c_str());
    u8g2.setCursor((128 - cityWidth) / 2, 56);
    u8g2.print(location);

    u8g2.sendBuffer();
    delay(100);
}
