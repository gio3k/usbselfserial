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
#include "driver.hpp"
#include "error.hpp"
#include "serial.hpp"
#include <libusb-1.0/libusb.h>

namespace uss {

template <typename DeviceT = void> class BaseDevice {
public:
    BaudRate baud_rate = 9600;
    DataBits data_bits = DataBits::DataBits_8;
    Parity parity = Parity::Parity_None;
    StopBits stop_bits = StopBits::StopBits_1;
    bool rts = false, dtr = false;

    virtual libusb_device_handle* GetUsbHandle() = 0;
    virtual libusb_device* GetUsbDevice() = 0;
    virtual uint8_t GetInEndpoint() = 0;
    virtual uint8_t GetOutEndpoint() = 0;

    virtual void Configure() = 0;

    virtual void SetDriver(BaseDriver<DeviceT>* driver) = 0;

protected:
    // Get*Endpoint & Get*Interface need a populated device
    virtual void PopulateUsbInfo(libusb_device* usb_device) = 0;
};

} // namespace uss