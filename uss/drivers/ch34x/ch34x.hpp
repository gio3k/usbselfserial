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
#include "../../device.hpp"
#include "../../driver.hpp"
#include "../../error.hpp"
#include "data.hpp"
#include <cstdio>

namespace uss {
namespace driver {
namespace ch34x {

class Ch34xDriver : public BaseDriver {
    constexpr static const uint8_t DataBitsConverter[] = {
        ctl::LcrCs5, ctl::LcrCs6, ctl::LcrCs7, ctl::LcrCs8};
    constexpr static const uint8_t StopBitsConverter[] = {0, 0, ctl::LcrSb2};
    constexpr static const uint8_t ParityConverter[] = {
        0, ctl::LcrPaOdd, ctl::LcrPaEven, ctl::LcrPaMark, ctl::LcrPaSpace};
    constexpr static const uint32_t ControlTransferTimeout = 2000;

    uint8_t version = 0x0;

    int SendDeviceControlOut(BaseDevice& device, uint8_t request,
                             uint16_t value, uint16_t index) {
        return libusb_control_transfer(device.GetUsbHandle(), Ch34xCtlOut,
                                       request, value, index, NULL, 0,
                                       ControlTransferTimeout);
    };

    int SendDeviceControlIn(BaseDevice& device, uint8_t request, uint16_t value,
                            uint16_t index, uint8_t* data, uint16_t length) {
        return libusb_control_transfer(device.GetUsbHandle(), Ch34xCtlIn,
                                       request, value, index, data, length,
                                       ControlTransferTimeout);
    };

    void UpdateBaudRate(BaseDevice& device, uint32_t new_baud_rate) {
        // This is a flawed way to do it
        uint32_t factor;
        uint16_t divisor;
        int ret;

        factor = FlawedBaudrateFactor / new_baud_rate;
        divisor = FlawedBaudrateDivMax;

        while ((factor > 0xfff0) && divisor) {
            factor >>= 3;
            divisor--;
        }

        if (factor > 0xfff0)
            throw error::InvalidDeviceConfigException("baud_rate",
                                                      new_baud_rate);

        factor = 0x10000 - factor;

        divisor |= 0x0080; // or ch341a waits until buffer full

        ret = SendDeviceControlOut(device, ctl::CmdRegWrite, 0x1312,
                                   (factor & 0xff00) | divisor);
        if (ret < 0)
            throw error::InvalidDeviceConfigException("(1)baud_rate",
                                                      new_baud_rate);

        ret = SendDeviceControlOut(device, ctl::CmdRegWrite, 0x0f2c,
                                   factor & 0xff);
        if (ret < 0)
            throw error::InvalidDeviceConfigException("(2)baud_rate",
                                                      new_baud_rate);
    }

public:
    void HandleDeviceConfigure(BaseDevice& device) override {
        int ret;
        uint16_t lcr = ctl::LcrEnRx | ctl::LcrEnTx;

        UpdateBaudRate(device, device.baud_rate);

        if (device.stop_bits == StopBits::StopBits_1_5)
            throw error::InvalidDeviceConfigException("(1)stop_bits",
                                                      (int)device.stop_bits);

        if ((int)device.data_bits >= sizeof(DataBitsConverter))
            throw error::InvalidDeviceConfigException("data_bits",
                                                      (int)device.data_bits);
        if ((int)device.stop_bits >= sizeof(StopBitsConverter))
            throw error::InvalidDeviceConfigException("(2)stop_bits",
                                                      (int)device.stop_bits);
        if ((int)device.parity >= sizeof(ParityConverter))
            throw error::InvalidDeviceConfigException("parity",
                                                      (int)device.parity);

        // Create control message
        lcr |= DataBitsConverter[(int)device.data_bits];
        lcr |= StopBitsConverter[(int)device.stop_bits];
        lcr |= ParityConverter[(int)device.parity];

        ret = SendDeviceControlOut(device, ctl::CmdRegWrite, 0x2518, lcr);
        if (ret < 0)
            throw error::DevicePrepException("Failed to set ch34x chip LCR");
    }

