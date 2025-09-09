#include "Adafruit_TinyUSB.h"
#include <bluefruit.h>

BLEDis bledis;
BLEHidAdafruit blehid;

const int ledPin = LED_BUILTIN; // pin to use for the LED

#define USE_USB_HID 0

#if USB_USB_HID
// Report ID
enum {
  RID_KEYBOARD = 1,kkjlk
  RID_CONSUMER_CONTROL, // Media, volume etc ..
};

// USB HID object
// desc report, desc len, protocol, interval, use out endpoint
// Using TinyUSB's template
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(RID_CONSUMER_CONTROL))
};

// USB HID object.
Adafruit_USBD_HID usb_hid;
#endif

void setup()
{

  Serial.begin(115200);

  // set LED pin to output mode
  pinMode(ledPin, OUTPUT);

  #if USE_USB_HID
  // Set up USB keyboard
  TinyUSBDevice.begin(0);

  // Set up output report (on control endpoint) for Capslock indicator
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("TinyUSB HID Composite");
  usb_hid.begin();

  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
  #endif

  Serial.println("Bluefruit52 HID");
  Serial.println("--------------------------------------\n");

  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values

  // Configure and start DIS (Device Information Service)
  bledis.setManufacturer("Robobits Inc.");
  bledis.setModel("GenCoupe ADV");
  bledis.begin();

  /* Start BLE HID
   * Note: Apple requires BLE devices to have a min connection interval >= 20m
   * (The smaller the connection interval the faster we can send data).
   * However, for HID and MIDI device Apple will accept a min connection
   * interval as low as 11.25 ms. Therefore BLEHidAdafruit::begin() will try to
   * set the min and max connection interval to 11.25 ms and 15 ms respectively
   * for the best performance.
   */
  blehid.begin();

  /* Set connection interval (min, max) to your perferred value.
   * Note: It is already set by BLEHidAdafruit::begin() to 11.25ms - 15ms
   * min = 9*1.25=11.25 ms, max = 12*1.25= 15 ms
   */
  /* Bluefruit.Periph.setConnInterval(9, 12); */

  // Set up and start advertising
  startAdv();
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);

  // Include BLE HID service
  Bluefruit.Advertising.addService(blehid);

  // There is enough room for the dev name in the advertising packet
  Bluefruit.Advertising.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void loop()
{
  digitalWrite(ledPin, HIGH);

  #if USE_USB_HID
  if (usb_hid.ready()) {
    usb_hid.sendReport16(RID_CONSUMER_CONTROL, HID_USAGE_CONSUMER_VOLUME_DECREMENT);
    usb_hid.sendReport16(RID_KEYBOARD, HID_KEY_VOLUME_DOWN);
  }
  #endif
  delay(100);

  #if USE_USB_HID
  if (usb_hid.ready()) {
    usb_hid.sendReport16(RID_CONSUMER_CONTROL, 0);
    usb_hid.sendReport16(RID_KEYBOARD, 0);
  }
  #endif

  digitalWrite(ledPin, LOW);

  delay(1000);
}
