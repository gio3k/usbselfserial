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
#pragma once
#include "../controller.hpp"
#include "../device.hpp"
#include "../error.hpp"
#include <cstdio>
#include <functional>
#include <libusb-1.0/libusb.h>

namespace uss {
namespace ctl {

struct HotpluggableInstanceData {
    libusb_device_handle* provisional_usb_handle = 0x0;
    libusb_device_handle* usb_handle = 0x0;
    libusb_device* usb_device = 0x0;
    bool handle_disconnect = false;
};

class Hotpluggable : public BaseDevice, public BaseController {
    HotpluggableInstanceData instance;
    libusb_hotplug_callback_handle callback_handle;
    std::function<void(Hotpluggable*)> connect_callback = NULL;
    std::function<void(Hotpluggable*)> disconnect_callback = NULL;

    static int HotplugCallback(struct libusb_context* ctx,
                               struct libusb_device* dev,
                               libusb_hotplug_event event, void* user_data) {
        HotpluggableInstanceData* instance =
            (HotpluggableInstanceData*)user_data;

        if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
            instance->usb_handle = NULL;
            instance->usb_device = NULL;
            int ret = libusb_open(dev, &instance->provisional_usb_handle);
            if (ret < 0)
                throw error::LibUsbErrorException("Error on hotplug connect",
                                                  ret);

        } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
            instance->provisional_usb_handle = NULL;
            instance->usb_handle = NULL;
            instance->usb_device = NULL;
            instance->handle_disconnect = true;
        }

        return 0;
    }

public:
    Hotpluggable(uint16_t vid, uint16_t pid,
                 std::function<void(Hotpluggable*)> _connect_callback = NULL,
                 std::function<void(Hotpluggable*)> _disconnect_callback = NULL)
        : connect_callback(_connect_callback),
          disconnect_callback(_disconnect_callback) {
        int ret = libusb_hotplug_register_callback(
            NULL,
            static_cast<libusb_hotplug_event>(
                LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
            LIBUSB_HOTPLUG_NO_FLAGS, vid, pid, LIBUSB_HOTPLUG_MATCH_ANY,
            HotplugCallback, &instance, &callback_handle);

        if (ret < 0)
            throw error::LibUsbErrorException(
                "Error on hotplug callback register", ret);

        instance.provisional_usb_handle =
            libusb_open_device_with_vid_pid(NULL, vid, pid);
    }

    libusb_device* GetUsbDevice() override {
        if (instance.usb_handle == NULL)
            return NULL;
        if (instance.usb_device == NULL)
            instance.usb_device = libusb_get_device(instance.usb_handle);
        return instance.usb_device;
    }

    libusb_device_handle* GetUsbHandle() override {
        return instance.usb_handle;
    }

    void Update() override {
        if (instance.handle_disconnect) {
            instance.handle_disconnect = false;
            if (disconnect_callback != NULL)
                disconnect_callback(this);
        }

        if (instance.usb_handle != NULL)
            return;

        if (instance.provisional_usb_handle == NULL)
            return;

        instance.usb_handle = instance.provisional_usb_handle;
        instance.provisional_usb_handle = NULL;

        Reinitialize();

        if (connect_callback != NULL)
            connect_callback(this);
    }
};

} // namespace ctl
} // namespace uss