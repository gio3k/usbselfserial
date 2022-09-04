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
#include <stdint.h>

namespace uss {

class BaseDevice;
class BaseDriver {
public:
    virtual ~BaseDriver() {}

    virtual void HandleDeviceInit(BaseDevice& device) = 0;
    virtual void HandleDeviceConfigure(BaseDevice& device) = 0;
    virtual void HandleDeviceUpdateLines(BaseDevice& device) = 0;
    virtual void SetDeviceBreak(BaseDevice& device, bool value) = 0;
    virtual void SetUpDevice(BaseDevice& device) = 0;

    virtual uint8_t GetDeviceInEndpoint(BaseDevice& device) = 0;
    virtual uint8_t GetDeviceOutEndpoint(BaseDevice& device) = 0;
    virtual uint16_t GetDeviceInEndpointPacketSize(BaseDevice& device) = 0;
    virtual uint16_t GetDeviceOutEndpointPacketSize(BaseDevice& device) = 0;
};

} // namespace uss