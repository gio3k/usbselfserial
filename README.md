# usbselfserial
usbselfserial is a header-only C++ library for USB serial communication without inbuilt OS drivers. \
It gives an operating system without USB serial support (like iOS) the ability to use a USB serial adapter. 

## Warning ⚠️
usbselfserial is currently very experimental. \
It only supports CDC ACM and CH34X devices at the moment (but please feel free to PR a new driver)

## Example compilation
To compile the example, you just need libusb and a C++11 capable compiler.
* Ubuntu / Debian:
```
sudo apt-get install libusb-1.0-0-dev pkg-config
g++ example.cpp -O2 `pkg-config --libs --cflags libusb-1.0` -lutil -lpthread -std=c++17 -o example
```
* macOS:
```
brew install libusb
g++ -std=c++11 -O2 -lusb example.cpp -o example
```
You might need to run the example as root so the device can be detached from the OS drivers.
