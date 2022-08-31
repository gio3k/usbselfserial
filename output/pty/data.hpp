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
#include "../../types.hpp"

namespace usbselfserial {
namespace output {
namespace pty {

/**
 * Data for a single PtyOutput instance
 * This is so these values can be passed to libusb callbacks as a single
 * user_data pointer.
 */
struct PtyOutputInstanceData {
    int mfd, sfd;
    const char* sfdname;

    /**
     * transfer_*
     * libusb transfers
     */
    struct libusb_transfer* transfer_rx;

    /**
     * *_activity
     * Used to know whether or not the instance has finished or not after
     * calling TryComplete() (look at bool Completed())
     */
    bool transfer_rx_activity = false;

    // RX transfer buffer
    u8_t buffer_rx[1024];
    u8_t buffer_tx[32];
};

} // namespace pty
} // namespace output
} // namespace usbselfserial