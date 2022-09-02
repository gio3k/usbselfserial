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
#include "../driver/base.hpp"

namespace usbselfserial {
namespace output {

class BaseOutput {
private:
    bool currently_finalizing = false;

protected:
    void EndFinalizeRequest() { currently_finalizing = false; }
    virtual void HandleFinalizeRequest() = 0;

public:
    virtual void Update() = 0;
    virtual bool HasFinished() = 0;

    virtual void LinkDevice(usbselfserial::driver::BaseDevice* device) = 0;
    virtual void UnlinkDevice() = 0;
    virtual bool HasLinkedDevice() = 0;

    /**
     * Tell class to complete / clean up execution
     */
    void TryFinalize() {
        if (currently_finalizing)
            return;
        HandleFinalizeRequest();
        currently_finalizing = true;
    }
};

} // namespace output
} // namespace usbselfserial