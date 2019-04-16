/*
 * This file is part of the TREZOR project, https://trezor.io/
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/usb/hid.h>
#include <libopencm3/usb/usbd.h>
#include <string.h>
#include "bitmaps.h"
#include "buttons.h"
#include "hmac.h"
#include "layout.h"
#include "oled.h"
#include "pbkdf2.h"
#include "rng.h"
#include "setup.h"

const int states = 2;
int state = 0;
int frame = 0;

uint8_t seed[128];
uint8_t *pass = (uint8_t *)"meadow";
uint32_t passlen;
uint8_t *salt = (uint8_t *)"TREZOR";
uint32_t saltlen;

static const struct usb_device_descriptor dev_descr = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x1209,
    .idProduct = 0x53c1,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

/* got via usbhid-dump from CP2110 */
static const uint8_t hid_report_descriptor[] = {
    0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x09, 0x01, 0x75, 0x08, 0x95,
    0x40, 0x26, 0xFF, 0x00, 0x15, 0x00, 0x85, 0x01, 0x95, 0x01, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x02, 0x95, 0x02, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x03, 0x95, 0x03, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x04, 0x95, 0x04, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x05, 0x95, 0x05, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x06, 0x95, 0x06, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x07, 0x95, 0x07, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x08, 0x95, 0x08, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x09, 0x95, 0x09, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0A, 0x95, 0x0A, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0B, 0x95, 0x0B, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0C, 0x95, 0x0C, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0D, 0x95, 0x0D, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0E, 0x95, 0x0E, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0F, 0x95, 0x0F, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x10, 0x95, 0x10, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x11, 0x95, 0x11, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x12, 0x95, 0x12, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x13, 0x95, 0x13, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x14, 0x95, 0x14, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x15, 0x95, 0x15, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x16, 0x95, 0x16, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x17, 0x95, 0x17, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x18, 0x95, 0x18, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x19, 0x95, 0x19, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1A, 0x95, 0x1A, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1B, 0x95, 0x1B, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1C, 0x95, 0x1C, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1D, 0x95, 0x1D, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1E, 0x95, 0x1E, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1F, 0x95, 0x1F, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x20, 0x95, 0x20, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x21, 0x95, 0x21, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x22, 0x95, 0x22, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x23, 0x95, 0x23, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x24, 0x95, 0x24, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x25, 0x95, 0x25, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x26, 0x95, 0x26, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x27, 0x95, 0x27, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x28, 0x95, 0x28, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x29, 0x95, 0x29, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2A, 0x95, 0x2A, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2B, 0x95, 0x2B, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2C, 0x95, 0x2C, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2D, 0x95, 0x2D, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2E, 0x95, 0x2E, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2F, 0x95, 0x2F, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x30, 0x95, 0x30, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x31, 0x95, 0x31, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x32, 0x95, 0x32, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x33, 0x95, 0x33, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x34, 0x95, 0x34, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x35, 0x95, 0x35, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x36, 0x95, 0x36, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x37, 0x95, 0x37, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x38, 0x95, 0x38, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x39, 0x95, 0x39, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3A, 0x95, 0x3A, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3B, 0x95, 0x3B, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3C, 0x95, 0x3C, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3D, 0x95, 0x3D, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3E, 0x95, 0x3E, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3F, 0x95, 0x3F, 0x09, 0x01,
    0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x40, 0x95, 0x01, 0x09, 0x01,
    0xB1, 0x02, 0x85, 0x41, 0x95, 0x01, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x42,
    0x95, 0x06, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x43, 0x95, 0x01, 0x09, 0x01,
    0xB1, 0x02, 0x85, 0x44, 0x95, 0x02, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x45,
    0x95, 0x04, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x46, 0x95, 0x02, 0x09, 0x01,
    0xB1, 0x02, 0x85, 0x47, 0x95, 0x02, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x50,
    0x95, 0x08, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x51, 0x95, 0x01, 0x09, 0x01,
    0xB1, 0x02, 0x85, 0x52, 0x95, 0x01, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x60,
    0x95, 0x0A, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x61, 0x95, 0x3F, 0x09, 0x01,
    0xB1, 0x02, 0x85, 0x62, 0x95, 0x3F, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x63,
    0x95, 0x3F, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x64, 0x95, 0x3F, 0x09, 0x01,
    0xB1, 0x02, 0x85, 0x65, 0x95, 0x3E, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x66,
    0x95, 0x13, 0x09, 0x01, 0xB1, 0x02, 0xC0,
};

