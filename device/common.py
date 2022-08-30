"""
Base classes for usbselfserial,
Inspired / based on:
    the usb-serial-for-android project made in Java,
        * which is copyright 2011-2013 Google Inc., and copyright 2013 Mike Wakerly
        * https://github.com/mik3y/usb-serial-for-android
    the Linux serial port drivers,
        * https://github.com/torvalds/linux/tree/master/drivers/usb/serial
    and the FreeBSD serial port drivers
        * https://github.com/freebsd/freebsd-src/tree/main/sys/dev/usb/serial
Some parts rewritten in Python for usbselfserial!
    * (by the time you read this it could have a different name!)
    * usbselfserial by lotuspar (https://github.com/lotuspar)
- 2022
"""
from enum import Enum

class DataBits(Enum):
    """ Serial port databits value """
    # 5 data bits.
    DATABITS_5 = 0
    # 6 data bits.
    DATABITS_6 = 1
    # 7 data bits.
    DATABITS_7 = 2
    # 8 data bits.
    DATABITS_8 = 3

class StopBits(Enum):
    """ Serial port stopbits value """
    # 1 stop bit.
    STOPBITS_1 = 0
    # 1.5 stop bits.
    STOPBITS_1_5 = 1
    # 2 stop bits.
    STOPBITS_2 = 2

class Parity(Enum):
    """ Serial port parity value """
    # No parity.
    PARITY_NONE = 0
    # Odd parity.
    PARITY_ODD = 1
    # Even parity.
    PARITY_EVEN = 2
    # Mark parity.
    PARITY_MARK = 3
    # Space parity.
    PARITY_SPACE = 4
