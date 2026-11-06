/*
  nice!nano v2 variant for the Adafruit nRF52 core.
  Based on the Adafruit pca10056 variant (LGPL 2.1).

  Pin numbers are raw nRF52840 GPIOs: P0.n = n, P1.n = 32 + n.
  Pro Micro silkscreen -> GPIO:
    D0=8  D1=6  D2=17 D3=20 D4=22 D5=24 D6=32 D7=11 D8=36 D9=38
    D10=9 D14=43 D15=45 D16=10  A1=2 A2=29 A3=31
  P0.13 gates the external VCC pin. Genuine nice!nano: LOW = VCC on.
  The clone this mod was built on is the other way round (HIGH = VCC on).
  Check yours with the VccProbe sketch and set EXT_VCC_ON to match.
*/

#ifndef _VARIANT_NICENANO_
#define _VARIANT_NICENANO_

#define VARIANT_MCK       (64000000ul)

// Clones do not reliably fit the 32 kHz crystal, so use the internal RC.
#define USE_LFRC

#include "WVariant.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#define PINS_COUNT           (48)
#define NUM_DIGITAL_PINS     (48)
#define NUM_ANALOG_INPUTS    (8)
#define NUM_ANALOG_OUTPUTS   (0)

// LEDs (blue LED on P0.15, active high)
#define PIN_LED1             (15)
#define LED_BUILTIN          PIN_LED1
#define LED_CONN             PIN_LED1
#define LED_RED              PIN_LED1
#define LED_BLUE             PIN_LED1
#define LED_STATE_ON         1

// External VCC switch
#define PIN_EXT_VCC          (13)
#define EXT_VCC_ON           1

/*
 * Analog pins: A0..A7 are the chip's AIN0..AIN7, NOT the Pro Micro labels.
 * Silkscreen A1/A2/A3 are AIN0 (P0.02), AIN5 (P0.29), AIN7 (P0.31).
 */
#define PIN_A0               (2)
#define PIN_A1               (3)
#define PIN_A2               (4)
#define PIN_A3               (5)
#define PIN_A4               (28)
#define PIN_A5               (29)
#define PIN_A6               (30)
#define PIN_A7               (31)

static const uint8_t A0  = PIN_A0 ;
static const uint8_t A1  = PIN_A1 ;
static const uint8_t A2  = PIN_A2 ;
static const uint8_t A3  = PIN_A3 ;
static const uint8_t A4  = PIN_A4 ;
static const uint8_t A5  = PIN_A5 ;
static const uint8_t A6  = PIN_A6 ;
static const uint8_t A7  = PIN_A7 ;
#define ADC_RESOLUTION    14

#define PIN_AREF           (0xff)
static const uint8_t AREF = PIN_AREF;

// Serial1 on the Pro Micro TX/RX pads
#define PIN_SERIAL1_RX      (8)
#define PIN_SERIAL1_TX      (6)

#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_MISO         (45)
#define PIN_SPI_MOSI         (10)
#define PIN_SPI_SCK          (43)

static const uint8_t SS   = 9 ;
static const uint8_t MOSI = PIN_SPI_MOSI ;
static const uint8_t MISO = PIN_SPI_MISO ;
static const uint8_t SCK  = PIN_SPI_SCK ;

#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA         (17)
#define PIN_WIRE_SCL         (20)

#ifdef __cplusplus
}
#endif

#endif
