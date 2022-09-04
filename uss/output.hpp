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
#include "device.hpp"
#include <functional>

namespace uss {

class BaseOutput {
public:
    BaseOutput(BaseDevice* _device) : device(_device) {}

    virtual void HandleEvents() = 0;
    virtual void SetDevice(BaseDevice* _device) = 0;
    virtual void RemoveDevice() = 0;
    virtual void EndTransfers(std::function<void(int)> callback = NULL) = 0;
    virtual void
    SetTransferCompletionCallback(std::function<void(int)> callback) = 0;

protected:
    BaseDevice* device;
};

} // namespace uss