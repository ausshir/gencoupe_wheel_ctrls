#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>
#include <nrf.h>

#define USE_SERIAL 1

// BLE Services
BLEDis bledis;
BLEHidAdafruit blehid;

// Input Configuration
const int INPUT_PIN = A2;
const int NUM_SAMPLES = 10;
const int MAX_NOISE = 25;

// HID Services
Adafruit_USBD_HID usb_hid;

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
  {"mode",   276, 349, HID_KEYBOARD, HID_KEY_ENTER, KEYBOARD_MODIFIER_LEFTGUI}, // Home
  {"mute",   376, 449, HID_CONSUMER, HID_USAGE_CONSUMER_PLAY_PAUSE, 0},
  {"vol up", 476, 524, HID_CONSUMER, HID_USAGE_CONSUMER_VOLUME_INCREMENT, 0},
  {"vol dn", 551, 624, HID_CONSUMER, HID_USAGE_CONSUMER_VOLUME_DECREMENT, 0},
  {"voice", 651, 689, HID_KEYBOARD, 0, KEYBOARD_MODIFIER_LEFTGUI}, // Assistant
  {"hangup", 721, 809, HID_KEYBOARD, HID_KEY_ESCAPE, 0},           // Back
  {"answer", 831, 899, HID_KEYBOARD_MOD, HID_KEY_TAB, KEYBOARD_MODIFIER_LEFTALT}, // Task switcher
  {"none", 925, 970, HID_KEYBOARD, HID_KEY_ESCAPE, 0} // No key pressed
};

const int NUM_COMMANDS = sizeof(commandMap)/sizeof(commandMap[0]);
String last_msg = "";

void setup() {

  NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Msk | WDT_CONFIG_HALT_Msk); // Run in sleep and halt
  NRF_WDT->CRV = 5 * 32768; // ~5 seconds @ 32768 ticks
  NRF_WDT->RREN |= WDT_RREN_RR0_Msk; // Enable reload register 0
  NRF_WDT->TASKS_START = 1; // Start
  // Note - clear with writing NRF_WDT->RR[0] = 0x6E524635;

#if !USE_SERIAL
  // Note - on some devices the composite serial device
  // results in the HID keyboard being ignored
  // When disabled, must reprogram from bootloader
  Serial.end();
#else
  Serial.begin(115200);
  Serial.println("GC Wheel Control - Starting");
#endif

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(INPUT_PIN, INPUT);

  NRF_WDT->RR[0] = 0x6E524635;
  delay(1000);  // Give USB host time to stabilize

  // HID setup
  // Light green while setting up USB HID
  // If host is not up, it may hang here & WDT will reset
  digitalWrite(LED_GREEN, HIGH);
  NRF_WDT->RR[0] = 0x6E524635;
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("TinyUSB HID Composite");
  usb_hid.begin();
  digitalWrite(LED_GREEN, LOW);

  // BLE setup
  NRF_WDT->RR[0] = 0x6E524635;
  Bluefruit.begin();
  Bluefruit.setTxPower(4);

  bledis.setManufacturer("Robobits Inc.");
  bledis.setModel("GenCoupe ADV");
  bledis.begin();

  NRF_WDT->RR[0] = 0x6E524635;
  blehid.begin();
  startAdv();

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
  NRF_WDT->RR[0] = 0x6E524635;

  // Sample analog input
  int total = 0;
  int min_sample = 1023;
  int max_sample = 0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    int sample = analogRead(INPUT_PIN);
    total += sample;
    if (sample < min_sample) min_sample = sample;
    if (sample > max_sample) max_sample = sample;
    delay(1);
  }

  int avg = total / NUM_SAMPLES;
  int sample_spread = max_sample - min_sample;

  if (sample_spread > MAX_NOISE) {
    Serial.println("Noisy input: avg=" + String(avg) + " spread=" + String(sample_spread));
    return; // Skip this loop iteration
  }

  // Match to a known command range
  String msg = "unk";
  for (int i=0; i<NUM_COMMANDS; i++) {
    if (avg >= commandMap[i].min && avg <= commandMap[i].max) {
      msg = commandMap[i].label;
      break;
    }
  }

  // If command changed, act on it
  if (msg != last_msg) {
    #if USE_SERIAL
    Serial.println("Key = " + msg + " val=" + String(avg));
    #endif

    if (msg != "unk") {
      if (msg != "none") {
        last_msg = msg;
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
      else {
        #if USE_SERIAL
        Serial.println("Key UP: " + last_msg);
        #endif
        last_msg = msg;
      }
    }
  }

  digitalWrite(LED_RED, LOW);
  delay(200);
}