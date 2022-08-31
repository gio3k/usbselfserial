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
#include "../common.hpp"
#include <libusb-1.0/libusb.h>

namespace usbselfserial {
namespace driver {

struct CdcAcmDevice {
    libusb_device* usb_device;
    libusb_device_handle* usb_handle;
    u8_t interface_data;
    u8_t interface_comm;
    u8_t endpoint_out;
    u8_t endpoint_in;
};

} // namespace driver
} // namespace usbselfserial