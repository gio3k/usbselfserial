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
#include "../../completable.hpp"
#include "../../driver/device.hpp"
#include "data.hpp"
#include "util.hpp"
#include <cstdio>
#include <sys/errno.h>
#include <thread>

namespace usbselfserial {
namespace output {

/**
 * PTY output class.
 * Handles a USB serial connection and links it to pty i/o
 */
class PtyOutput : public Completable {
private:
    usbselfserial::driver::BaseDevice& device;
    pty::PtyOutputInstanceData data;
    std::thread thread_pty_read;

    static void LIBUSB_CALL
    transfer_rx_callback(struct libusb_transfer* transfer) {
        // usb -> PtyOutput -> pty
        pty::PtyOutputInstanceData* data =
            (pty::PtyOutputInstanceData*)transfer->user_data;
        data->transfer_rx_activity = false;

        // Make sure transfer completed
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            printf("Failed to submit RX transfer! code %i\n", transfer->status);
            libusb_cancel_transfer(data->transfer_rx);
            return;
        }

        // Write to pty fd
        write(data->mfd, data->buffer_rx, transfer->actual_length);

        // Resubmit transfer
        int ret = libusb_submit_transfer(data->transfer_rx);
        if (ret != 0) {
            printf("Failed to submit RX transfer! code %i (%s)\n", ret,
                   libusb_error_name(ret));
            libusb_cancel_transfer(data->transfer_rx);
        }

        data->transfer_rx_activity = true;
    }

public:
    PtyOutput(usbselfserial::driver::BaseDevice& _device, void* _config)
        : device(_device) {
        // Create pty
        pty::create_pty(data, (const char*)_config);

        // Allocate transfers
        data.transfer_rx = libusb_alloc_transfer(0);

        // Set up transfers
        libusb_fill_bulk_transfer(
            data.transfer_rx, device.GetDeviceData()->usb_handle,
            device.GetDeviceData()->endpoint_in, data.buffer_rx,
            sizeof(data.buffer_rx), transfer_rx_callback, &data, 0);

        // Submit transfers
        int ret = libusb_submit_transfer(data.transfer_rx);
        if (ret != 0) {
            printf("libusb_submit_transfer failure! code %i (%s)\n", ret,
                   libusb_error_name(ret));
            throw "Failed to submit RX transfer.";
        }

        // Init thread
        data.transfer_rx_activity = true;
    }

    ~PtyOutput() {
        if (data.transfer_rx != 0x0)
            libusb_free_transfer(data.transfer_rx);
        data.transfer_rx = 0x0;
    }

    void Run() {
        // Set buffer to fully 0
        memset(data.buffer_tx, 0, sizeof(data.buffer_tx));

        // Read from pty fd
        int ret = read(data.mfd, data.buffer_tx, sizeof(data.buffer_tx));
        if (ret == -1) {
            if (errno != EAGAIN)
                printf("Error reading from pty fd! code %i\n", errno);
            return;
        }

        // Send to USB
        libusb_bulk_transfer(device.GetDeviceData()->usb_handle,
                             device.GetDeviceData()->endpoint_out,
                             data.buffer_tx, sizeof(data.buffer_tx), NULL,
                             2000);
    }

    void HandleCompletionRequest() override {
        // Cancel transfers
        if (data.transfer_rx != 0x0)
            libusb_cancel_transfer(data.transfer_rx);
    }

    bool Completed() override { return !(data.transfer_rx_activity); }
};

} // namespace output
} // namespace usbselfserial