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
#include "../types.hpp"
#include <libusb-1.0/libusb.h>

namespace usbselfserial {
namespace driver {

enum DataBits { DataBits_5 = 0, DataBits_6, DataBits_7, DataBits_8 };

enum StopBits { StopBits_1 = 0, StopBits_1_5, StopBits_2 };

enum Parity {
    Parity_None = 0,
    Parity_Odd,
    Parity_Even,
    Parity_Mark,
    Parity_Space
};
struct GenericDeviceData {
    libusb_device* usb_device;
    libusb_device_handle* usb_handle;
    u8_t endpoint_out;
    u8_t endpoint_in;
};

} // namespace driver
} // namespace usbselfserial