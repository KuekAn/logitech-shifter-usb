// Raw readout of the Logitech Driving Force Shifter on a nice!nano v2.
// Prints "x,y,revUp,revDown" at ~10 Hz on USB serial (115200).
//   x, y    : 10-bit ADC, ratiometric to VDD (0..1023 = 0..3.3 V)
//   revUp   : reverse pin read with pull-up
//   revDown : reverse pin read with pull-down
// If revUp and revDown always agree, the shifter is driving the line.

#include <Adafruit_TinyUSB.h>

const uint8_t PIN_X       = 31;  // P0.31, Pro Micro A3, orange wire
const uint8_t PIN_Y       = 29;  // P0.29, Pro Micro A2, green wire
const uint8_t PIN_REVERSE = 8;   // P0.08, Pro Micro D0/RX, gray wire

void setup() {
  pinMode(PIN_EXT_VCC, OUTPUT);
  digitalWrite(PIN_EXT_VCC, EXT_VCC_ON);

  pinMode(LED_BUILTIN, OUTPUT);

  analogReference(AR_VDD4);
  analogReadResolution(10);
  analogOversampling(8);

  Serial.begin(115200);
}

void loop() {
  int x = analogRead(PIN_X);
  int y = analogRead(PIN_Y);

  pinMode(PIN_REVERSE, INPUT_PULLUP);
  delayMicroseconds(50);
  int revUp = digitalRead(PIN_REVERSE);

  pinMode(PIN_REVERSE, INPUT_PULLDOWN);
  delayMicroseconds(50);
  int revDown = digitalRead(PIN_REVERSE);

  Serial.print(x);
  Serial.print(',');
  Serial.print(y);
  Serial.print(',');
  Serial.print(revUp);
  Serial.print(',');
  Serial.println(revDown);

  digitalToggle(LED_BUILTIN);
  delay(100);
}
