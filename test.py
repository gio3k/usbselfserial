from threading import Thread
from driver.ch34x import Ch34xDeviceHandler
import usb1

ctx = usb1.USBContext()

device = Ch34xDeviceHandler(ctx, 0x1a86, 0x7523, "/tmp/ptyU0")
device._set_baud_rate(250000)

def __event_loop():
    while True:
        ctx.handleEventsTimeout(1)

_event_thread = Thread(target=__event_loop)
_event_thread.daemon = True
_event_thread.start()