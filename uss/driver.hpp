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

namespace uss {

template <typename DeviceT> class BaseDriver {
public:
    virtual void HandleDeviceInit(DeviceT& device) = 0;
    virtual void HandleDeviceConnect(DeviceT& device) = 0;
    virtual void HandleDeviceDisconnect(DeviceT& device) = 0;
    virtual void HandleDeviceConfigure(DeviceT& device) = 0;
    virtual void HandleDeviceUpdateLines(DeviceT& device) = 0;
};

} // namespace uss