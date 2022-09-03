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
#include "../../usbvars.hpp"
#include <stdint.h>

namespace uss {
namespace driver {
namespace cdcacm {

struct CdcAcmDeviceData {
    uint8_t comm_interface, data_interface;
    uint8_t in_endpoint, out_endpoint;
    uint16_t in_endpoint_packet_size, out_endpoint_packet_size;
};

namespace ctl {

constexpr const uint8_t SetLineCoding = 0x20;
constexpr const uint8_t GetLineCoding = 0x21;
constexpr const uint8_t SetControlLineState = 0x22;
constexpr const uint8_t SetBreak = 0x23;

} // namespace ctl
} // namespace cdcacm
} // namespace driver
} // namespace uss