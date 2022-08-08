"""
Base classes for usbpty,
Based on the usb-serial-for-android project made in Java
    * which is copyright 2011-2013 Google Inc., and copyright 2013 Mike Wakerly
    * https://github.com/mik3y/usb-serial-for-android
Parts rewritten in Python for usbpty!
    * (by the time you read this it could have a different name!)
    * usbpty by lotuspar (https://github.com/lotuspar)
- 2022
"""
import os
import pty
import fcntl
import termios
import errno
from enum import Enum
from threading import Lock, Thread
from usb1 import USBContext, USBDevice, USBDeviceHandle, USBInterface, USBEndpoint, USBTransfer
from usb1 import CAP_HAS_HOTPLUG, HOTPLUG_EVENT_DEVICE_ARRIVED, HOTPLUG_EVENT_DEVICE_LEFT

class DataBits(Enum):
    # 5 data bits.
    DATABITS_5 = 5
    # 6 data bits.
    DATABITS_6 = 6
    # 7 data bits.
    DATABITS_7 = 7
    # 8 data bits.
    DATABITS_8 = 8

class StopBits(Enum):
    # 1 stop bit.
    STOPBITS_1 = 1
    # 1.5 stop bits.
    STOPBITS_1_5 = 3
    # 2 stop bits.
    STOPBITS_2 = 2

class Parity(Enum):
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

class BaseUSBDeviceHandler:
    """
    Base class for device handlers
    Contains functions and attributes for handling a serial port from a USB device
    """

    def __init__(self) -> None:
        self._context: USBContext = None
        self._device: USBDevice = None
        self._handle: USBDeviceHandle = None
        self._interface: USBInterface = None
        self._write_endpoint: USBEndpoint = None
        self._read_endpoint: USBEndpoint = None

    @property
    def device(self) -> USBDevice:
        """ Returns the raw Device backing this port or None """
        return self._device

    @property
    def handle(self) -> USBDeviceHandle:
        """ Returns the device handle or None """
        return self._handle

    @property
    def context(self) -> USBContext:
        """ Returns USB context or None """
        return self._context

    @property
    def write_endpoint(self) -> USBEndpoint:
        """ Returns the write endpoint or None """
        return self._write_endpoint

    @property
    def read_endpoint(self) -> USBEndpoint:
        """ Returns the read endpoint or None """
        return self._read_endpoint

    def _set_device_specific(self) -> None:
        """
        Needs to be overridden!
        Should set _read_endpoint, _write_endpoint & _interface
        Also should claim interfaces & detach kernel drivers
        """
        raise Exception("Called base class function!")

    def _init(self) -> None:
        """
        Needs to be overridden!
        Should initialize connection
        """
        raise Exception("Called base class function!")

    def read(self, timeout: int) -> bytearray:
        """
        Needs to be overridden!
        Should read from serial port
        """
        raise Exception("Called base class function!")

    def write(self, src: bytearray, timeout: int) -> None:
        """
        Needs to be overridden!
        Should write to serial port
        """
        raise Exception("Called base class function!")

    def set_parameters(self, baud_rate: int, data_bits: int, stop_bits: int, parity: int) -> None:
        """
        Needs to be overridden!
        Should set connection parameters
        """
        raise Exception("Called base class function!")

    def set_break(self, value: bool) -> None:
        """
        Needs to be overridden!
        Should set break status
        """
        raise Exception("Called base class function!")

    def clear_input(self) -> None:
        """
        Needs to be overridden!
        Should clear input buffer
        """
        raise Exception("Called base class function!")