    void HandleDeviceUpdateLines(BaseDevice& device) override {
        // Create control message
        uint8_t message = 0;

        if (device.dtr)
            message |= ctl::ModemDtr;
        if (device.rts)
            message |= ctl::ModemRts;

        if (version < 20) {
            // uchcom_set_dtrrts_10
            // https://github.com/openbsd/src/blob/08933a0defbec6cd08faa2ea5d07912ace16b3ae/sys/dev/usb/uchcom.c#L510
            // ret = ControlIn(CH34X_CMD_REG_READ,
            printf("DTR/RTS for this chip version not implemented\n");
        } else {
            int ret = SendDeviceControlOut(device, ctl::ModemWrite, message, 0);
            if (ret < 0)
                throw error::DevicePrepException(
                    "Failed to set ch34x DTR / RTS");
        }
    }

    void HandleDeviceInit(BaseDevice& device) override {
        int ret;
        uint8_t buffer[8];

        // Get chip version
        ret = SendDeviceControlIn(device, ctl::CmdVersion, 0, 0, buffer, 8);
        if (ret < 0)
            throw error::DevicePrepException("Failed to get ch34x version");
        version = buffer[0];

        // Clear / init chip
        ret = SendDeviceControlOut(device, ctl::CmdC1, 0, 0);
        if (ret < 0) {
            printf("usb fail code %i (%s)\n", ret, libusb_error_name(ret));
            throw error::DevicePrepException("Failed to clear ch34x chip");
        }

        // Set baudrate after chip clear
        UpdateBaudRate(device, device.baud_rate);

        // Get LCR
        ret =
            SendDeviceControlIn(device, ctl::CmdRegRead, 0x2518, 0, buffer, 8);
        if (ret < 0)
            throw error::DevicePrepException("Failed to get ch34x chip LCR");

        // Set LCR
        ret = SendDeviceControlOut(device, ctl::CmdRegWrite, 0x2518,
                                   ctl::LcrEnRx | ctl::LcrEnTx | ctl::LcrCs8);
        if (ret < 0) {
            printf("usb fail code %i (%s)\n", ret, libusb_error_name(ret));
            throw "Failed to set ch34x chip LCR";
        }

        // Attempt to get status
        ret =
            SendDeviceControlIn(device, ctl::CmdRegRead, 0x0706, 0, buffer, 8);
        printf("-> status buffer 0:%02x 1:%02x 2:%02x 3:%02x 4:%02x 5:%02x "
               "6:%02x 7:%02x\n",
               buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5],
               buffer[6], buffer[7]);
        if (ret < 0) {
            printf("usb fail code %i (%s)\n", ret, libusb_error_name(ret));
            throw error::DevicePrepException("Failed to get ch34x chip status");
        }

        // Reset chip
        ret = SendDeviceControlOut(device, ctl::CmdC1, 0x501f, 0xd90a);
        if (ret < 0) {
            printf("usb fail code %i (%s)\n", ret, libusb_error_name(ret));
            throw error::DevicePrepException("Failed to clear ch34x chip");
        }

        // Set baudrate again
        UpdateBaudRate(device, device.baud_rate);

        // Update control lines / do handshake
        HandleDeviceUpdateLines(device);

        // Notify
        printf("Init completed.\n");
    }

