#include "usbselfserial.hpp"
#include <chrono>
#include <cstdio>
#include <stdio.h>
#include <thread>

int main() {
    int ret;

    // Init libusb context
    ret = libusb_init(NULL);
    if (ret < 0) {
        printf("Failed to initialize libusb! code %i (%s)\n", ret,
               libusb_error_name(ret));
        return 1;
    }

    // Configure pty output
    usbselfserial::output::pty::PtyOutputConfig config;
    config.location = "/tmp/usb_ch34x";
    config.retain_pty = false;

    // Set up Creator for ch34x device with pty output
    usbselfserial::Creator<usbselfserial::driver::Ch34xDevice,
                           usbselfserial::output::PtyOutput>
        creator(0x1a86, 0x7523, &config);

    // Start loop
    while (true) {
        struct timeval tv = {0L, 0L};
        try {
            // Make sure Creator has created things
            if (creator.Create()) {
                // .Create() returns true if the device was just created, so use
                // this time to set the baudrate
                creator.GetDevice()->SetBaudRate(250000);
            }

            // Handle libusb events (this is required!)
            ret = libusb_handle_events_timeout(NULL, &tv);
            if (ret < 0)
                goto clean;

            // Update output if it exists (you could maybe use a thread for
            // this)
            if (creator.GetOutput())
                creator.GetOutput()->Update();
            else
                std::this_thread::sleep_for(
                    std::chrono::seconds(2)); // Sleep if no output exists yet
        } catch (const char* error) {
            printf("Error caught: %s\n", error);
            goto clean;
        }
    }

clean:
    libusb_exit(NULL);
    return 0;
}