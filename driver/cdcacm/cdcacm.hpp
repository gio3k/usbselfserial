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
#include "../device.hpp"
#include "vars.hpp"
#include <libusb-1.0/libusb.h>
#include <stdio.h>

#define CONTROL_IN (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
#define CONTROL_OUT (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)
#define CONTROL_TIMEOUT 2000

namespace usbselfserial {
namespace driver {

struct CdcAcmDeviceData {
    GenericDeviceData generic;
    u8_t interface_data;
    u8_t interface_comm;
};

class CdcAcmDevice : public BaseDevice {
protected:
    CdcAcmDeviceData device;
    bool rts = true;
    bool dtr = true;

    /**
     * Send control message to device
     * @param request Request
     * @param value Value
     * @param data Pointer to data / NULL
     * @param length Length of data if necessary
     * @return int Return code of transfer
     */
    int ControlOut(u8_t request, u16_t value, u8_t* data = 0x0,
                   u16_t length = 0) {
        return libusb_control_transfer(device.generic.usb_handle, USB_RT_ACM,
                                       request, value, device.interface_comm,
                                       data, length, 2000);
    }

    void Configure() override {
        u8_t cvt_data_bits[] = {5, 6, 7, 8};
        u8_t cvt_stop_bits[] = {0, 1, 2};
        u8_t cvt_parity[] = {0, 1, 2, 3, 4};

        // Make sure config values aren't too high
        if ((u8_t)data_bits >= sizeof(cvt_data_bits))
            throw "Invalid data bits value";

        if ((u8_t)stop_bits >= sizeof(cvt_stop_bits))
            throw "Invalid stop bits value";

        if ((u8_t)parity >= sizeof(cvt_parity))
            throw "Invalid parity value";

        // Create control packet
        u8_t output[7];
        *((u32_t*)output) = baud_rate;
        output[4] = cvt_stop_bits[stop_bits];
        output[5] = cvt_parity[parity];
        output[6] = cvt_data_bits[data_bits];

        // Send to chip
        ControlOut(SET_LINE_CODING, 0, output, 7);
    }

    void UpdateControlLines() override {
        u8_t output = (rts ? 0x02 : 0) | (dtr ? 0x01 : 0);
        ControlOut(SET_CONTROL_LINE_STATE, output);
    }

public:
    CdcAcmDevice(libusb_device_handle* _handle) : BaseDevice() {
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
                case USB_CLASS_COMM:
                    device.interface_comm =
                        descriptor_interface->bInterfaceNumber;

                    break;
                case USB_CLASS_CDC_DATA:
                    device.interface_data =
                        descriptor_interface->bInterfaceNumber;

                    // For each endpoint.. (with the amount of them found in the
                    // interface descriptor)
                    for (int ie = 0; ie < descriptor_interface->bNumEndpoints;
                         ie++) {
                        descriptor_endpoint =
                            descriptor_interface->endpoint + ie;
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
                }
            }

            // Free configuration descriptor
            libusb_free_config_descriptor(descriptor_config);
        }

        printf(
            "Prepared CDC ACM device. (usb_device %p, usb_handle %p, "
            "interfaces "
            "[data 0x%02x, comm 0x%02x], endpoints [out 0x%02x, in 0x%02x])\n",
            device.generic.usb_device, device.generic.usb_handle,
            device.interface_data, device.interface_comm,
            device.generic.endpoint_out, device.generic.endpoint_in);

        // Detach interfaces
        if (libusb_kernel_driver_active(device.generic.usb_handle,
                                        device.interface_comm)) {
            ret = libusb_detach_kernel_driver(device.generic.usb_handle,
                                              device.interface_comm);
            if (ret != 0) {
                printf("Failed to detach kernel driver from CDC Communication "
                       "interface, code %i (%s)\n",
                       ret, libusb_error_name(ret));
            }
        }

        if (libusb_kernel_driver_active(device.generic.usb_handle,
                                        device.interface_data)) {
            ret = libusb_detach_kernel_driver(device.generic.usb_handle,
                                              device.interface_data);
            if (ret != 0) {
                printf("Failed to detach kernel driver from CDC Data "
                       "interface, code %i (%s)\n",
                       ret, libusb_error_name(ret));
            }
        }

        // Claim interfaces
        ret = libusb_claim_interface(device.generic.usb_handle,
                                     device.interface_comm);
        if (ret != 0) {
            printf(
                "Failed to claim CDC Communication interface, code %i (%s)\n",
                ret, libusb_error_name(ret));
        }

        ret = libusb_claim_interface(device.generic.usb_handle,
                                     device.interface_data);
        if (ret != 0) {
            printf("Failed to claim CDC Data interface, code %i (%s)\n", ret,
                   libusb_error_name(ret));
        }

        // Init device
        Init();
    }

    ~CdcAcmDevice() {
        libusb_release_interface(device.generic.usb_handle,
                                 device.interface_comm);
        libusb_release_interface(device.generic.usb_handle,
                                 device.interface_data);
        libusb_close(device.generic.usb_handle);
    }

    void Init() override {
        UpdateControlLines();
        Configure();
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
        ControlOut(SEND_BREAK, value ? 0xffff : 0);
    }

    GenericDeviceData* GetDeviceData() override {
        return (GenericDeviceData*)&device;
    }
};

} // namespace driver
} // namespace usbselfserial