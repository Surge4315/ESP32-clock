#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

// Piny do I2C OLED
#define SCREEN_SDA 21
#define SCREEN_SCL 22

const int buttonPin = 0;
int hour = 3600;
int offset = 2 * hour; // base time offset GMT+2 for Cracow
int UStime = 0;

volatile int clockCounter = 0;

volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 200; // 200 ms

// WiFi
const char *ssid = "";
const char *password = "";

// OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCREEN_SCL, SCREEN_SDA);

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); //update every 60 seconds

// last sunday of march and october
bool EUSummerTime(int day, int month, int wday, int hour)
{
    // wday: 0 = sunday, 1 = monday, ..., 6 = saturday
    // hour: 0..23 (UTC)

    if (month < 3 || month > 10)
        return false; // january–february and november–december -> winter time
    if (month > 3 && month < 10)
        return true; // april–september -> summer time

    int lastSunday;
    if (month == 3)
    {
        // last sunday of march
        // takes day of month
        // subtracts it from 31 to get days left in month
        // adds weekday to that to get last weekday of month
        // because if we have a weekday and add days to it and modulo by 7 we just get next weekdays
        // mods by 7 to get offset to last sunday
        // subtracts offset from 31 to get date of last sunday
        lastSunday = 31 - ((wday + (31 - day)) % 7);
        // DST starts at 01:00 UTC on last Sunday of March
        if (day > lastSunday)
            return true;
        if (day < lastSunday)
            return false;
        return hour >= 1; // on last Sunday, summer time starts at 01:00 UTC
    }
    else
    { // month == 10
        // last sunday of october
        lastSunday = 31 - ((wday + (31 - day)) % 7);
        // DST ends at 01:00 UTC on last Sunday of October
        if (day < lastSunday)
            return true;
        if (day > lastSunday)
            return false;
        return hour < 1; // on last Sunday, summer time ends at 01:00 UTC
    }
}

// second sunday of march to first sunday of november
bool USSummerTime(int day, int month, int wday, int hour)
{
    if (month < 3 || month > 11)
        return false; // Jan, Feb, Dec -> standard time
    if (month > 3 && month < 11)
        return true;  // Apr–Oct -> daylight time

    int firstSunday, secondSunday;

    if (month == 3)
    {
        // find weekday of March 1
        int march1_wday = (wday - ((day - 1) % 7) + 7) % 7;
        firstSunday = (march1_wday == 0) ? 1 : (8 - march1_wday);
        secondSunday = firstSunday + 7;

        if (day > secondSunday) return true;
        if (day < secondSunday) return false;
        return hour >= 2; // DST starts at 2:00 AM local time
    }
    else // month == 11
    {
        // find weekday of November 1
        int nov1_wday = (wday - ((day - 1) % 7) + 7) % 7;
        firstSunday = (nov1_wday == 0) ? 1 : (8 - nov1_wday);

        if (day < firstSunday) return true;
        if (day > firstSunday) return false;
        return hour < 2; // DST ends at 2:00 AM local time
    }
}

// interrupt handler for button press with debounce
void IRAM_ATTR handleButtonPress()
{
    unsigned long currentTime = millis();
    if (currentTime - lastInterruptTime > debounceDelay)
    {
        buttonPressed = true;
        lastInterruptTime = currentTime;
    }
}

void setup()
{
    Serial.begin(115200);
    Wire.begin(SCREEN_SDA, SCREEN_SCL);
    pinMode(buttonPin, INPUT_PULLUP);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    u8g2.begin();
    timeClient.begin();

    attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonPress, RISING);
}

void loop()
{
    // starting with no offset to get current time in UTC
    timeClient.setTimeOffset(0);
    timeClient.update();
    unsigned long currentEpoch = timeClient.getEpochTime();
    time_t currentTime = (time_t)currentEpoch;
    struct tm *timeInfo = gmtime(&currentTime);

    int EUtime = 0;
    if (EUSummerTime(timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_wday, timeInfo->tm_hour))
    {
        EUtime = 0; // if summertime than no change
    }
    else
    {
        EUtime = hour; // if no summertime then we subtract one hour
    }

   
        if (buttonPressed)
    {
        buttonPressed = false; // flag reset

        clockCounter = (clockCounter + 1) % 7;
    }

    switch (clockCounter)
    {
    case 0:
        offset = 2 * hour - EUtime; // Cracow
        break;
    case 1:
        offset = 3 * hour - EUtime; // Pitesti
        break;
    case 2:
        offset = 8 * hour; // Manila
        break;
    case 3:
        offset = 9 * hour; // Tokyo
        break;
    case 4:
        offset = -7 * hour - UStime; // Los Angeles
        break;
    case 5:
        offset = -5 * hour - UStime; // New Orleans
        break;
    case 6:
        offset = hour - EUtime; // London
        break;
    default:
        offset = 0; // Greenwich Mean Time
        break;
    }
    timeClient.setTimeOffset(offset);

     //for some godforsaken reason US uses localtime and not utc so this hellish contraption operates on hope nobody turns it on during transition hour, but should fix itself later so hurray
    if (USSummerTime(timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_wday, timeInfo->tm_hour))
    {
        UStime = 0; // if summertime time than no change
    }
    else
    {
        UStime = hour; // if no summertime time then we subtract one hour
    }

    // second time getting for proper display
    currentEpoch = timeClient.getEpochTime();
    currentTime = (time_t)currentEpoch;
    timeInfo = gmtime(&currentTime);

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
