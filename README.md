# usbselfserial
usbselfserial is a header-only C++ library for USB serial communication without inbuilt OS drivers. \
It allows an operating system without USB serial support (like iOS) the ability to use a USB serial adapter. 

## Warning ⚠️
usbselfserial is currently very experimental. \
It only supports CDC ACM and CH34X devices at the moment (but please feel free to PR a new driver)

## Example compilation
To compile the example, you just need libusb and a C++11 capable compiler.
* Ubuntu / Debian:
```
sudo apt-get install libusb-1.0-0-dev pkg-config
g++ example.cpp `pkg-config --libs --cflags libusb-1.0` -lutil -std=c++11 -o example
```
