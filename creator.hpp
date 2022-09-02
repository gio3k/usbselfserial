/**
 * usbselfserial by lotuspar (https://github.com/lotuspar)
 *
 * Inspired / based on:
 *     the usb-serial-for-android project made in Java,
 *         * which is copyright 2011-2013 Google Inc., 2013 Mike Wakerly
 *         * https://github.com/mik3y/usb-serial-for-android
 *     the Linux serial port drivers,
 *         * https://github.com/torvalds/linux/tree/master/drivers/usb/serial
 *     and the FreeBSD serial port drivers
 *         * https://github.com/freebsd/freebsd-src/tree/main/sys/dev/usb/serial
 * Some parts rewritten in C++ for usbselfserial!
 *     * (by the time you read this it could have a different name!)
 * - 2022
 */
#include "driver/base.hpp"
#include "output/base.hpp"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <type_traits>

namespace usbselfserial {

struct CreatorConnectionData {
    bool connected = false;
    libusb_device_handle* device_handle = NULL;
};

/**
 * Hotplug capable device instance creator
 * @tparam DeviceT Device / driver class to use
 * @tparam OutputT Output class to use
 */
template <class DeviceT, class OutputT> class Creator {
    static_assert(std::is_base_of<driver::BaseDevice, DeviceT>::value,
                  "DeviceT must inherit BaseDevice.");
    static_assert(std::is_base_of<output::BaseOutput, OutputT>::value,
                  "OutputT must inherit BaseOutput.");

protected:
    driver::BaseDevice* device = 0x0;
    output::BaseOutput* output = 0x0;
    CreatorConnectionData connection;

private:
    libusb_hotplug_callback_handle handle_callback;

    static int hotplug_callback(struct libusb_context* ctx,
                                struct libusb_device* dev,
                                libusb_hotplug_event event, void* user_data) {
        CreatorConnectionData* connection = (CreatorConnectionData*)user_data;
        if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
            int ret = libusb_open(dev, &connection->device_handle);
            if (ret != 0)
                printf("Failed to open connected device. code: %i (%s)\n", ret,
                       libusb_error_name(ret));
            connection->connected = true;
        } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
            connection->connected = false;
            connection->device_handle = NULL;
        }

        return 0;
    }

public:
    Creator(u16_t vendor_id, u16_t product_id, void* output_config = 0x0) {
        // Register hotplug callback
        int ret = libusb_hotplug_register_callback(
            NULL,
            static_cast<libusb_hotplug_event>(
                LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
            LIBUSB_HOTPLUG_NO_FLAGS, vendor_id, product_id,
            LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, &connection,
            &handle_callback);

        if (ret < 0)
            printf("Failed to create hotplug callback. code: %i (%s)\n", ret,
                   libusb_error_name(ret));

        // Create output
        printf("Creating output...\n");
        output = new OutputT(output_config);

        // Try to get device handle
        connection.device_handle =
            libusb_open_device_with_vid_pid(NULL, vendor_id, product_id);
        if (connection.device_handle != 0x0)
            connection.connected = true;
        else if (ret < 0)
            throw "Failed to set up hotplug and device not found.";
    }

    ~Creator() {
        // todo: actual clean up
        libusb_hotplug_deregister_callback(NULL, handle_callback);
    }

    /**
     * Attempt to create instances
     * @return true Device created
     * @return false No device created
     */
    bool Create() {
        if (!connection.connected) {
            if (device == NULL)
                return false;

            // If device instance still exists but the connection is gone...
            // try to finalize output
            output->TryFinalize();

            // If output hasn't finished...
            if (!output->HasFinished())
                return false; // return while we wait.

            if (output->HasLinkedDevice()) {
                // Unlink the device if required
                output->UnlinkDevice();
            } else {
                delete device;
                device = NULL;
            }

            return false;
        }

        if (device == NULL) {
            // Create new device
            printf("Creating new device instance (usb_handle %p)!\n",
                   connection.device_handle);
            device = new DeviceT(connection.device_handle);
            output->LinkDevice(device);
            return true;
        }

        return false;
    }

    DeviceT* GetDevice() { return (DeviceT*)device; }
    OutputT* GetOutput() { return (OutputT*)output; }
};

} // namespace usbselfserial