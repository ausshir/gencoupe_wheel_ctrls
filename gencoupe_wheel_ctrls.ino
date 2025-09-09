#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>

// BLE Services
BLEDis bledis;
BLEHidAdafruit blehid;

// LED and Input Configuration
const int ledPin = LED_BUILTIN;
const int INPUT_PIN = A2;
const int NUM_SAMPLES = 5;

// USB HID Toggle
// Serial debugging not available in this mode
#define USE_USB_HID true

#if USE_USB_HID
// Report IDs
enum {
  RID_KEYBOARD = 1,
  RID_CONSUMER_CONTROL
};

// HID Report Descriptor
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(RID_CONSUMER_CONTROL))
};

Adafruit_USBD_HID usb_hid;
#endif

// Command Mapping
struct CommandRange {
  const char* label;
  int min;
  int max;
};

CommandRange commandMap[] = {
  {"next",    51, 124},
  {"prev",   151, 249},
  {"mode",   276, 349},
  {"mute",   376, 449},
  {"vol up", 476, 524},
  {"vol dn", 551, 624},
  {"voice",  651, 689},
  {"hangup", 721, 809},
  {"answer", 831, 899}
};

const int NUM_COMMANDS = sizeof(commandMap) / sizeof(commandMap[0]);
String last_msg = "";

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(INPUT_PIN, INPUT);

  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("GC Wheel Control - Starting");

#if USE_USB_HID
  TinyUSBDevice.begin(0);
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("TinyUSB HID Composite");
  usb_hid.begin();

  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
#endif

  Bluefruit.begin();
  Bluefruit.setTxPower(4);

  bledis.setManufacturer("Robobits Inc.");
  bledis.setModel("GenCoupe ADV");
  bledis.begin();

  blehid.begin();
  startAdv();
}

void startAdv() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {
  digitalWrite(ledPin, HIGH);

  // Sample analog input
  int total = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    total += analogRead(INPUT_PIN);
    delay(1);
  }
  int avg = total / NUM_SAMPLES;

  // Detect command
  String msg = "unk";
  for (int i = 0; i < NUM_COMMANDS; i++) {
    if (avg >= commandMap[i].min && avg <= commandMap[i].max) {
      msg = commandMap[i].label;
      break;
    }
  }

  // If command changed, act on it
  if (msg != last_msg) {
    last_msg = msg;
    if (msg != "unk") {
      Serial.println("Key = " + msg + " val=" + String(avg));

      // Send HID report via BLE
      if (msg == "vol up") blehid.consumerKeyPress(HID_USAGE_CONSUMER_VOLUME_INCREMENT);
      else if (msg == "vol dn") blehid.consumerKeyPress(HID_USAGE_CONSUMER_VOLUME_DECREMENT);
      else if (msg == "prev") blehid.consumerKeyPress(HID_USAGE_CONSUMER_SCAN_PREVIOUS);
      else if (msg == "next") blehid.consumerKeyPress(HID_USAGE_CONSUMER_SCAN_NEXT);

      delay(10);

      blehid.keyRelease();
      blehid.consumerKeyRelease();

#if USE_USB_HID
      if (usb_hid.ready()) {
        if (msg == "vol up") usb_hid.sendReport16(RID_CONSUMER_CONTROL, HID_USAGE_CONSUMER_VOLUME_INCREMENT);
        else if (msg == "vol dn") usb_hid.sendReport16(RID_CONSUMER_CONTROL, HID_USAGE_CONSUMER_VOLUME_DECREMENT);
        else if (msg == "prev") usb_hid.sendReport16(RID_CONSUMER_CONTROL, HID_USAGE_CONSUMER_SCAN_PREVIOUS);
        else if (msg == "next") usb_hid.sendReport16(RID_CONSUMER_CONTROL, HID_USAGE_CONSUMER_SCAN_NEXT);

        delay(10);

        usb_hid.sendReport16(RID_CONSUMER_CONTROL, 0);
        usb_hid.sendReport16(RID_KEYBOARD, 0);
      }
#endif
    }
  }
  digitalWrite(ledPin, LOW);

  delay(200);
}
