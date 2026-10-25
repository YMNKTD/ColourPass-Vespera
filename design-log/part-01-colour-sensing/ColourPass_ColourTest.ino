
/*##
  ColourPass - Day 1
  DFRobot TCS3200 Colour Sensor Test
  Arduino MKR WiFi 1010

  Sensor:
  S0  -> 0
  S1  -> 1
  S2  -> 2
  S3  -> 3
  OUT -> 4
*/

const int S0 = 0;
const int S1 = 1;
const int S2 = 2;
const int S3 = 3;
const int sensorOut = 4;

unsigned long readColour(
  int s2Value,
  int s3Value
) {
  digitalWrite(S2, s2Value);
  digitalWrite(S3, s3Value);

  delay(10);

  return pulseIn(sensorOut, LOW, 100000UL);
}

void setup() {
  Serial.begin(115200);

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);

  // 20% frequency scaling
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  delay(1000);

  Serial.println("ColourPass Day 1");
  Serial.println("Colour sensor ready");
}

void loop() {
  unsigned long red = readColour(LOW, LOW);
  unsigned long green = readColour(HIGH, HIGH);
  unsigned long blue = readColour(LOW, HIGH);

  Serial.print("R: ");
  Serial.print(red);

  Serial.print(" | G: ");
  Serial.print(green);

  Serial.print(" | B: ");
  Serial.println(blue);

  delay(500);
}
