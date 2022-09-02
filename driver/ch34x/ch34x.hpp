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
#include "../base.hpp"
#include "vars.hpp"
#include <cstddef>
#include <cstdio>
#include <libusb-1.0/libusb.h>
#include <stdio.h>

#define CONTROL_IN (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
#define CONTROL_OUT (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)
#define CONTROL_TIMEOUT 2000

namespace usbselfserial {
namespace driver {

struct Ch34xDeviceData {
    GenericDeviceData generic;
    u8_t interface;
};

class Ch34xDevice : public BaseDevice {
protected:
    Ch34xDeviceData device;
    bool rts = true;
    bool dtr = true;
    u8_t version = 0x0;

    /**
     * Send control message to device
     * @param request Request
     * @param value Value
     * @param index Index
     * @return int Return code of transfer
     */
    int ControlOut(u8_t request, u16_t value, u16_t index) {
        return libusb_control_transfer(device.generic.usb_handle, CH34X_CTL_OUT,
                                       request, value, index, NULL, 0, 0);
    }

    /**
     * Send control message to device and receive input
     * @param request Request
     * @param value Value
     * @param index Index
     * @param data Pointer to output data to
     * @param length Amount of data to request
     * @return int Return code of transfer
     */
    int ControlIn(u8_t request, u16_t value, u16_t index, u8_t* data,
                  u16_t length) {
        return libusb_control_transfer(device.generic.usb_handle, CH34X_CTL_IN,
                                       request, value, index, data, length, 0);
    }

    /**
     * Tell chip about current baud rate
     */
    void UpdateBaudRate(u32_t new_baud_rate) {
        // This is a flawed way to do it
        u32_t factor;
        u16_t divisor;
        int ret;

        factor = CH34X_FLAWED_BAUDRATE_FACTOR / new_baud_rate;
        divisor = CH34X_FLAWED_BAUDRATE_DIVMAX;

        while ((factor > 0xfff0) && divisor) {
            factor >>= 3;
            divisor--;
        }

        if (factor > 0xfff0)
            throw "Invalid baudrate";

        factor = 0x10000 - factor;

        divisor |= 0x0080; // or ch341a waits until buffer full

        ret = ControlOut(CH34X_CMD_REG_WRITE, 0x1312,
                         (factor & 0xff00) | divisor);
        if (ret < 0)
            throw "(1) Failed to set baudrate";

        ret = ControlOut(CH34X_CMD_REG_WRITE, 0x0f2c, factor & 0xff);
        if (ret < 0)
            throw "(2) Failed to set baudrate";
    }

    void Configure() override {
        UpdateBaudRate(baud_rate);
        u16_t lcr = CH34X_LCR_ENRX | CH34X_LCR_ENTX;
        int ret;

        u8_t cvt_data_bits[] = {CH34X_LCR_CS5, CH34X_LCR_CS6, CH34X_LCR_CS7,
                                CH34X_LCR_CS8};
        u8_t cvt_stop_bits[] = {0, 0, CH34X_LCR_SB2};
        u8_t cvt_parity[] = {0, CH34X_LCR_PAODD, CH34X_LCR_PAEVEN,
                             CH34X_LCR_PAMARK, CH34X_LCR_PASPACE};

        if (stop_bits == StopBits_1_5)
            throw "Unsupported amount of stop bits";

        // Make sure config values aren't too high
        if ((u8_t)data_bits >= sizeof(cvt_data_bits))
            throw "Invalid data bits value";

        if ((u8_t)stop_bits >= sizeof(cvt_stop_bits))
            throw "Invalid stop bits value";

        if ((u8_t)parity >= sizeof(cvt_parity))
            throw "Invalid parity value";

        lcr |= cvt_data_bits[data_bits];
        lcr |= cvt_stop_bits[stop_bits];
        lcr |= cvt_parity[parity];

        ret = ControlOut(CH34X_CMD_REG_WRITE, 0x2518, lcr);
        if (ret < 0)
            throw "Failed to set ch34x chip LCR";
    }

