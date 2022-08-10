"""
CH34X driver for usbpty,
Based on the usb-serial-for-android project made in Java
    * which is copyright 2011-2013 Google Inc., and copyright 2013 Mike Wakerly
    * https://github.com/mik3y/usb-serial-for-android
Parts rewritten in Python for usbpty!
    * (by the time you read this it could have a different name!)
    * usbpty by lotuspar (https://github.com/lotuspar)
- 2022
"""
from .handler import CommonUSBDeviceHandler, DataBits, StopBits, Parity
from usb1 import USBContext, USBErrorAccess

CH34X_LCR_ENABLE_RX = 0x80
CH34X_LCR_ENABLE_TX = 0x40
CH34X_LCR_MARK_SPACE  = 0x20
CH34X_LCR_PAR_EVEN = 0x10
CH34X_LCR_ENABLE_PAR = 0x08
CH34X_LCR_STOP_BITS_2 = 0x04
CH34X_LCR_CS8 = 0x03
CH34X_LCR_CS7 = 0x02
CH34X_LCR_CS6 = 0x01
CH34X_LCR_CS5 = 0x00

CH34X_SCL_DTR = 0x20
CH34X_SCL_RTS = 0x40

CH34X_USB_READ_ENDPOINT = 0x82
CH34X_USB_WRITE_ENDPOINT = 0x02
CH34X_USB_TIMEOUT = 5000

CH34X_CTL_TO_DEVICE = 0x40
CH34X_CTL_TO_HOST = 0xC0

CH34X_DEFAULT_BAUD_RATE = 115200
CH34X_BAUDBASE_FACTOR = 1532620800
CH34X_BAUDBASE_DIVMAX = 3