static const struct {
  struct usb_hid_descriptor hid_descriptor;
  struct {
    uint8_t bReportDescriptorType;
    uint16_t wDescriptorLength;
  } __attribute__((packed)) hid_report;
} __attribute__((packed))
hid_function = {.hid_descriptor =
                    {
                        .bLength = sizeof(hid_function),
                        .bDescriptorType = USB_DT_HID,
                        .bcdHID = 0x0111,
                        .bCountryCode = 0,
                        .bNumDescriptors = 1,
                    },
                .hid_report = {
                    .bReportDescriptorType = USB_DT_REPORT,
                    .wDescriptorLength = sizeof(hid_report_descriptor),
                }};

static const struct usb_endpoint_descriptor hid_endpoints[2] = {
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x81,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = 64,
        .bInterval = 1,
    },
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x02,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = 64,
        .bInterval = 1,
    }};

static const struct usb_interface_descriptor hid_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface = 0,
    .endpoint = hid_endpoints,
    .extra = &hid_function,
    .extralen = sizeof(hid_function),
}};

static const struct usb_interface ifaces[] = {{
    .num_altsetting = 1,
    .altsetting = hid_iface,
}};

static const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 1,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0x80,
    .bMaxPower = 0x32,
    .interface = ifaces,
};

static const char *usb_strings[] = {
    "SatoshiLabs",
    "TREZOR",
    "01234567",
};

static enum usbd_request_return_codes hid_control_request(
    usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
    usbd_control_complete_callback *complete) {
  (void)complete;
  (void)dev;

  if ((req->bmRequestType != 0x81) ||
      (req->bRequest != USB_REQ_GET_DESCRIPTOR) || (req->wValue != 0x2200))
    return 0;

  /* Handle the HID report descriptor. */
  *buf = (uint8_t *)hid_report_descriptor;
  *len = sizeof(hid_report_descriptor);

  return 1;
}

static void hid_rx_callback(usbd_device *dev, uint8_t ep) {
  (void)dev;
  (void)ep;
}

static void hid_set_config(usbd_device *dev, uint16_t wValue) {
  (void)wValue;

  usbd_ep_setup(dev, 0x81, USB_ENDPOINT_ATTR_INTERRUPT, 64, 0);
  usbd_ep_setup(dev, 0x02, USB_ENDPOINT_ATTR_INTERRUPT, 64, hid_rx_callback);

  usbd_register_control_callback(
      dev, USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
      USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT, hid_control_request);
}

static usbd_device *usbd_dev;
static uint8_t usbd_control_buffer[128];

void usbInit(void) {
  usbd_dev = usbd_init(&otgfs_usb_driver, &dev_descr, &config, usb_strings, 3,
                       usbd_control_buffer, sizeof(usbd_control_buffer));
  usbd_register_set_config_callback(usbd_dev, hid_set_config);
}

int main(void) {
#ifndef APPVER
  setup();
  __stack_chk_guard = random32();  // this supports compiler provided
                                   // unpredictable stack protection checks
  oledInit();
#else
  setupApp();
  __stack_chk_guard = random32();  // this supports compiler provided
                                   // unpredictable stack protection checks
#endif

  usbInit();

  passlen = strlen((char *)pass);
  saltlen = strlen((char *)salt);

  for (;;) {
    frame = 0;
    switch (state) {
      case 0:
        oledClear();
        oledDrawBitmap(40, 0, &bmp_logo64);
        break;
    }
    oledRefresh();

    do {
      usbd_poll(usbd_dev);
      switch (state) {
        case 1:
          layoutProgress("WORKING", frame % 41 * 25);
          pbkdf2_hmac_sha512(pass, passlen, salt, saltlen, 100, seed, 64);
          usbd_ep_write_packet(usbd_dev, 0x81, seed, 64);
          break;
      }

      buttonUpdate();
      frame += 1;
    } while (!button.YesUp && !button.NoUp);

    if (button.YesUp) {
      state = (state + 1) % states;
      oledSwipeLeft();
    } else {
      state = (state + states - 1) % states;
      oledSwipeRight();
    }
  }

  return 0;
}
