// Logitech Driving Force Shifter -> USB HID joystick on a nice!nano v2.
// Buttons 1-6 = gears 1-6, button 7 = reverse, none pressed = neutral.
// USB identity (1209:0001 "H-Pattern Shifter") comes from the nicenano board definition.
// With a serial monitor open (115200) it prints "x,y,rev,gear" at ~10 Hz for recalibration.

#include <Adafruit_TinyUSB.h>

const uint8_t PIN_X       = 31;  // P0.31, Pro Micro A3, orange wire
const uint8_t PIN_Y       = 29;  // P0.29, Pro Micro A2, green wire
const uint8_t PIN_REVERSE = 8;   // P0.08, Pro Micro D0/RX, gray wire

// Calibration from this unit (10-bit, ratiometric to VDD).
// Measured: left column x 156-217, middle 423-470, right 673-769;
// up y 761-1014, neutral 412-452, down 0-94.
const int X_LEFT_MAX  = 300;  // x below this: gears 1/2
const int X_RIGHT_MIN = 590;  // x above this: gears 5/6/R
const int Y_UP_ENGAGE    = 650;  // enter an upper gear (1/3/5)
const int Y_UP_RELEASE   = 600;  // fall back to neutral
const int Y_DOWN_ENGAGE  = 230;  // enter a lower gear (2/4/6/R)
const int Y_DOWN_RELEASE = 280;

const uint8_t GEAR_NEUTRAL = 0;
const uint8_t GEAR_REVERSE = 7;

uint8_t const desc_hid_report[] = {
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     ),
  HID_USAGE      ( HID_USAGE_DESKTOP_JOYSTICK ),
  HID_COLLECTION ( HID_COLLECTION_APPLICATION ),
    // 7 buttons: gears 1-6 and reverse
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_BUTTON ),
    HID_USAGE_MIN    ( 1                     ),
    HID_USAGE_MAX    ( 7                     ),
    HID_LOGICAL_MIN  ( 0                     ),
    HID_LOGICAL_MAX  ( 1                     ),
    HID_REPORT_COUNT ( 7                     ),
    HID_REPORT_SIZE  ( 1                     ),
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
    // 1 bit padding
    HID_REPORT_COUNT ( 1                     ),
    HID_REPORT_SIZE  ( 1                     ),
    HID_INPUT        ( HID_CONSTANT          ),
  HID_COLLECTION_END
};

Adafruit_USBD_HID usb_hid;

uint8_t gear = GEAR_NEUTRAL;
uint8_t sentButtons = 0xFF;  // force the first report
uint32_t lastPrint = 0;

// Column 0 = left, 1 = middle, 2 = right.
uint8_t column(int x) {
  if (x < X_LEFT_MAX)  return 0;
  if (x > X_RIGHT_MIN) return 2;
  return 1;
}

uint8_t updateGear(int x, int y, bool reverse) {
  bool up   = (gear == 1 || gear == 3 || gear == 5);
  bool down = (gear == 2 || gear == 4 || gear == 6 || gear == GEAR_REVERSE);

  // Stay in the current gear until the stick is clearly back towards neutral.
  if (up   && y > Y_UP_RELEASE)   return gear;
  if (down && y < Y_DOWN_RELEASE) {
    if (gear == 6 || gear == GEAR_REVERSE) return reverse ? GEAR_REVERSE : 6;
    return gear;
  }

  if (y > Y_UP_ENGAGE)   return 1 + 2 * column(x);   // 1, 3, 5
  if (y < Y_DOWN_ENGAGE) {
    uint8_t g = 2 + 2 * column(x);                   // 2, 4, 6
    return (g == 6 && reverse) ? GEAR_REVERSE : g;
  }
  return GEAR_NEUTRAL;
}

void setup() {
  pinMode(PIN_EXT_VCC, OUTPUT);
  digitalWrite(PIN_EXT_VCC, EXT_VCC_ON);

  pinMode(PIN_REVERSE, INPUT_PULLDOWN);  // a loose wire reads "not reverse"
  pinMode(LED_BUILTIN, OUTPUT);

  analogReference(AR_VDD4);
  analogReadResolution(10);
  analogOversampling(8);

  Serial.begin(115200);

  usb_hid.setPollInterval(1);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  // USB is already up before setup(); re-enumerate so the host sees the HID interface.
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void loop() {
  int x = analogRead(PIN_X);
  int y = analogRead(PIN_Y);
  bool reverse = digitalRead(PIN_REVERSE);

  gear = updateGear(x, y, reverse);
  uint8_t buttons = (gear == GEAR_NEUTRAL) ? 0 : (1 << (gear - 1));

  if (buttons != sentButtons && TinyUSBDevice.mounted() && usb_hid.ready()) {
    if (usb_hid.sendReport(0, &buttons, sizeof(buttons))) {
      sentButtons = buttons;
    }
  }
  digitalWrite(LED_BUILTIN, gear != GEAR_NEUTRAL);

  if (Serial && millis() - lastPrint >= 100) {
    lastPrint = millis();
    Serial.print(x);
    Serial.print(',');
    Serial.print(y);
    Serial.print(',');
    Serial.print(reverse);
    Serial.print(',');
    Serial.println(gear == GEAR_REVERSE ? "R" : gear == GEAR_NEUTRAL ? "N" : String(gear).c_str());
  }

  delay(1);
}
