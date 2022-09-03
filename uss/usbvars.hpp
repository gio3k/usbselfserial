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
// Mostly defines from linux/include/uapi/linux/usb/ch9.h
#pragma once
#include <stdint.h>

namespace uss {
namespace driver {
namespace usbvars {

constexpr const uint8_t UsbTypeClass = (0x01 << 5);
constexpr const uint8_t UsbRecipInterface = 0x01;
constexpr const uint8_t UsbClassComm = 2;
constexpr const uint8_t UsbClassCdcData = 0x0a;

constexpr const uint8_t UsbDirIn = 0x20;
constexpr const uint8_t UsbDirOut = 0x21;

// Defines from linux/drivers/usb/class/cdc-acm.h
constexpr const uint8_t UsbRtAcm = (UsbTypeClass | UsbRecipInterface);

} // namespace usbvars
} // namespace driver
} // namespace uss