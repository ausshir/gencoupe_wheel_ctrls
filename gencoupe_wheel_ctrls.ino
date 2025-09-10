#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>

// BLE Services
BLEDis bledis;
BLEHidAdafruit blehid;

// LED and Input Configuration
const int INPUT_PIN = A2;
const int NUM_SAMPLES = 5;

#define USE_SERIAL false

// HID Report IDs
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

enum HidType {
  HID_NONE,
  HID_CONSUMER,
  HID_KEYBOARD,
  HID_KEYBOARD_MOD
};

// Command Mapping
struct CommandRange {
  const char* label;
  int min;
  int max;
  HidType type;
  uint16_t code;     // consumer usage or keyboard key
  uint8_t modifier;  // keyboard modifier (0 if none)
};

CommandRange commandMap[] = {
  {"next",    51, 124, HID_CONSUMER, HID_USAGE_CONSUMER_SCAN_NEXT, 0},
  {"prev",   151, 249, HID_CONSUMER, HID_USAGE_CONSUMER_SCAN_PREVIOUS, 0},
  {"mode",   276, 349, HID_KEYBOARD, HID_KEY_HOME, 0},
  {"mute",   376, 449, HID_CONSUMER, HID_USAGE_CONSUMER_MUTE, 0},
  {"vol up", 476, 524, HID_CONSUMER, HID_USAGE_CONSUMER_VOLUME_INCREMENT, 0},
  {"vol dn", 551, 624, HID_CONSUMER, HID_USAGE_CONSUMER_VOLUME_DECREMENT, 0},
  {"voice", 651, 689, HID_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTGUI}, // Assistant
  {"hangup", 721, 809, HID_KEYBOARD, HID_KEY_ESCAPE, 0},           // Back
  {"answer", 831, 899, HID_KEYBOARD_MOD, HID_KEY_TAB, KEYBOARD_MODIFIER_LEFTALT} // Task switcher
};

const int NUM_COMMANDS = sizeof(commandMap)/sizeof(commandMap[0]);
String last_msg = "";

void setup() {

#if !USE_SERIAL
  Serial.end();
#endif

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(INPUT_PIN, INPUT);

  // HID setup
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("TinyUSB HID Composite");
  usb_hid.begin();

  // BLE setup
  Bluefruit.begin();
  Bluefruit.setTxPower(4);

  bledis.setManufacturer("Robobits Inc.");
  bledis.setModel("GenCoupe ADV");
  bledis.begin();

  blehid.begin();
  startAdv();

#if USE_SERIAL
  Serial.begin(115200);
  Serial.println("GC Wheel Control - Starting");
#endif

}

void startAdv() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_GENERIC_HID);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {

  digitalWrite(LED_RED, HIGH);

  // Sample analog input
  int total = 0;
  for (int i=0; i<NUM_SAMPLES; i++) {
    total += analogRead(INPUT_PIN);
    delay(1);
  }
  int avg = total / NUM_SAMPLES;

  // Detect command
  String msg = "unk";
  for (int i=0; i<NUM_COMMANDS; i++) {
    if (avg >= commandMap[i].min && avg <= commandMap[i].max) {
      msg = commandMap[i].label;
      break;
    }
  }

  // If command changed, act on it
  if (msg != last_msg) {
    last_msg = msg;
    #if USE_SERIAL
    Serial.println("Key = " + msg + " val=" + String(avg));
    #endif

    if (msg != "unk") {
      digitalWrite(LED_GREEN, HIGH);
      for (int i=0; i<NUM_COMMANDS; i++) {
        if (msg == commandMap[i].label) {
          CommandRange &cmd = commandMap[i];

          if (cmd.type == HID_CONSUMER) {

            usb_hid.sendReport16(RID_CONSUMER_CONTROL, cmd.code);
            delay(10);
            usb_hid.sendReport16(RID_CONSUMER_CONTROL, 0);
          }
          else if (cmd.type == HID_KEYBOARD || cmd.type == HID_KEYBOARD_MOD) {
            uint8_t keys[6] = { (uint8_t)cmd.code, 0, 0, 0, 0, 0 };
            usb_hid.keyboardReport(RID_KEYBOARD, cmd.modifier, keys);
            delay(10);
            usb_hid.keyboardRelease(RID_KEYBOARD);
          }
          break;
        }
      }
      digitalWrite(LED_GREEN, LOW);
    }
  }

  digitalWrite(LED_RED, LOW);
  delay(200);
}