class CommonUSBDeviceHandler(BaseUSBDeviceHandler):
    def __init__(self, context: USBContext, vendor_id: any, product_id: any, pty_name: any):
        super(CommonUSBDeviceHandler, self).__init__()
        self._context = context
        self.__read_transfer = None # reading from USB, going to PTY
        self.__write_transfer = None # writing to USB, coming from PTY
        self.__write_lock = Lock()
        self.__write_buffer = bytearray()
        self.__pty_fd = None

        # Find device / handle
        self._device = self._context.getByVendorIDAndProductID(
            vendor_id,
            product_id
        )
        if self._device is None:
            raise Exception("USB device not found")

        # Set up hotplug
        if self._context.hasCapability(CAP_HAS_HOTPLUG):
            self._context.hotplugRegisterCallback(self.__hotplug_callback,
                events=HOTPLUG_EVENT_DEVICE_ARRIVED | HOTPLUG_EVENT_DEVICE_LEFT,
                vendor_id=vendor_id, product_id=product_id)
        else:
            print("USB context doesn't support hotplug")
            # Init device
            self.__open_device()

        # Init pty
        self.__pty_fd = create_pty(pty_name)


    def __open_device(self, device: USBDevice = None):
        if device is not None:
            self._device = device
        elif self._device is None:
            raise Exception("__open_device was not provided a device!")

        self._handle = self._device.open()
        if self._handle is None:
            raise Exception("Failed to open handle")

        # Set interface and endpoints
        self._set_device_specific()
        if self._interface is None:
            raise Exception("_set_device_specific didn't set device interface!")
        if self._read_endpoint is None:
            raise Exception("_set_device_specific didn't set read endpoint!")
        if self._write_endpoint is None:
            raise Exception("_set_device_specific didn't set write endpoint!")

        # Init connection
        self._init()
        print("debug [dev,int,rep,wep]: ", self._device, self._interface, self._read_endpoint, self._write_endpoint)

        # Init transfer
        # pt.1: device to pty 
        self.__read_transfer = self._handle.getTransfer(0)
        if self.__read_transfer is None:
            raise Exception("Failed to create read transfer")

        self.__read_transfer.setBulk(self._read_endpoint, 32, self.__read_callback, None)
        print("Prepared device to pty transfer")

        # pt.2: pty to device thread
        self.__write_transfer = self._handle.getTransfer(0)
        if self.__write_transfer is None:
            raise Exception("Failed to create write transfer")

        self.__write_transfer.setBulk(self._write_endpoint, self.__write_buffer, self.__write_callback, None)
        print("Prepared pty to device transfer")

        # pt.3: submit transfers
        self.__read_transfer.submit()
        self.__write_transfer.submit()
        print("Started transfers")

    def __read_callback(self, transfer: USBTransfer) -> None:
        buf = transfer.getBuffer()[:transfer.getActualLength()]
        #print("from printer to pty >", buf.hex())
        #print("from printer to pty full >", transfer.getBuffer().hex())
        self.__write_lock.acquire()
        os.write(self.__pty_fd, buf)
        self.__write_lock.release()
        self.__read_transfer.submit()

    def __write_callback(self, transfer: USBTransfer) -> None:
        self.__write_lock.acquire()
        #self.__write_buffer = os.read(self.__pty_fd, 32)
        self.__write_lock.release()
        self.__write_transfer.submit()

    def __hotplug_callback(self, context: USBContext, device: USBDevice, event):
        if event is HOTPLUG_EVENT_DEVICE_LEFT:
            if self._device is not None:
                self._handle.close()
                self._device.close()
                self._device = None
        else:
            self.__open_device(device)

def create_pty(ptyname):
    """
    This is mostly / fully taken from the Klipper / Klippy source code:
    https://github.com/Klipper3d/klipper/blob/a709ba43af8edaaa307775ed73cb49fac2b5e550/scripts/avrsim.py#L143
    """
    try:
        os.unlink(ptyname)
    except os.error:
        pass
    mfd, sfd = pty.openpty()
    filename = os.ttyname(sfd)
    os.chmod(filename, 0o666)
    os.symlink(filename, ptyname)
    tcattr = termios.tcgetattr(mfd)
    tcattr[3] = tcattr[3] & ~termios.ECHO
    termios.tcsetattr(mfd, termios.TCSAFLUSH, tcattr)
    return mfd
