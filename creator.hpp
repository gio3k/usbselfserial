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
#include "types.hpp"
#include <chrono>
#include <cstdio>
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <thread>

namespace usbselfserial {

template <typename DeviceT, typename OutputT> struct CreatorInstanceData {
    libusb_device_handle* device_handle = 0x0;
    DeviceT* device_instance = 0x0;
    OutputT* output_instance = 0x0;
    void* output_config_data = 0x0;
    bool connected = false;
};

/**
 * Hotplug capable device instance creator
 * @tparam DeviceT Device / driver class to use
 * @tparam OutputT Output class to use
 */
template <typename DeviceT, typename OutputT> class Creator {
private:
    CreatorInstanceData<DeviceT, OutputT> data;
    libusb_hotplug_callback_handle handle_callback;

    static int hotplug_callback(struct libusb_context* ctx,
                                struct libusb_device* dev,
                                libusb_hotplug_event event, void* user_data) {
        CreatorInstanceData<DeviceT, OutputT>* data =
            (CreatorInstanceData<DeviceT, OutputT>*)user_data;

        if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
            int ret = libusb_open(dev, &data->device_handle);
            if (ret != 0)
                printf("Failed to open connected device. code: %i (%s)\n", ret,
                       libusb_error_name(ret));
            data->connected = true;
        } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
            data->connected = false;
        }

        return 0;
    }

public:
    Creator(u16_t vendor_id, u16_t product_id, void* output_config = 0x0) {
        // Register hotplug callback
        int ret = libusb_hotplug_register_callback(
            NULL,
            LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
            0, vendor_id, product_id, LIBUSB_HOTPLUG_MATCH_ANY,
            hotplug_callback, &data, &handle_callback);

        if (ret != 0)
            printf("Failed to create hotplug callback. code: %i (%s)\n", ret,
                   libusb_error_name(ret));

        // Set config data
        data.output_config_data = output_config;

        // Try to get device handle
        data.device_handle =
            libusb_open_device_with_vid_pid(NULL, vendor_id, product_id);
        if (data.device_handle != 0x0)
            data.connected = true;
    }

    ~Creator() { libusb_hotplug_deregister_callback(NULL, handle_callback); }

    bool Create() {
        if (!data.connected) {
            if (data.output_instance == 0x0 && data.device_instance == 0x0)
                return false;

            // if output instance exists...
            if (data.output_instance != 0x0) {
                // request completion
                data.output_instance->TryFinalize();

                // if it's already finished...
                if (data.output_instance->HasFinished()) {
                    // delete the instance
                    delete data.output_instance;
                    data.output_instance = 0x0;
                }
            }

            // if device instance exists but output instance doesn't...
            if (data.output_instance == 0x0 && data.device_instance != 0x0) {
                // delete the instance
                delete data.device_instance;
                data.device_instance = 0x0;
            }

            return false;
        }

        bool out = false;
        if (data.device_instance == 0x0) {
            data.device_instance = new DeviceT(data.device_handle);
            out = true;
        }
        if (data.output_instance == 0x0) {
            data.output_instance =
                new OutputT(*data.device_instance, data.output_config_data);
            out = true;
        }

        return out;
    }

    DeviceT* GetDevice() { return data.device_instance; }
    OutputT* GetOutput() { return data.output_instance; }
};

} // namespace usbselfserial