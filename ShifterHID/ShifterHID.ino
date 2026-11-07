// Logitech Driving Force Shifter -> USB / Bluetooth LE HID joystick on a nice!nano v2.
// Buttons 1-6 = gears 1-6, button 7 = reverse, none pressed = neutral.
//
// USB:       plugged into a computer, it is a wired joystick (1209:0001, name from boards.txt).
// Bluetooth: without a USB data connection (battery on B+, or a USB charger) it advertises as
//            DEVICE_NAME and works as a Bluetooth LE joystick. It never uses both at once:
//            when USB comes up, Bluetooth disconnects, so a game never sees the shifter twice.
//
// Serial monitor (115200, USB only): prints "x,y,rev,gear,link,vbat_mv" at ~10 Hz.
// Commands (type + Enter): "unpair" forgets all paired Bluetooth hosts;
//                          "uf2" restarts into the bootloader with the NICENANO drive.

#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>

const char DEVICE_NAME[] = "Logitech H Shifter";  // Bluetooth name (USB name: usb_product in boards.txt)

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

// USB must be gone this long before Bluetooth starts (avoids advertising during USB enumeration).
const uint32_t BLE_START_DELAY_MS = 2000;
const uint32_t BATTERY_PERIOD_MS  = 30000;
const uint32_t REFRESH_MS         = 1000;  // repeat the current state this often

// Same report for USB and Bluetooth; Bluetooth HID needs a report ID, USB keeps none.
#define SHIFTER_REPORT_DESC(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     ), \
  HID_USAGE      ( HID_USAGE_DESKTOP_JOYSTICK ), \
  HID_COLLECTION ( HID_COLLECTION_APPLICATION ), \
    __VA_ARGS__ \
    /* 7 buttons: gears 1-6 and reverse */ \
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_BUTTON ), \
    HID_USAGE_MIN    ( 1                     ), \
    HID_USAGE_MAX    ( 7                     ), \
    HID_LOGICAL_MIN  ( 0                     ), \
    HID_LOGICAL_MAX  ( 1                     ), \
    HID_REPORT_COUNT ( 7                     ), \
    HID_REPORT_SIZE  ( 1                     ), \
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ), \
    /* 1 bit padding */ \
    HID_REPORT_COUNT ( 1                     ), \
    HID_REPORT_SIZE  ( 1                     ), \
    HID_INPUT        ( HID_CONSTANT          ), \
  HID_COLLECTION_END

const uint8_t BLE_REPORT_ID = 1;  // BLEHidGeneric maps input report n to report ID n+1
uint8_t const desc_usb_report[] = { SHIFTER_REPORT_DESC() };
uint8_t const desc_ble_report[] = { SHIFTER_REPORT_DESC(HID_REPORT_ID(BLE_REPORT_ID)) };

Adafruit_USBD_HID usb_hid;
BLEDis        bledis;
BLEBas        blebas;
BLEHidGeneric blehid(1);  // one input report

uint8_t gear = GEAR_NEUTRAL;
uint8_t sentUsb = 0xFF;  // last buttons sent per link; 0xFF forces a report
uint8_t sentBle = 0xFF;
uint32_t sentUsbAt = 0;
uint32_t sentBleAt = 0;
uint32_t usbSeenAt = 0;
uint32_t lastPrint = 0;
uint32_t lastBattery = 0;
uint32_t vbatMv = 0;
char cmd[16];
uint8_t cmdLen = 0;

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

// On the nice!nano the battery (B+) feeds the chip's VDDH pin, so VDDH/5 against the
// internal 0.6 V reference gives the battery voltage. On USB without a battery this
// reads the charger output (~4.2 V).
uint32_t readBatteryMv() {
  analogReference(AR_INTERNAL);  // 0..3.6 V
  analogReadResolution(12);
  uint32_t raw = analogReadVDDHDIV5();
  analogReference(AR_VDD4);      // back to the shifter's ratiometric setup
  analogReadResolution(10);
  return raw * 3600UL * 5 / 4095;
}

// Rough single-cell LiPo discharge curve.
uint8_t batteryPercent(uint32_t mv) {
  static const uint16_t curve[][2] = {
    {4200, 100}, {4100, 90}, {4000, 80}, {3900, 65}, {3800, 50},
    {3700, 30}, {3600, 15}, {3500, 5}, {3300, 0},
  };
  if (mv >= curve[0][0]) return 100;
  for (uint8_t i = 1; i < sizeof(curve) / sizeof(curve[0]); i++) {
    if (mv >= curve[i][0]) {
      uint32_t span = curve[i - 1][0] - curve[i][0];
      return curve[i][1] + (mv - curve[i][0]) * (curve[i - 1][1] - curve[i][1]) / span;
    }
  }
  return 0;
}

void setupBluetooth() {
  Bluefruit.autoConnLed(false);  // the LED is driven below
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(DEVICE_NAME);

  bledis.setManufacturer("DIY");
  bledis.setModel(DEVICE_NAME);
  bledis.begin();

  blebas.begin();
  blebas.write(batteryPercent(vbatMv));

  uint16_t inputLen[] = { 1 };
  blehid.enableKeyboard(false);
  blehid.enableMouse(false);
  blehid.setReportLen(inputLen);
  blehid.setReportMap(desc_ble_report, sizeof(desc_ble_report));
  blehid.begin();

  // Ask for a 7.5-15 ms connection interval: low latency for shifting.
  Bluefruit.Periph.setConnInterval(6, 12);

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_JOYSTICK);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(false);  // loop() decides when to advertise
  Bluefruit.Advertising.setInterval(32, 244);        // 20 ms fast, 152.5 ms slow (units of 0.625 ms)
  Bluefruit.Advertising.setFastTimeout(30);
}

