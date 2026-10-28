 #include <Wire.h>

const int TOUCH_PIN = 5;

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);

  Wire.begin();
  Wire.setClock(100000);

  delay(1500);
  Serial.println("ColourPass Hardware Test");
}

void loop() {
  Serial.println("Scanning I2C devices...");

  int devices = 0;

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);

    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      devices++;
    }
  }

  if (devices == 0) {
    Serial.println("No I2C devices found!");
  }

  Serial.print("Touch state: ");
  Serial.println(digitalRead(TOUCH_PIN));

  Serial.println("--------------------");
  delay(2000);
}
