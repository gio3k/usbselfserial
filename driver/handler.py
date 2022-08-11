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
from queue import Queue
import termios
import errno
from enum import Enum
from threading import Lock, Thread
from time import sleep
from usb1 import USBContext, USBDevice, USBDeviceHandle, USBInterface, USBEndpoint, USBTransfer
from usb1 import USBErrorPipe, USBErrorOther, USBError
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
        self._alive: bool = False # is the device connected?
        self._handled: bool = False # is the device actually being handled?
        self._context: USBContext = None
        self._device: USBDevice = None
        self._handle: USBDeviceHandle = None
        self._interface: USBInterface = None
        self._write_endpoint: USBEndpoint = None
        self._read_endpoint: USBEndpoint = None
        self._baud_rate: int = 9600

    @property
    def alive(self) -> bool:
        """ Returns whether or not the device is active """
        return self._alive

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

    @property
    def baud_rate(self) -> int:
        """ Returns the current baudrate """
        return self._baud_rate

    @baud_rate.setter
    def baud_rate(self, value: int) -> None:
        """ Sets the current baudrate """
        self._baud_rate = value
        if self._alive:
            self._set_baud_rate(value)

    def _set_device_specific(self) -> None:
        """
        Needs to be overridden!
        Should set _read_endpoint, _write_endpoint & _interface
        Also should claim interfaces & detach kernel drivers
        """
        raise Exception("Called base class function!")

    def _set_baud_rate(self, rate: int) -> None:
        """
        Needs to be overridden!
        Should set baud rate if possible
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
    def __init__(self, vendor_id: hex, product_id: hex, pty_name: any, baud_rate: int):
        super(CommonUSBDeviceHandler, self).__init__()
        self.__read_transfer: USBTransfer = None # reading from USB, going to PTY
        self.__write_transfer: USBTransfer = None # writing to USB, coming from PTY
        self.__write_waiting: bool = False
        self.__write_buffer: bytes = bytes()

        self.__thread_pty_read: Thread = None
        self.__thread_ctx_event: Thread = None

        self._handled = True
        self._context: USBContext = USBContext()
        self._baud_rate = baud_rate

        # Init pty
        self.__pty_fd = create_pty(pty_name)

        # Start event thread
        print("Starting event thread!")
        self.__thread_ctx_event = Thread(target=self.__threadloop_ctx_event)
        self.__thread_ctx_event.start()

        # Set up hotplug
        if self._context.hasCapability(CAP_HAS_HOTPLUG):
            print("Waiting for device...")
            self._context.hotplugRegisterCallback(self.__hotplug_callback,
                events=HOTPLUG_EVENT_DEVICE_ARRIVED | HOTPLUG_EVENT_DEVICE_LEFT,
                vendor_id=vendor_id, product_id=product_id)
            self._device = self._context.getByVendorIDAndProductID(
                vendor_id,
                product_id
            )
        else:
            print("USB context doesn't support hotplug")
            # Init device
            self._device = self._context.getByVendorIDAndProductID(
                vendor_id,
                product_id
            )
            if self._device is None:
                raise Exception("USB device not found")
            self.__open_device()

        while True:
            if not self._handled:
                break
            try:
                while True:
                    sleep(1)
                    if not self._alive and self._device is not None:
                        print("opening device:", self._device)
                        self.__open_device()
            except (KeyboardInterrupt, SystemExit):
                self._handled = False
            
        self._handled = False
        self.__thread_pty_read.join()
        self.__thread_ctx_event.join()
        
    def __start_threads(self):
        # Create thread(s)
        if self.__thread_pty_read is None:
            print("Starting reader thread!")
            self.__thread_pty_read = Thread(target=self.__threadloop_pty_read)
            self.__thread_pty_read.start()
    
    def __open_device(self, device: USBDevice = None):
        if self._handle is not None:
            print("Closing previous handle before opening new device")
            self._handle.close()
            self._handle = None

        # Make sure previous buffers / data are cleared / ready
        print("Clearing buffers!")
        self.__write_buffer = bytes()
        self.__read_transfer = None
        self.__write_transfer = None

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

        # Set alive status
        self._alive = True

        # Make sure threads have started now
        self.__start_threads()

        # Init transfer
        # pt.1: device to pty
        self.__read_transfer = self._handle.getTransfer(0)
        if self.__read_transfer is None:
            raise Exception("Failed to create read transfer")

        self.__read_transfer.setBulk(self._read_endpoint, 32, self.__read_callback)
        print("Prepared device to pty transfer")

        # pt.2: pty to device thread
        self.__write_transfer = self._handle.getTransfer(0)
        if self.__write_transfer is None:
            raise Exception("Failed to create write transfer")

        #self.__write_transfer.setBulk(self._write_endpoint, 0, self.__write_callback)
        print("Prepared pty to device transfer")

        # pt.3: submit transfers
        self.__read_transfer.submit()
        print("Started transfers")

    def __read_callback(self, transfer: USBTransfer) -> None:
        if not self._alive:
            print("dooming!")
            transfer.doom()
            return
        try:
            buf = transfer.getBuffer()[:transfer.getActualLength()]
            os.write(self.__pty_fd, buf)
            #print("[to pty]", buf)
            if self._alive:
                self.__read_transfer.submit()
        except (USBError):
            print("Read transfer submit failed... device disconnect?")
            self._alive = False
            self._device = None

    def __write_callback(self, transfer: USBTransfer) -> None:
        if not self._alive:
            transfer.doom()
            return

        #print("(ack) [to spr]")
        self.__write_waiting = False

    def __threadloop_ctx_event(self) -> None:
        while True:
            if not self._handled:
                return
            try:
                self._context.handleEventsTimeout(1)
            except (KeyboardInterrupt, SystemExit):
                self._handled = False

    def __threadloop_pty_read(self) -> None:
        while True:
            if not self._handled:
                return
            try:
                while True:
                    if not self._alive:
                        if self.__write_transfer is not None:
                            self.__write_transfer.doom()
                        continue
                    if self.__write_transfer is not None and not self.__write_waiting:
                        self.__write_buffer = os.read(self.__pty_fd, 32)
                        self.__write_transfer.setBulk(self._write_endpoint, self.__write_buffer, self.__write_callback)
                        self.__write_waiting = True
                        #print("(submitting) [to spr]", self.__write_buffer)
                        self.__write_transfer.submit()
            except (KeyboardInterrupt, SystemExit):
                self._handled = False

    def __hotplug_callback(self, context: USBContext, device: USBDevice, event):
        if event is HOTPLUG_EVENT_DEVICE_LEFT:
            print("Device disconnected!")
            self._alive = False
            self._device = None
        elif event is HOTPLUG_EVENT_DEVICE_ARRIVED:
            print("Device connected!", device)
            self._device = device

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
