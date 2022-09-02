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
#include "data.hpp"
#include "util.hpp"
#include <string.h>    // memset
#include <sys/errno.h> // errno

namespace usbselfserial {
namespace output {

/**
 * PTY output class.
 * Handles a USB serial connection and links it to pty i/o
 */
class PtyOutput : public BaseOutput {
protected:
    pty::PtyOutputInstanceData instance;
    pty::PtyOutputConfig config;

private:
    usbselfserial::driver::BaseDevice* device;

    // usb [->] PtyOutput -> pty
    static void LIBUSB_CALL callback_rx(struct libusb_transfer* transfer) {
        pty::PtyOutputInstanceData* instance =
            (pty::PtyOutputInstanceData*)transfer->user_data;
        instance->active = false;

        // Make sure transfer completed
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            printf("Failed to submit RX transfer! code %i\n", transfer->status);
            libusb_cancel_transfer(instance->transfer_rx);
            return;
        }

        // Write to pty fd
        write(instance->mfd, instance->buffer_rx, transfer->actual_length);

        // Resubmit transfer
        int ret = libusb_submit_transfer(instance->transfer_rx);
        if (ret < 0) {
            printf("Failed to submit RX transfer! code %i (%s)\n", ret,
                   libusb_error_name(ret));
            libusb_cancel_transfer(instance->transfer_rx);
        }

        instance->active = true;
    }

public:
    PtyOutput(void* _config) {
        config = *(pty::PtyOutputConfig*)_config;
        pty::create_pty(instance, config.location);
    }

    void LinkDevice(usbselfserial::driver::BaseDevice* _device) override {
        if (_device == NULL)
            throw "Tried to link null device!";
        device = _device;

        // Allocate transfers
        instance.transfer_rx = libusb_alloc_transfer(0);

        // Set up transfers
        libusb_fill_bulk_transfer(
            instance.transfer_rx, device->GetDeviceData()->usb_handle,
            device->GetDeviceData()->endpoint_in, instance.buffer_rx,
            sizeof(instance.buffer_rx), callback_rx, &instance, 0);

        // Submit transfers
        int ret = libusb_submit_transfer(instance.transfer_rx);
        if (ret < 0) {
            printf("libusb_submit_transfer failure! code %i (%s)\n", ret,
                   libusb_error_name(ret));
            throw "Failed to submit RX transfer.";
        }

        // Init thread
        instance.active = true;
    }

    void UnlinkDevice() override {
        if (instance.transfer_rx != 0x0)
            libusb_free_transfer(instance.transfer_rx);
        instance.transfer_rx = 0x0;
        instance.active = false;
        memset(instance.buffer_rx, 0, sizeof(instance.buffer_rx));
        memset(instance.buffer_tx, 0, sizeof(instance.buffer_tx));
        if (!config.retain_pty) {
            close(instance.mfd);
            pty::create_pty(instance, config.location);
        }
    }

    void Update() override {
        if (!HasLinkedDevice())
            return;

        // Set buffer to fully 0
        memset(instance.buffer_tx, 0, sizeof(instance.buffer_tx));

        // Read from pty fd
        int ret =
            read(instance.mfd, instance.buffer_tx, sizeof(instance.buffer_tx));
        if (ret == -1) {
            if (errno != EAGAIN)
                printf("Error reading from pty fd! code %i\n", errno);
            return;
        }

        if (ret == 0)
            return;

        // Send to USB
        libusb_bulk_transfer(device->GetDeviceData()->usb_handle,
                             device->GetDeviceData()->endpoint_out,
                             instance.buffer_tx, ret, NULL, 2000);
    }

    void HandleFinalizeRequest() override {
        // Cancel transfers
        if (instance.transfer_rx != 0x0)
            libusb_cancel_transfer(instance.transfer_rx);
    }

    bool HasLinkedDevice() override {
        // hacky way
        return (instance.transfer_rx != 0x0);
    }

    bool HasFinished() override { return !(instance.active); }
};

} // namespace output
} // namespace usbselfserial