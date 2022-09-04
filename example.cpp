#include "uss/drivers/cdcacm/cdcacm.hpp"
#include "uss/drivers/ch34x/ch34x.hpp"
#include "uss/uss.hpp"
#include <cstdio>
#include <thread>

using namespace uss;

int main(int argc, char** argv) {
    bool active = true;
    libusb_init(NULL);

    // Create a driver
    // This is a driver for WinChipHead CH340/CH341/HL340 devices
    driver::ch34x::Ch34xDriver driver;

    // Create an output
    // This outputs to a virtual serial port / PTY at /tmp/uss0
    uss::output::pty::PtyOutput output(NULL, "/tmp/uss0", true);

    // Set output transfer completion callback
    output.SetTransferCompletionCallback(
        [&output](int result) { output.RemoveDevice(); });

    // Create a device
    // This uses the device 1a86:7523 and supports hotplug
    uss::ctl::Hotpluggable ctl(
        0x1a86, 0x7523,
        [&output](uss::ctl::Hotpluggable* device) {
            output.SetDevice(device);
        }, // Device connected event
        [&output](uss::ctl::Hotpluggable* device) {
            output.EndTransfers();
        }); // Device disconnected event

    // Set device baud rate (250000)
    ctl.baud_rate = 250000;

    // Set the device driver to the CH34x one from before
    ctl.SetDriver(&driver);

    std::thread output_thread([&output, &active]() {
        while (active) {
            output.HandleEvents();
        }
    });

    struct timeval tv = {1L, 0L};
    while (active) {
        // For each loop iteration...
        try {
            // Handle LibUSB events
            int ret = libusb_handle_events_timeout_completed(NULL, &tv, NULL);
            if (ret < 0)
                goto clean;

            // Update device
            ctl.Update();

        } catch (const char* error) {
            printf("Error caught during main loop: %s\n", error);
            goto clean;
        }
    }

clean:
    active = false;
    output_thread.join();
    libusb_exit(NULL);
    return 2;
}