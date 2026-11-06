// Finds which P0.13 level switches the nice!nano's VCC pin on.
// Flips P0.13 every 2 s and prints "p013,x,y" at ~10 Hz on USB serial (115200).
// With the shifter wired, x and y read 0 while VCC is off and ~400-500 (stick in neutral) while it is on.
// Genuine nice!nano: VCC on when P0.13 = 0. Some clones (the one this mod was built on): when P0.13 = 1.
// Put the level that powers the shifter into EXT_VCC_ON in hardware/nicenano/nrf52/variants/nicenano/variant.h.

#include <Adafruit_TinyUSB.h>

const uint8_t PIN_X = 31;  // P0.31, Pro Micro A3, orange wire
const uint8_t PIN_Y = 29;  // P0.29, Pro Micro A2, green wire

void setup() {
  pinMode(PIN_EXT_VCC, OUTPUT);

  analogReference(AR_VDD4);
  analogReadResolution(10);
  analogOversampling(8);

  Serial.begin(115200);
}

void loop() {
  int level = (millis() / 2000) % 2;
  digitalWrite(PIN_EXT_VCC, level);

  Serial.print(level);
  Serial.print(',');
  Serial.print(analogRead(PIN_X));
  Serial.print(',');
  Serial.println(analogRead(PIN_Y));

  delay(100);
}
