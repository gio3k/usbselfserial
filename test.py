import os
from threading import Thread
from driver.ch34x import Ch34xDeviceHandler
import usb1

ctx = usb1.USBContext()

device = Ch34xDeviceHandler(ctx, 0x1a86, 0x7523, "/tmp/ptyU0")
device.baud_rate = 250000