    void UpdateControlLines() override {
        u16_t msg = 0;
        int ret;

        if (dtr)
            msg |= CH34X_MODEM_DTR;
        if (rts)
            msg |= CH34X_MODEM_RTS;

        if (version < 20) {
            // uchcom_set_dtrrts_10
            // https://github.com/openbsd/src/blob/08933a0defbec6cd08faa2ea5d07912ace16b3ae/sys/dev/usb/uchcom.c#L510
            // ret = ControlIn(CH34X_CMD_REG_READ,
            printf("DTR/RTS for this chip version not implemented\n");
        } else {
            ret = ControlOut(CH34X_MODEM_WRITE, msg, 0);
            if (ret != 0)
                throw "Failed to set ch34x DTR / RTS";
        }
    }

public:
    Ch34xDevice(libusb_device_handle* _handle) : BaseDevice() {
        device.generic.usb_handle = _handle;
        device.generic.usb_device = libusb_get_device(_handle);

        libusb_device_descriptor descriptor_device;
        libusb_config_descriptor* descriptor_config;
        const libusb_interface* interface;
        const libusb_interface_descriptor* descriptor_interface;
        const libusb_endpoint_descriptor* descriptor_endpoint;

        // Get device descriptor
        int ret = libusb_get_device_descriptor(device.generic.usb_device,
                                               &descriptor_device);
        if (ret != 0) {
            throw "Failed to get device descriptor";
        }

        // For each configuration.. (with the amount of them found in the device
        // descriptor)
        for (int ic = 0; ic < descriptor_device.bNumConfigurations; ic++) {
            // Get the configuration descriptor
            libusb_get_config_descriptor(device.generic.usb_device, ic,
                                         &descriptor_config);

            // For each interface.. (with the amount of them found in the
            // configuration descriptor)
            for (int ii = 0; ii < descriptor_config->bNumInterfaces; ii++) {
                interface = descriptor_config->interface + ii;

                if (!interface->altsetting)
                    continue;

                descriptor_interface = interface->altsetting;

                // Set device interface numbers
                switch (descriptor_interface->bInterfaceClass) {
                case 0xff:
                    device.interface = descriptor_interface->bInterfaceNumber;

                    // For each endpoint.. (with the amount of them found in the
                    // interface descriptor)
                    for (int ie = 0; ie < descriptor_interface->bNumEndpoints;
                         ie++) {
                        descriptor_endpoint =
                            descriptor_interface->endpoint + ie;

                        if (descriptor_endpoint->bmAttributes == 3) {
                            printf(
                                "Skipping (hopefully) interrupt endpoint %i\n",
                                ie);
                            continue;
                        }

                        // if (descriptor_endpoint->bmAttributes &
                        if (USB_DIR_IN ==
                            (descriptor_endpoint->bEndpointAddress &
                             USB_DIR_IN)) {
                            // Set IN endpoint
                            device.generic.endpoint_in =
                                descriptor_endpoint->bEndpointAddress;
                        } else {
                            // Set OUT endpoint
                            device.generic.endpoint_out =
                                descriptor_endpoint->bEndpointAddress;
                        }
                    }
                    break;
                default:
                    throw "Found unknown interface for ch34x device!";
                }
            }

            // Free configuration descriptor
            libusb_free_config_descriptor(descriptor_config);
        }

        printf("Prepared ch34x device. (usb_device %p, usb_handle %p, "
               "interfaces [0x%02x], endpoints [out 0x%02x, in 0x%02x])\n",
               device.generic.usb_device, device.generic.usb_handle,
               device.interface, device.generic.endpoint_out,
               device.generic.endpoint_in);

        // Detach interfaces
        if (libusb_kernel_driver_active(device.generic.usb_handle,
                                        device.interface)) {
            ret = libusb_detach_kernel_driver(device.generic.usb_handle,
                                              device.interface);
            if (ret != 0) {
                printf("Failed to detach kernel driver from ch34x interface, "
                       "code %i (%s)\n",
                       ret, libusb_error_name(ret));
            }
        }

        // Claim interfaces
        ret =
            libusb_claim_interface(device.generic.usb_handle, device.interface);
        if (ret != 0) {
            printf("Failed to claim ch34x interface, code %i (%s)\n", ret,
                   libusb_error_name(ret));
        }

        // Init device
        Init();
    }

