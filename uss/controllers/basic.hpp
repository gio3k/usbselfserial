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
#include <libusb-1.0/libusb.h>

namespace uss {
namespace ctl {

class Basic : public BaseDevice, public BaseController {
private:
    libusb_device_handle* usb_handle = 0x0;
    libusb_device* usb_device = 0x0;

public:
    Basic(uint16_t vid, uint16_t pid) {
        usb_handle = libusb_open_device_with_vid_pid(NULL, vid, pid);
        if (usb_handle == NULL)
            throw error::NoDeviceException();
    }

    libusb_device* GetUsbDevice() override {
        if (usb_handle == NULL)
            return NULL;
        if (usb_device == NULL)
            usb_device = libusb_get_device(usb_handle);
        return usb_device;
    }

    libusb_device_handle* GetUsbHandle() override { return usb_handle; }

    void Update() override { return; }
};

} // namespace ctl
} // namespace uss