class Ch34xDeviceHandler(CommonUSBDeviceHandler):
    """
    Device handler for CH340 / CH341 USB Serial devices.
    Based on code from FreeBSD, Linux and usb-serial-for-android
    """
    def __init__(self, context: USBContext, vendor_id: any, product_id: any, pty_name: any):
        self._dtr = False
        self._rts = False
        self._version = None
        super().__init__(context, vendor_id, product_id, pty_name)

    def _set_device_specific(self) -> None:
        self._interface = 0
        try:
            self._handle.detachKernelDriver(self._interface)
        except USBErrorAccess:
            print("ch34x: Failed to detach kernel driver!")

        self._read_endpoint = CH34X_USB_READ_ENDPOINT
        self._write_endpoint = CH34X_USB_WRITE_ENDPOINT
        self._handle.claimInterface(self._interface)

    def __control_out(self, request, value, index):
        """ Send control command to the USB device """
        return self._handle.controlWrite(
            CH34X_CTL_TO_DEVICE,
            request, value, index, [], CH34X_USB_TIMEOUT)

    def __control_in(self, request, value, index, length):
        """ Send control command to the USB device and recieve input """
        return self._handle.controlRead(
            CH34X_CTL_TO_HOST,
            request, value, index, length, CH34X_USB_TIMEOUT)

    def __check_state(self, msg, request, value, expected):
        """ Send control command to the USB device and validate input """
        buf = self.__control_in(request, value, 0, len(expected))
        ret = len(buf)
        if ret < 0:
            raise Exception(f"Failed check_state: send command failed [{msg}]")

        if ret != len(expected):
            raise Exception(f"Failed check_state: expected {len(expected)} bytes but got {ret} [{msg}]")

        for i, v in enumerate(expected):
            if v == -1:
                continue

            cur = buf[i] & 0xFF
            if v != cur:
                raise Exception(f"Failed check_state: (i{i}) expected 0x{v:X} byte, but got 0x{cur:X} [{msg}]")

    def __set_control_lines(self):
        # also seems to be a handshake ?
        value = 0
        if self._dtr:
            value |= CH34X_SCL_DTR
        if self._rts:
            value | CH34X_SCL_RTS

        if self._version < 20:
            # todo: implement ch34x version 10 handshake
            # UCHCOM_REG_STAT1	0x06
            # UCHCOM_REQ_SET_DTRRTS	0xA4
            # https://github.com/openbsd/src/blob/08933a0defbec6cd08faa2ea5d07912ace16b3ae/sys/dev/usb/uchcom.c#L510
            raise Exception("Unimplemented chip version!")
        else:
            if self.__control_out(0xA4, value, 0) < 0:
                raise Exception("Failed to set control lines")

    def _set_baud_rate(self, rate: int):
        """
        Calculate and set CH340 baudrate
        Based on setBaudRate from usb-serial-for-android/Ch34xSerialDriver.java
        """
        factor = 0
        divisor = 0

        if rate == 921600:
            divisor = 7
            factor = 0xF300
        else:
            factor = CH34X_BAUDBASE_FACTOR // rate
            divisor = CH34X_BAUDBASE_DIVMAX

            while factor > 0xFFF0 and divisor > 0:
                factor >>= 3
                divisor -= 1

            if factor > 0xFFF0:
                raise ValueError(f"unsupported baudrate: {rate}")

            factor = 0x10000 - factor

        divisor |= 0x0080 # or ch341a waits until buffer full

        val1 = int((factor & 0xFF00) | divisor)
        val2 = int(factor & 0xFF)
        ret = self.__control_out(0x9A, 0x1312, val1)
        if ret < 0:
            raise Exception("(1) failed to set baudrate!")

        ret = self.__control_out(0x9A, 0x0F2C, val2)
        if ret < 0:
            raise Exception("(2) failed to set baudrate!")

    def _init(self) -> None:
        # Get chip version
        verbf = self.__control_in(0x5F, 0, 0, 8)
        if (hasattr(verbf, "__len__") and len(verbf) == 0) or verbf == 0:
                raise Exception("init: Chip version check failed")
        self._version = verbf[0]
        print(f"ch34x: Chip version {self._version:X}")

        # Clear chip
        if self.__control_out(0xA1, 0, 0) < 0:
            raise Exception("init: Chip clear failed")
        
        # Set baud rate after chip clear
        self._set_baud_rate(CH34X_DEFAULT_BAUD_RATE)

        self.__check_state("init #4", 0x95, 0x2518, [-1, 0x00])

        # Set line control register
        if self.__control_out(0x9a, 0x2518, CH34X_LCR_ENABLE_RX | CH34X_LCR_ENABLE_TX | CH34X_LCR_CS8) < 0:
            raise Exception("init: Set line control failed")

        self.__check_state("init #6", 0x95, 0x0706, [-1, -1])

        # Reset chip
        if self.__control_out(0xA1, 0x501F, 0xD90A) < 0:
            raise Exception("init: Chip reset failed")

        # Set baud rate after chip reset
        self._set_baud_rate(CH34X_DEFAULT_BAUD_RATE)

        # Turn on DTR & RTS
        # todo: why?
        self._dtr = True
        self._rts = True
        self.__set_control_lines()

        #self.__check_state("init #10", 0x95, 0x0706, [-1, -1])

        self._set_baud_rate(self._baud_rate)

    def set_break(self, value: bool) -> None:
        req = self.__control_in(0x95, 0x1805, 0, 2)
        if (hasattr(req, "__len__") and len(req) == 0) or req == 0:
            raise Exception("Break check failed")

        if value:
            req[0] &= ~1
            req[1] &= ~0x40
        else:
            req[0] |= 1
            req[1] |= 0x40
        
        ctl = (req[1] & 0xff) << 8 | (req[0] & 0xff)
        if self.__control_out(0x9A, 0x1805, ctl) < 0:
            raise Exception("Break set failed")

    def set_dtr(self, value: bool):
        self._dtr = value
        self.__set_control_lines()

    def set_rts(self, value: bool):
        self._rts = value
        self.__set_control_lines()

    def set_parameters(self, baud_rate: int, data_bits: int, stop_bits: int, parity: int) -> None:
        if baud_rate <= 0:
            raise ValueError(f"baudrate of {baud_rate} is invalid!")
        self._set_baud_rate(baud_rate)

        # Prepare LCR
        lcr = CH34X_LCR_ENABLE_RX | CH34X_LCR_ENABLE_TX

        if data_bits == DataBits.DATABITS_5:
            lcr |= CH34X_LCR_CS5
        elif data_bits == DataBits.DATABITS_6:
            lcr |= CH34X_LCR_CS6
        elif data_bits == DataBits.DATABITS_7:
            lcr |= CH34X_LCR_CS7
        elif data_bits == DataBits.DATABITS_8:
            lcr |= CH34X_LCR_CS8
        else:
            raise ValueError(f"Unsupported number of data bits: {data_bits}")

        if parity == Parity.PARITY_NONE:
            pass
        elif parity == Parity.PARITY_ODD:
            lcr |= CH34X_LCR_ENABLE_PAR
        elif parity == Parity.PARITY_EVEN:
            lcr |= CH34X_LCR_ENABLE_PAR | CH34X_LCR_PAR_EVEN
        elif parity == Parity.PARITY_MARK:
            lcr |= CH34X_LCR_ENABLE_PAR | CH34X_LCR_MARK_SPACE
        elif parity == Parity.PARITY_SPACE:
            lcr |= CH34X_LCR_ENABLE_PAR | CH34X_LCR_MARK_SPACE | CH34X_LCR_PAR_EVEN
        else:
            raise ValueError(f"Unsupported parity mode: {parity}")

        if stop_bits == StopBits.STOPBITS_1:
            pass
        elif stop_bits == StopBits.STOPBITS_1_5:
            raise ValueError("Unsupported number of stop bits: 1.5")
        elif stop_bits == StopBits.STOPBITS_2:
            lcr |= CH34X_LCR_STOP_BITS_2
        else:
            raise ValueError(f"Unsupported number of stop bits: {stop_bits}")

        ret = self.__control_out(0x9A, 0x2518, lcr)
        if ret < 0:
            raise SystemError("Failed to set control byte!")
