/*
  nice!nano v2 variant for the Adafruit nRF52 core.
  Based on the Adafruit pca10056 variant (LGPL 2.1).
*/

#include "variant.h"
#include "wiring_constants.h"
#include "wiring_digital.h"
#include "nrf.h"

const uint32_t g_ADigitalPinMap[] =
{
  // P0
  0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 ,
  8 , 9 , 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,

  // P1
  32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47
};


void initVariant()
{
  pinMode(PIN_LED1, OUTPUT);
  ledOff(PIN_LED1);

  // Power the external VCC pin as early as possible.
  pinMode(PIN_EXT_VCC, OUTPUT);
  digitalWrite(PIN_EXT_VCC, EXT_VCC_ON);
}
