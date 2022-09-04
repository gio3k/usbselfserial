#include "argparse.hpp"
#include "uss/driver.hpp"
#include "uss/drivers/cdcacm/cdcacm.hpp"
#include "uss/drivers/ch34x/ch34x.hpp"
#include "uss/uss.hpp"
#include <cstdio>
#include <thread>

using namespace uss;

int main(int argc, char** argv) {
    bool active = true;
    argparse::ArgumentParser program("usbselfserial_creator");

    program.add_argument("-v", "--vid", "--vendor-id")
        .required()
        .scan<'x', uint16_t>()
        .help("specify the USB vendor ID.");

    program.add_argument("-p", "--pid", "--product-id")
        .required()
        .scan<'x', uint16_t>()
        .help("specify the USB product ID.");

    program.add_argument("--bus")
        .default_value<uint8_t>(0)
        .scan<'u', uint8_t>()
        .help("specify the USB bus.");

    program.add_argument("--port")
        .default_value<uint8_t>(0)
        .scan<'u', uint8_t>()
        .help("specify the USB port.");

    program.add_argument("-d", "--driver")
        .required()
        .help("specify the driver (ch34x, cdcacm).");

    program.add_argument("-o", "--output")
        .required()
        .help("specify the output location of the pty.");

    program.add_argument("-r", "--baudrate")
        .required()
        .scan<'u', uint32_t>()
        .default_value<uint32_t>(250000)
        .help("specify the baudrate.");

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    uint16_t arg_vid = program.get<uint16_t>("-v");
    uint16_t arg_pid = program.get<uint16_t>("-p");
    uint8_t arg_bus = program.get<uint8_t>("--bus");
    uint8_t arg_port = program.get<uint8_t>("--port");
    uint32_t arg_baudrate = program.get<uint32_t>("-r");
    std::string arg_driver = program.get<std::string>("-d");
    std::string arg_output = program.get<std::string>("-o");

    libusb_init(NULL);

    // Create a driver
    BaseDriver* driver;
    if (arg_driver == "ch34x") {
        // This is a driver for WinChipHead CH340/CH341/HL340 devices
        driver = new driver::ch34x::Ch34xDriver;
    } else if (arg_driver == "cdcacm") {
        // This is a driver for CDC ACM devices
        driver = new driver::cdcacm::CdcAcmDriver;
    } else {
        printf("Unknown driver type. Please use cdcacm or ch34x.\n");
        return 1;
    }

    printf("-> driver %s to pty %s\n", arg_driver.c_str(), arg_output.c_str());

    // Create an output
    // This outputs to a virtual serial port / PTY at /tmp/uss0
    uss::output::pty::PtyOutput output(NULL, arg_output.c_str(), true);

    // Set output transfer completion callback
    output.SetTransferCompletionCallback(
        [&output](int result) { output.RemoveDevice(); });

    // Create a device
    // This uses the device 1a86:7523 and supports hotplug
    uss::ctl::Hotpluggable ctl(
        {arg_vid, arg_pid, arg_bus, arg_port},
        [&output](uss::ctl::Hotpluggable* device) {
            output.SetDevice(device);
        }, // Device connected event
        [&output](uss::ctl::Hotpluggable* device) {
            output.EndTransfers();
        }); // Device disconnected event

    // Set device baud rate (250000)
    ctl.baud_rate = arg_baudrate;

    // Set the device driver to the CH34x one from before
    ctl.SetDriver(driver);

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
    delete driver;
    return 2;
}