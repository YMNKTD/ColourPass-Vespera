
#include <Wire.h>
#include <Waveshare_LCD1602_RGB.h>

Waveshare_LCD1602_RGB lcd(16, 2);

const int TOUCH_PIN = 5;
bool lastTouch = false;

void showReady() {
  lcd.clear();
  lcd.setRGB(0, 80, 255);  // Blue backlight

  lcd.setCursor(0, 0);
  lcd.send_string("COLOURPASS");

  lcd.setCursor(0, 1);
  lcd.send_string("SYSTEM READY");
}

void showTouch() {
  lcd.clear();
  lcd.setRGB(0, 255, 0);  // Green backlight

  lcd.setCursor(0, 0);
  lcd.send_string("TOUCH DETECTED");

  lcd.setCursor(0, 1);
  lcd.send_string("INPUT ACTIVE");
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);

  lcd.init();
  showReady();

  Serial.println("ColourPass LCD + Touch Test");
}

void loop() {
  bool touched = digitalRead(TOUCH_PIN) == HIGH;

  if (touched != lastTouch) {
    if (touched) {
      Serial.println("Touch detected!");
      showTouch();
    } else {
      Serial.println("Touch released");
      showReady();
    }

    lastTouch = touched;
  }

  delay(50);
}
