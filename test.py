import os
from threading import Thread
from driver.ch34x import Ch34xDeviceHandler
import usb1

ctx = usb1.USBContext()

device = Ch34xDeviceHandler(ctx, 0x1a86, 0x7523, "/tmp/ptyU0")
device._set_baud_rate(250000)

def __event_loop():
    try:
        while True:
            ctx.handleEventsTimeout(1)
    except (KeyboardInterrupt, SystemExit):
        if os.path.exists("/tmp/ptyU0"):
            os.unlink("/tmp/ptyU0")
        print('Closing!')
        device._close()

while True:
    __event_loop()