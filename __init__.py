#!/usr/bin/env python
"""
USBPTY
"""
import argparse
import importlib
USBPTY_VERSION = 1.0
ARGS: argparse.Namespace = None

def hexfstr(x):
    return int(x, 0)

parser = argparse.ArgumentParser(
    description="create virtual serial port / pty from USB device directly")
parser.add_argument("-p", "--path",
                dest="path", required=True, help="Path to new virtual serial port")
parser.add_argument("-d", "--driver",
                dest="driver", required=True, help="Driver to use")
parser.add_argument("-vid", "--vendor-id", 
                dest="vendor_id", required=True, help="USB device vendor", type=hexfstr)
parser.add_argument("-pid", "--product-id", 
                dest="product_id", required=True, help="USB device product", type=hexfstr)
parser.add_argument("-b", "--baud", "--baudrate", 
                dest="baud_rate", default=9600, help="Device baudrate", type=int)
ARGS = parser.parse_args()

# attempt to import driver
try:
    driver = importlib.import_module(f"driver.{ARGS.driver}")
except ModuleNotFoundError:
    print("driver wasn't found! please look in the driver/ directory")
except e:
    print("unknown import error")
    raise e

# create device
device = driver.ModuleDeviceHandler(
    vendor_id=ARGS.vendor_id, product_id=ARGS.product_id,
    pty_name=ARGS.path, baud_rate=ARGS.baud_rate)
print("setting baud rate")
