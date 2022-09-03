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
#include <cstring>
#include <libusb-1.0/libusb.h>

namespace uss {

class BaseDevice {
public:
    BaudRate baud_rate = 9600;
    DataBits data_bits = DataBits::DataBits_8;
    Parity parity = Parity::Parity_None;
    StopBits stop_bits = StopBits::StopBits_1;
    bool rts = true, dtr = true;

    void Configure() {
        if (driver == NULL)
            throw error::NoDriverException();
        driver->HandleDeviceConfigure(*this);
    }

    void Reinitialize() { SetDriver(driver); }

    void SetDriver(BaseDriver* new_driver) {
        if (new_driver == NULL)
            throw error::NoDriverException();
        ResetDriverSpecificData();
        driver = new_driver;
        if (!Ready())
            return;
        driver->SetUpDevice(*this);
        driver->HandleDeviceInit(*this);
    }
    BaseDriver* GetDriver() { return driver; }

    template <typename T> T& GetDriverSpecificData() {
        static_assert(sizeof(T) <= sizeof(_driver_specific_data),
                      "Type too big for GetDriverSpecificData");
        return *(T*)_driver_specific_data;
    }

    virtual libusb_device_handle* GetUsbHandle() = 0;
    virtual libusb_device* GetUsbDevice() = 0;

    uint8_t GetInEndpoint() {
        if (driver == NULL)
            throw error::NoDriverException();
        return driver->GetDeviceInEndpoint(*this);
    }
    uint8_t GetOutEndpoint() {
        if (driver == NULL)
            throw error::NoDriverException();
        return driver->GetDeviceOutEndpoint(*this);
    }
    uint16_t GetInEndpointPacketSize() {
        if (driver == NULL)
            throw error::NoDriverException();
        return driver->GetDeviceInEndpointPacketSize(*this);
    }
    uint16_t GetOutEndpointPacketSize() {
        if (driver == NULL)
            throw error::NoDriverException();
        return driver->GetDeviceOutEndpointPacketSize(*this);
    }

    void SetBreak(bool value) {
        if (driver == NULL)
            throw error::NoDriverException();
        return driver->SetDeviceBreak(*this, value);
    }

    bool Ready() { return (GetUsbHandle() != NULL); }

protected:
    BaseDriver* driver = NULL;

    void ResetDriverSpecificData() {
        memset(_driver_specific_data, 0, sizeof(_driver_specific_data));
    }

private:
    char _driver_specific_data[32];
};

} // namespace uss