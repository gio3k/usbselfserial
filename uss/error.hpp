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
#include <exception>
#include <string>

namespace uss {
namespace error {

class InvalidDeviceConfigException : public std::exception {
    std::string msg;

public:
    InvalidDeviceConfigException(const std::string& config_key, int value)
        : msg("Invalid value " + std::to_string(value) + " for config key " +
              config_key) {}

    const char* what() const throw() override { return msg.c_str(); }
};

class NoDriverException : public std::exception {
public:
    const char* what() const throw() override { return "Device has no driver"; }
};

class DevicePopulateException : public std::exception {
    std::string msg;

public:
    DevicePopulateException(const std::string& reason)
        : msg("Failed to populate device USB info: " + reason) {}
    const char* what() const throw() override { return msg.c_str(); }
};

} // namespace error
} // namespace uss