    void SetUpDevice(BaseDevice& device) override {
        int ret;
        libusb_device_descriptor device_descriptor;
        libusb_config_descriptor* config_descriptor;
        const libusb_endpoint_descriptor* endpoint_descriptor;
        const libusb_interface_descriptor* interface_descriptor;
        const libusb_interface* interface;
        Ch34xDeviceData& device_data =
            device.GetDriverSpecificData<Ch34xDeviceData>();

        // Get device descriptor
        ret = libusb_get_device_descriptor(device.GetUsbDevice(),
                                           &device_descriptor);
        if (ret < 0)
            throw error::DevicePopulateException(
                "Couldn't get device descriptor.");

        // For each configuration.. (with the amount of them found in the device
        // descriptor)
        for (int ic = 0; ic < device_descriptor.bNumConfigurations; ic++) {
            // Get the configuration descriptor
            libusb_get_config_descriptor(device.GetUsbDevice(), ic,
                                         &config_descriptor);

            // For each interface.. (with the amount of them found in the
            // configuration descriptor)
            for (int ii = 0; ii < config_descriptor->bNumInterfaces; ii++) {
                interface = config_descriptor->interface + ii;

                if (!interface->altsetting)
                    continue;

                interface_descriptor = interface->altsetting;

                // Set device interface numbers
                switch (interface_descriptor->bInterfaceClass) {
                case 0xff:
                    device_data.interface =
                        interface_descriptor->bInterfaceNumber;

                    // For each endpoint.. (with the amount of them found in the
                    // interface descriptor)
                    for (int ie = 0; ie < interface_descriptor->bNumEndpoints;
                         ie++) {
                        endpoint_descriptor =
                            interface_descriptor->endpoint + ie;

                        if (endpoint_descriptor->bmAttributes == 3) {
                            printf(
                                "Skipping (hopefully) interrupt endpoint %i\n",
                                ie);
                            continue;
                        }

                        if (driver::usbvars::UsbDirIn ==
                            (endpoint_descriptor->bEndpointAddress &
                             driver::usbvars::UsbDirIn)) {
                            // Set IN endpoint
                            device_data.in_endpoint =
                                endpoint_descriptor->bEndpointAddress;
                            device_data.in_endpoint_packet_size =
                                endpoint_descriptor->wMaxPacketSize;
                        } else {
                            // Set OUT endpoint
                            device_data.out_endpoint =
                                endpoint_descriptor->bEndpointAddress;
                            device_data.out_endpoint_packet_size =
                                endpoint_descriptor->wMaxPacketSize;
                        }
                    }

                    break;
                }
            }

            // Free configuration descriptor
            libusb_free_config_descriptor(config_descriptor);
        }

        printf("int:%i, in:%i, out:%i\n", device_data.interface,
               device_data.in_endpoint, device_data.out_endpoint);

        if (device_data.in_endpoint == 0 && device_data.out_endpoint == 0)
            throw error::DevicePopulateException(
                "Couldn't populate endpoints.");

        // Detach interfaces
        if (libusb_kernel_driver_active(device.GetUsbHandle(),
                                        device_data.interface)) {
            ret = libusb_detach_kernel_driver(device.GetUsbHandle(),
                                              device_data.interface);
            if (ret != 0) {
                printf("Failed to detach kernel driver from interface, code %i "
                       "(%s)\n",
                       ret, libusb_error_name(ret));
            }
        }

        if (libusb_kernel_driver_active(device.GetUsbHandle(),
                                        device_data.interface)) {
            ret = libusb_detach_kernel_driver(device.GetUsbHandle(),
                                              device_data.interface);
            if (ret != 0) {
                printf("Failed to detach kernel driver from interface, code %i "
                       "(%s)\n",
                       ret, libusb_error_name(ret));
            }
        }

        // Claim interfaces
        ret = libusb_claim_interface(device.GetUsbHandle(),
                                     device_data.interface);
        if (ret != 0) {
            printf("Failed to claim interface, code %i (%s)\n", ret,
                   libusb_error_name(ret));
        }
    }

    uint8_t GetDeviceInEndpoint(BaseDevice& device) override {
        return device.GetDriverSpecificData<Ch34xDeviceData>().in_endpoint;
    }

    uint8_t GetDeviceOutEndpoint(BaseDevice& device) override {
        return device.GetDriverSpecificData<Ch34xDeviceData>().out_endpoint;
    }

    uint16_t GetDeviceInEndpointPacketSize(BaseDevice& device) override {
        return device.GetDriverSpecificData<Ch34xDeviceData>()
            .in_endpoint_packet_size;
    }

    uint16_t GetDeviceOutEndpointPacketSize(BaseDevice& device) override {
        return device.GetDriverSpecificData<Ch34xDeviceData>()
            .out_endpoint_packet_size;
    }

    void SetDeviceBreak(BaseDevice& device, bool value) override {
        int ret;
        uint8_t buffer[2];

        // Read register
        // CH341_REG_BREAK1, CH341_REG_BREAK2
        ret =
            SendDeviceControlIn(device, ctl::CmdRegRead, 0x1805, 0, buffer, 2);
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
        SendDeviceControlOut(device, ctl::CmdRegWrite, 0x1805,
                             *(uint16_t*)buffer);
    }
};

} // namespace ch34x
} // namespace driver
} // namespace uss