// Bluetooth runs only while there is no USB data connection.
void manageBluetooth(bool usb, uint32_t now) {
  if (usb) usbSeenAt = now;
  bool allowed = !usb && (now - usbSeenAt >= BLE_START_DELAY_MS);
  bool connected = Bluefruit.connected() > 0;

  if (allowed) {
    if (!connected && !Bluefruit.Advertising.isRunning()) Bluefruit.Advertising.start(0);
  } else {
    if (Bluefruit.Advertising.isRunning()) Bluefruit.Advertising.stop();
    if (connected) {
      BLEConnection* conn = Bluefruit.Connection(Bluefruit.connHandle());
      if (conn) conn->disconnect();
    }
  }
}

void handleSerialCommand() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      cmd[cmdLen] = 0;
      if (strcmp(cmd, "unpair") == 0) {
        Bluefruit.Periph.clearBonds();
        Serial.println("# Bluetooth pairings cleared");
      } else if (strcmp(cmd, "uf2") == 0) {
        Serial.println("# restarting into the bootloader (NICENANO drive)");
        Serial.flush();
        delay(100);
        enterUf2Dfu();  // same as double-tapping reset
      }
      cmdLen = 0;
    } else if (cmdLen < sizeof(cmd) - 1) {
      cmd[cmdLen++] = c;
    }
  }
}

const char* linkName(bool usb) {
  if (usb) return "USB";
  if (Bluefruit.connected()) return "BLE";
  if (Bluefruit.Advertising.isRunning()) return "ADV";
  return "WAIT";
}

void setup() {
  pinMode(PIN_EXT_VCC, OUTPUT);
  digitalWrite(PIN_EXT_VCC, EXT_VCC_ON);

  pinMode(PIN_REVERSE, INPUT_PULLDOWN);  // a loose wire reads "not reverse"
  pinMode(LED_BUILTIN, OUTPUT);

  analogOversampling(8);
  vbatMv = readBatteryMv();  // also sets AR_VDD4, 10-bit for the shifter

  Serial.begin(115200);

  usb_hid.setPollInterval(1);
  usb_hid.setReportDescriptor(desc_usb_report, sizeof(desc_usb_report));
  usb_hid.begin();

  setupBluetooth();

  // USB is already up before setup(); re-enumerate so the host sees the HID interface.
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void loop() {
  uint32_t now = millis();
  int x = analogRead(PIN_X);
  int y = analogRead(PIN_Y);
  bool reverse = digitalRead(PIN_REVERSE);

  gear = updateGear(x, y, reverse);
  uint8_t buttons = (gear == GEAR_NEUTRAL) ? 0 : (1 << (gear - 1));

  bool usb = TinyUSBDevice.mounted();
  manageBluetooth(usb, now);

  // Reports go out on every change, and the current state is repeated every REFRESH_MS:
  // a report sent while the host is still setting up (right after boot or connecting) is lost,
  // and without the repeat the host would show the wrong gear until the stick moves.

  // USB report
  if (!usb) {
    sentUsb = 0xFF;  // resend the current gear when USB comes back
  } else if ((buttons != sentUsb || now - sentUsbAt >= REFRESH_MS) && usb_hid.ready()) {
    if (usb_hid.sendReport(0, &buttons, sizeof(buttons))) { sentUsb = buttons; sentUsbAt = now; }
  }

  // Bluetooth report (fails, and is retried, until the host has subscribed)
  bool ble = Bluefruit.connected() > 0;
  if (!ble) {
    sentBle = 0xFF;
  } else if (buttons != sentBle || now - sentBleAt >= REFRESH_MS) {
    if (blehid.inputReport(BLE_REPORT_ID, &buttons, sizeof(buttons))) { sentBle = buttons; sentBleAt = now; }
  }

  if (now - lastBattery >= BATTERY_PERIOD_MS) {
    lastBattery = now;
    vbatMv = readBatteryMv();
    uint8_t pct = batteryPercent(vbatMv);
    if (ble) blebas.notify(pct); else blebas.write(pct);
  }

  // LED: on USB it shows "in gear"; while advertising it blinks briefly once a second;
  // otherwise it stays off to save battery.
  bool led;
  if (usb) led = (gear != GEAR_NEUTRAL);
  else if (Bluefruit.Advertising.isRunning()) led = (now % 1000) < 30;
  else led = false;
  digitalWrite(LED_BUILTIN, led);

  if (Serial) {
    handleSerialCommand();
    if (now - lastPrint >= 100) {
      lastPrint = now;
      Serial.print(x);
      Serial.print(',');
      Serial.print(y);
      Serial.print(',');
      Serial.print(reverse);
      Serial.print(',');
      Serial.print(gear == GEAR_REVERSE ? "R" : gear == GEAR_NEUTRAL ? "N" : String(gear).c_str());
      Serial.print(',');
      Serial.print(linkName(usb));
      Serial.print(',');
      Serial.println(vbatMv);
    }
  }

  delay(1);
}
