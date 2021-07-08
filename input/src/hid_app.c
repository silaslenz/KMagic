/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board.h"
#include "tusb.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#define UART_ID_DEV_0 uart0
#define UART_ID_DEV_1 uart1
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 2
#define PARITY UART_PARITY_EVEN

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN_DEV_0 0
#define UART_RX_PIN_DEV_0 1
#define UART_TX_PIN_DEV_1 4
#define UART_RX_PIN_DEV_1 5
void uart_startup(){
  uart_init(UART_ID_DEV_0, BAUD_RATE);
  gpio_set_function(UART_TX_PIN_DEV_0, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN_DEV_0, GPIO_FUNC_UART);
  uart_set_format(UART_ID_DEV_0, DATA_BITS, STOP_BITS, PARITY);

  uart_puts(UART_ID_DEV_0, " Hello, UART0!\n");


  uart_init(UART_ID_DEV_1, BAUD_RATE);
  gpio_set_function(UART_TX_PIN_DEV_1, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN_DEV_1, GPIO_FUNC_UART);
  uart_set_format(UART_ID_DEV_1, DATA_BITS, STOP_BITS, PARITY);

  uart_puts(UART_ID_DEV_1, " Hello, UART1!\n");

}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

// If your host terminal support ansi escape code such as TeraTerm
// it can be use to simulate mouse cursor movement within terminal
#define USE_ANSI_ESCAPE   0

#define MAX_REPORT  4

uart_inst_t* uartdev = uart0;
static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII };

// Each HID instance can has multiple reports
static struct
{
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
}hid_info[CFG_TUH_HID];

static void process_kbd_report(hid_keyboard_report_t const *report);
static void process_mouse_report(hid_mouse_report_t const * report);
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

void hid_app_task(void)
{
  // nothing to do
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  // Interface protocol (hid_interface_protocol_enum_t)
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

  // By default host stack will use activate boot protocol on supported interface.
  // Therefore for this simple example, we only need to parse generic report descriptor (with built-in parser)
  if ( itf_protocol == HID_ITF_PROTOCOL_NONE )
  {
    hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
    printf("HID has %u reports \r\n", hid_info[instance].report_count);
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  switch (itf_protocol)
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      TU_LOG2("HID receive boot keyboard report\r\n");
      process_kbd_report( (hid_keyboard_report_t const*) report );
    break;

    case HID_ITF_PROTOCOL_MOUSE:
      TU_LOG2("HID receive boot mouse report\r\n");
      process_mouse_report( (hid_mouse_report_t const*) report );
    break;

    default:
      // Generic report requires matching ReportID and contents with previous parsed report info
      process_generic_report(dev_addr, instance, report, len);
    break;
  }
}

//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

static void process_kbd_report(hid_keyboard_report_t const *report)
{  
  if (report->keycode[0] == 0x49 && uartdev == uart1){
    board_led_write(false);
    uartdev = uart0;
  }
  else if (report->keycode[0] == 0x49 && uartdev == uart0){
    board_led_write(true);
    uartdev = uart1;
  }
  
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, 0x10);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->modifier);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->keycode[0]);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->keycode[1]);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->keycode[2]);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->keycode[3]);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->keycode[4]);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->keycode[5]);
  while (!uart_is_writable(uartdev)){}
  uart_puts(uartdev, "\n");

}

//--------------------------------------------------------------------+
// Mouse
//--------------------------------------------------------------------+

static void process_mouse_report(hid_mouse_report_t const * report)
{
  if (report->buttons == 0x05 && uartdev == uart1){
    board_led_write(false);
    uartdev = uart0;
  }
  else if (report->buttons == 0x05 && uartdev == uart0){
    board_led_write(true);
    uartdev = uart1;
  }
  
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, 0x20);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->buttons);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->x);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->y);
  while (!uart_is_writable(uartdev)){}
  uart_putc_raw(uartdev, report->wheel);
  while (!uart_is_writable(uartdev)){}
  uart_puts(uartdev, "\n");

}

//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) dev_addr;

  uint8_t const rpt_count = hid_info[instance].report_count;
  tuh_hid_report_info_t* rpt_info_arr = hid_info[instance].report_info;
  tuh_hid_report_info_t* rpt_info = NULL;

  if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
  {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  }else
  {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    uint8_t const rpt_id = report[0];

    // Find report id in the arrray
    for(uint8_t i=0; i<rpt_count; i++)
    {
      if (rpt_id == rpt_info_arr[i].report_id )
      {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }

    report++;
    len--;
  }

  if (!rpt_info)
  {
    printf("Couldn't find the report info for this report !\r\n");
    return;
  }

  // For complete list of Usage Page & Usage checkout src/class/hid/hid.h. For examples:
  // - Keyboard                     : Desktop, Keyboard
  // - Mouse                        : Desktop, Mouse
  // - Gamepad                      : Desktop, Gamepad
  // - Consumer Control (Media Key) : Consumer, Consumer Control
  // - System Control (Power key)   : Desktop, System Control
  // - Generic (vendor)             : 0xFFxx, xx
  if ( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP )
  {
    switch (rpt_info->usage)
    {
      case HID_USAGE_DESKTOP_KEYBOARD:
        TU_LOG1("HID receive keyboard report\r\n");
        // Assume keyboard follow boot report layout
        process_kbd_report( (hid_keyboard_report_t const*) report );
      break;

      case HID_USAGE_DESKTOP_MOUSE:
        TU_LOG1("HID receive mouse report\r\n");
        // Assume mouse follow boot report layout
        process_mouse_report( (hid_mouse_report_t const*) report );
      break;

      default: break;
    }
  }
}
