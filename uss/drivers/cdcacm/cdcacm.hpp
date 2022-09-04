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
namespace cdcacm {

class CdcAcmDriver : public BaseDriver {
    constexpr static const uint8_t DataBitsConverter[] = {5, 6, 7, 8};
    constexpr static const uint8_t StopBitsConverter[] = {0, 1, 2};
    constexpr static const uint8_t ParityConverter[] = {0, 1, 2, 3, 4};
    constexpr static const uint32_t ControlTransferTimeout = 2000;

    int SendDeviceControlMessage(BaseDevice& device, uint8_t request,
                                 uint16_t value, uint8_t* data = 0x0,
                                 uint16_t length = 0) {
        return libusb_control_transfer(
            device.GetUsbHandle(), usbvars::UsbRtAcm, request, value,
            device.GetDriverSpecificData<CdcAcmDeviceData>().comm_interface,
            data, length, ControlTransferTimeout);
    };

public:
    void HandleDeviceConfigure(BaseDevice& device) override {
        if ((uint32_t)device.data_bits >= sizeof(DataBitsConverter))
            throw error::InvalidDeviceConfigException("data_bits",
                                                      (int)device.data_bits);
        if ((uint32_t)device.stop_bits >= sizeof(StopBitsConverter))
            throw error::InvalidDeviceConfigException("stop_bits",
                                                      (int)device.stop_bits);
        if ((uint32_t)device.parity >= sizeof(ParityConverter))
            throw error::InvalidDeviceConfigException("parity",
                                                      (int)device.parity);

        // Create control message
        uint8_t message[7];
        *reinterpret_cast<uint32_t*>(message) = device.baud_rate;
        message[4] = StopBitsConverter[(int)device.stop_bits];
        message[5] = ParityConverter[(int)device.parity];
        message[6] = DataBitsConverter[(int)device.data_bits];

        // Send to device
        SendDeviceControlMessage(device, ctl::SetLineCoding, 0, message, 7);
    }

    void HandleDeviceUpdateLines(BaseDevice& device) override {
        // Create control message
        uint8_t message = (device.rts ? 0x02 : 0) | (device.dtr ? 0x01 : 0);
        SendDeviceControlMessage(device, ctl::SetControlLineState, message);
    }

    void HandleDeviceInit(BaseDevice& device) override {
        // rts & dtr required for cdcacm
        device.rts = true;
        device.dtr = true;
        HandleDeviceUpdateLines(device);
        HandleDeviceConfigure(device);
    }

    void SetUpDevice(BaseDevice& device) override {
        int ret;
        libusb_device_descriptor device_descriptor;
        libusb_config_descriptor* config_descriptor;
        const libusb_endpoint_descriptor* endpoint_descriptor;
        const libusb_interface_descriptor* interface_descriptor;
        const libusb_interface* interface;
        CdcAcmDeviceData& device_data =
            device.GetDriverSpecificData<CdcAcmDeviceData>();

        // Get device descriptor
        ret = libusb_get_device_descriptor(device.GetUsbDevice(),
                                           &device_descriptor);
        if (ret < 0)
            throw error::DevicePopulateException(
                "Couldn't get device descriptor.");

        // For each configuration.. (with the amount of them found in the device
        // descriptor)
        for (uint8_t ic = 0; ic < device_descriptor.bNumConfigurations; ic++) {
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
                case usbvars::UsbClassComm:
                    device_data.comm_interface =
                        interface_descriptor->bInterfaceNumber;

                    break;
                case usbvars::UsbClassCdcData:
                    device_data.data_interface =
                        interface_descriptor->bInterfaceNumber;

                    // For each endpoint.. (with the amount of them found in the
                    // interface descriptor)
                    for (int ie = 0; ie < interface_descriptor->bNumEndpoints;
                         ie++) {
                        endpoint_descriptor =
                            interface_descriptor->endpoint + ie;
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

        printf("comm:%i, data:%i, in:%i, out:%i\n", device_data.comm_interface,
               device_data.data_interface, device_data.in_endpoint,
               device_data.out_endpoint);

        if (device_data.comm_interface == 0 && device_data.data_interface == 0)
            throw error::DevicePopulateException(
                "Couldn't populate interfaces.");

        if (device_data.in_endpoint == 0 && device_data.out_endpoint == 0)
            throw error::DevicePopulateException(
                "Couldn't populate endpoints.");

        // Detach interfaces
        if (libusb_kernel_driver_active(device.GetUsbHandle(),
                                        device_data.comm_interface)) {
            ret = libusb_detach_kernel_driver(device.GetUsbHandle(),
                                              device_data.comm_interface);
            if (ret != 0) {
                printf("Failed to detach kernel driver from CDC Communication "
                       "interface, code %i (%s)\n",
                       ret, libusb_error_name(ret));
            }
        }

        if (libusb_kernel_driver_active(device.GetUsbHandle(),
                                        device_data.data_interface)) {
            ret = libusb_detach_kernel_driver(device.GetUsbHandle(),
                                              device_data.data_interface);
            if (ret != 0) {
                printf("Failed to detach kernel driver from CDC Data "
                       "interface, code %i (%s)\n",
                       ret, libusb_error_name(ret));
            }
        }

        // Claim interfaces
        ret = libusb_claim_interface(device.GetUsbHandle(),
                                     device_data.comm_interface);
        if (ret != 0) {
            printf(
                "Failed to claim CDC Communication interface, code %i (%s)\n",
                ret, libusb_error_name(ret));
        }

        ret = libusb_claim_interface(device.GetUsbHandle(),
                                     device_data.data_interface);
        if (ret != 0) {
            printf("Failed to claim CDC Data interface, code %i (%s)\n", ret,
                   libusb_error_name(ret));
        }
    }

    uint8_t GetDeviceInEndpoint(BaseDevice& device) override {
        return device.GetDriverSpecificData<CdcAcmDeviceData>().in_endpoint;
    }

    uint8_t GetDeviceOutEndpoint(BaseDevice& device) override {
        return device.GetDriverSpecificData<CdcAcmDeviceData>().out_endpoint;
    }

    uint16_t GetDeviceInEndpointPacketSize(BaseDevice& device) override {
        return device.GetDriverSpecificData<CdcAcmDeviceData>()
            .in_endpoint_packet_size;
    }

    uint16_t GetDeviceOutEndpointPacketSize(BaseDevice& device) override {
        return device.GetDriverSpecificData<CdcAcmDeviceData>()
            .out_endpoint_packet_size;
    }

    void SetDeviceBreak(BaseDevice& device, bool value) override {
        SendDeviceControlMessage(device, ctl::SetBreak, value ? 0xffff : 0);
    }
};

} // namespace cdcacm
} // namespace driver
} // namespace uss