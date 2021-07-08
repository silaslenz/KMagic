"""
For one or more output devices. 
Acts as a keyboard/mouse and outputs keyboard/mouse reports sent to it over UART in this format, one byte each:
Keyboard:
- 0x10
- Modifier keys
- Keycode 0
- Keycode 1
- Keycode 2
- Keycode 3
- Keycode 4
- Keycode 5
- '\n'
Mouse:
- 0x20
- Mouse buttons
- X movement
- Y movement
- Wheel
- '\n'
See tinyusb and/or host code for details.

Uses Circuitpython together with Adafruits HID library.
"""
import board
import busio
import usb_hid

from adafruit_hid.keyboard import Keyboard
from adafruit_hid.mouse import Mouse
from adafruit_hid.keycode import Keycode

# Set up a keyboard device.
kbd = Keyboard(usb_hid.devices)
mouse = Mouse(usb_hid.devices)

modifier_translation = {
    1: 0xE0,
    2: 0xE1,
    4: 0xE2,
    8: 0xE3,
    16: 0xE4,
    32: 0xE5,
    64: 0xE6,
    128: 0xE7,
}
pressed_keys = set()
last_buttons = 0


def twos_comp(val, bits=8):
    """compute the 2's complement of int value val"""
    if (val & (1 << (bits - 1))) != 0:  # if sign bit is set e.g., 8bit: 128-255
        val = val - (1 << bits)  # compute negative value
    return val  # return positive value as is


uart = busio.UART(None, board.GP1, baudrate=115200)

while True:
    data = uart.readline()  # read up to 32 bytes

    if data is not None:
        print(data)
        if data[0] == 0x10 and len(data) == 9:
            modifier = data[1]
            keys = [data[2], data[3], data[4], data[5], data[6], data[7]]

            modifiers = []
            if modifier:

                modifier_bits = [int(x) for x in bin(modifier)[2:]][::-1]
                modifier_bits = [i for i, x in enumerate(modifier_bits) if x == 1]

                for modifier_bit in modifier_bits:
                    modifiers.append(modifier_translation[1 << modifier_bit])
            keys = set(modifiers + keys)
            keys.discard(0)
            print("keys", keys)
            for pressed_key in pressed_keys:
                if pressed_key not in keys:
                    kbd.release(pressed_key)
                    pressed_keys.remove(pressed_key)
            pressed_keys.update(keys)
            print("pressed keys", pressed_keys)
            if not keys:
                kbd.release_all()
                pressed_keys.clear()
            kbd.press(*keys)
        elif data[0] == 0x20 and len(data) == 6:
            buttons, x, y, wheel = (
                twos_comp(data[1]),
                twos_comp(data[2]),
                twos_comp(data[3]),
                twos_comp(data[4]),
            )
            print(int(buttons), int(x), int(y), int(wheel))
            mouse.move(x, y, wheel)

            if buttons and buttons != last_buttons:
                mouse.press(buttons)
                last_buttons = buttons
            elif last_buttons and not buttons:
                mouse.release_all()
                last_buttons = 0
