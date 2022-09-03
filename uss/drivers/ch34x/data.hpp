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
namespace ch34x {

struct Ch34xDeviceData {
    uint8_t interface;
    uint8_t in_endpoint, out_endpoint;
    uint16_t in_endpoint_packet_size, out_endpoint_packet_size;
};

constexpr const uint32_t FlawedBaudrateFactor = 1532620800;
constexpr const uint32_t FlawedBaudrateDivMax = 3;

constexpr const uint8_t Ch34xCtlOut = (usbvars::UsbDirOut | 0x40);
constexpr const uint8_t Ch34xCtlIn = (usbvars::UsbDirIn | 0x40);

namespace ctl {

constexpr const uint8_t CmdC1 = 0xa1;       // "CMD_C1", seems to be init
constexpr const uint8_t CmdVersion = 0x5f;  // "CMD_C3"
constexpr const uint8_t CmdRegRead = 0x95;  // "CMD_R"
constexpr const uint8_t CmdRegWrite = 0x9a; // "CMD_W"
constexpr const uint8_t ModemWrite = 0xa4;  // "CMD_C2"
constexpr const uint8_t ModemDtr = 0x20;    // "CH341_CTO_D"
constexpr const uint8_t ModemRts = 0x40;    // "CH341_CTO_R"

constexpr const uint8_t LcrEnPa = 0x08;       // "CH341_L_PO", enable parity
constexpr const uint8_t LcrEnRx = 0x80;       // "CH341_L_ER", enable rx
constexpr const uint8_t LcrEnTx = 0x40;       // "CH341_L_ET", enable tx
constexpr const uint8_t LcrCs8 = 0x03;        // "CH341_L_D8", cs8
constexpr const uint8_t LcrCs7 = 0x02;        // "CH341_L_D7", cs7
constexpr const uint8_t LcrCs6 = 0x01;        // "CH341_L_D6", cs6
constexpr const uint8_t LcrCs5 = 0x00;        // "CH341_L_D5", cs5
constexpr const uint8_t LcrSb2 = 0x04;        // "CH341_L_SB", 2 stop bits
constexpr const uint8_t LcrPaOdd = (LcrEnPa); // odd parity
constexpr const uint8_t LcrPaEven = (LcrEnPa | 0x10);         // even parity
constexpr const uint8_t LcrPaMark = (LcrEnPa | 0x20);         // mark parity
constexpr const uint8_t LcrPaSpace = (LcrEnPa | 0x20 | 0x10); // space parity

} // namespace ctl
} // namespace ch34x
} // namespace driver
} // namespace uss