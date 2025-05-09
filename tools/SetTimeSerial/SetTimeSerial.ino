#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, waiting for time input over Serial...");
  }

  // wait for serial input 
  while (Serial.available() < 12) {
    delay(100);
  }

  int year = Serial.parseInt();
  int month = Serial.parseInt();
  int day = Serial.parseInt();
  int hour = Serial.parseInt();
  int minute = Serial.parseInt();
  int second = Serial.parseInt();

  rtc.adjust(DateTime(year, month, day, hour, minute, second + 3));
  Serial.println("RTC was set!");
}

void loop() {
  DateTime now = rtc.now();
  Serial.print(now.year()); Serial.print("-");
  Serial.print(now.month()); Serial.print("-");
  Serial.print(now.day()); Serial.print(" ");
  Serial.print(now.hour()); Serial.print(":");
  Serial.print(now.minute()); Serial.print(":");
  Serial.print(now.second()); Serial.println();
  delay(1000);
}