    void Init() override {
        u8_t buffer[8];
        int ret;

        // Get chip version
        ret = ControlIn(CH34X_CMD_VERSION, 0, 0, buffer, 8);
        if (ret < 0)
            throw "Failed to get ch34x version";
        version = buffer[0];

        // Clear / init chip
        ret = ControlOut(CH34X_C1, 0, 0);
        if (ret < 0) {
            printf("usb fail code %i (%s)\n", ret, libusb_error_name(ret));
            throw "Failed to clear ch34x chip";
        }

        // Set baudrate after chip clear
        UpdateBaudRate(CH34X_DEFAULT_BAUD);

        // Get LCR
        ret = ControlIn(CH34X_CMD_REG_READ, 0x2518, 0, buffer, 8);
        if (ret < 0)
            throw "Failed to get ch34x chip LCR";

        // Set LCR
        ret = ControlOut(CH34X_CMD_REG_WRITE, 0x2518,
                         CH34X_LCR_ENRX | CH34X_LCR_ENTX | CH34X_LCR_CS8);
        if (ret < 0) {
            printf("usb fail code %i (%s)\n", ret, libusb_error_name(ret));
            throw "Failed to set ch34x chip LCR";
        }

        // Attempt to get status
        ret = ControlIn(CH34X_CMD_REG_READ, 0x0706, 0, buffer, 8);
        printf("-> status buffer 0:%02x 1:%02x 2:%02x 3:%02x 4:%02x 5:%02x "
               "6:%02x 7:%02x\n",
               buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5],
               buffer[6], buffer[7]);
        if (ret < 0) {
            printf("usb fail code %i (%s)\n", ret, libusb_error_name(ret));
            throw "Failed to get ch34x chip status";
        }

        // Reset chip
        ret = ControlOut(CH34X_C1, 0x501f, 0xd90a);
        if (ret < 0) {
            printf("usb fail code %i (%s)\n", ret, libusb_error_name(ret));
            throw "Failed to clear ch34x chip";
        }

        // Set baudrate again
        UpdateBaudRate(CH34X_DEFAULT_BAUD);

        // Update control lines / do handshake
        UpdateControlLines();

        // Notify
        printf("Init completed.\n");
    }

    ~Ch34xDevice() { libusb_close(device.generic.usb_handle); }

    GenericDeviceData* GetDeviceData() override {
        return (GenericDeviceData*)&device;
    }

    void SetDTR(bool value) override {
        dtr = value;
        UpdateControlLines();
    }

    void SetRTS(bool value) override {
        rts = value;
        UpdateControlLines();
    }

    void SetBreak(bool value) override {
        u8_t buffer[2];
        int ret;

        // Read register
        // CH341_REG_BREAK1, CH341_REG_BREAK2
        ret = ControlIn(CH34X_CMD_REG_READ, 0x1805, 0, buffer, 2);
        if (ret != 0) {
            printf("Failed to read ch34x register @ 0x1805! code %i (%s)\n",
                   ret, libusb_error_name(ret));
            return;
        }

        if (value) {
            buffer[0] &= ~0x01; // CH341_NBREAK_BITS_REG1
            buffer[1] &= ~0x40; // CH341_NBREAK_BITS_REG2
        } else {
            buffer[0] |= 0x01;
            buffer[1] |= 0x40;
        }

        // Write register
        ControlOut(CH34X_CMD_REG_WRITE, 0x1805, *(u16_t*)buffer);
    }
};

} // namespace driver
} // namespace usbselfserial