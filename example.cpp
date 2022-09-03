#include "uss/drivers/ch34x/ch34x.hpp"
#include "uss/uss.hpp"

using namespace uss;

int main(int argc, char** argv) {
    libusb_init(NULL);

    // Create a driver
    // This is a driver for WinChipHead CH340/CH341/HL340 devices
    driver::ch34x::Ch34xDriver driver;

    // Create an output
    // This outputs to a virtual serial port / PTY at /tmp/uss0
    uss::output::pty::PtyOutput output(NULL, "/tmp/uss0", true);

    // Create a device
    // This uses the device 1a86:7523 and supports hotplug
    uss::ctl::Hotpluggable ctl(
        0x1a86, 0x7523,
        [&output](uss::ctl::Hotpluggable* device) {
            output.SetDevice(device);
        }, // Device connected event
        [&output](uss::ctl::Hotpluggable* device) {
            output.RemoveDevice();
        }); // Device disconnected event

    // Set device baud rate (250000)
    ctl.baud_rate = 250000;

    // Set the device driver to the CH34x one from before
    ctl.SetDriver(&driver);

    struct timeval tv = {0L, 0L};
    while (true) {
        // For each loop iteration...
        try {
            // Handle LibUSB events
            int ret = libusb_handle_events_timeout(NULL, &tv);
            if (ret < 0)
                goto clean;

            // Update device
            ctl.Update();

            // Update output
            output.Update();
        } catch (const char* error) {
            printf("Error caught during main loop: %s\n", error);
            goto clean;
        }
    }

clean:
    libusb_exit(NULL);
    return 2;
}