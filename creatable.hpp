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

namespace usbselfserial {

/**
 * Abstract class for classes that use Creator to make their instance
 */
class Creatable {
private:
    bool completing = false;

public:
    virtual void HandleCompletionRequest() = 0;
    virtual bool Completed() = 0;
    virtual void Run() = 0;

    /**
     * Request class to complete / clean up execution
     */
    void TryComplete() {
        if (completing)
            return;
        HandleCompletionRequest();
        completing = true;
    }
};

} // namespace usbselfserial