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
#include "../../error.hpp"
#include "../../output.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/fcntl.h> // F_*
#include <sys/poll.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <util.h> // openpty
#else
#include <pty.h>
#endif

namespace uss {
namespace output {
namespace pty {

class PtyError : public std::exception {
public:
    const char* what() const throw() override { return "PTY failure"; }
};

struct PtyOutputInstanceData {
    // pty
    int mfd = 0, sfd = 0;

    // rx_transfer (usb -> pty)
    struct libusb_transfer* rx_transfer = NULL;
    uint8_t rx_buffer[1024] = {0};

    // tx_transfer (pty -> usb)
    struct libusb_transfer* tx_transfer = NULL;
    uint8_t tx_buffer[1024] = {0};
    bool tx_sending = false;
    bool tx_allow = true;

    std::function<void(int)> transfer_end_callback = NULL;
};

class PtyOutput : public BaseOutput {
    constexpr static const uint32_t TransferTimeout = 0;
    PtyOutputInstanceData instance;
    std::string location;
    bool retain_pty;

    // usb [->] PtyOutput -> pty
    static void LIBUSB_CALL ReceiveCallback(struct libusb_transfer* transfer) {
        PtyOutputInstanceData* instance =
            (PtyOutputInstanceData*)transfer->user_data;

        if (transfer->status == LIBUSB_TRANSFER_CANCELLED ||
            transfer->status == LIBUSB_TRANSFER_ERROR ||
            transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
            printf("RX transfer fail. code %i (%s)\n", transfer->status,
                   libusb_error_name(transfer->status));

            if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
                printf("Trying to free cancelled transfer!\n");
                libusb_free_transfer(transfer);
            }

            instance->rx_transfer = NULL; // Just assume transfer is gone
            if (instance->tx_transfer == NULL)
                if (instance->transfer_end_callback != NULL)
                    instance->transfer_end_callback(0);
            return;
        }

        // Make sure transfer completed
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            printf(
                "RX transfer unknown fail. Trying to cancel it. code %i (%s)\n",
                transfer->status, libusb_error_name(transfer->status));
            libusb_cancel_transfer(instance->rx_transfer);
            return;
        }

        // Write to pty fd
        write(instance->mfd, instance->rx_buffer, transfer->actual_length);

        // Resubmit transfer
        int ret = libusb_submit_transfer(instance->rx_transfer);
        if (ret < 0) {
            printf("Failed to submit RX transfer. Cancelling! code %i (%s)\n",
                   ret, libusb_error_name(ret));
            libusb_cancel_transfer(instance->rx_transfer);
        }
    }

    // pty [->] PtyOutput -> usb
    static void LIBUSB_CALL TransmitCallback(struct libusb_transfer* transfer) {
        PtyOutputInstanceData* instance =
            (PtyOutputInstanceData*)transfer->user_data;

        if (transfer->status == LIBUSB_TRANSFER_CANCELLED ||
            transfer->status == LIBUSB_TRANSFER_ERROR ||
            transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
            printf("TX transfer fail. code %i (%s)\n", transfer->status,
                   libusb_error_name(transfer->status));

            if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
                printf("Trying to free cancelled transfer!\n");
                libusb_free_transfer(transfer);
            }

            instance->tx_transfer = NULL; // Just assume transfer is gone
            if (instance->rx_transfer == NULL)
                if (instance->transfer_end_callback != NULL)
                    instance->transfer_end_callback(0);
            return;
        }

        // Make sure transfer completed
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            printf(
                "TX transfer unknown fail. Trying to cancel it. code %i (%s)\n",
                transfer->status, libusb_error_name(transfer->status));
            libusb_cancel_transfer(instance->tx_transfer);
            return;
        }

        // Turn off sending bool
        instance->tx_sending = false;
    }

    /**
     * Create basic pty @ instance.mfd, instance.sfd
     */
    void CreatePty() {
        int ret;

        if (instance.mfd != 0) {
            printf("Can't open pty with already existing pty!\n");
            return;
        }

        // Open pty
        ret = openpty(&instance.mfd, &instance.sfd, NULL, NULL, NULL);
        if (ret != 0)
            throw PtyError();

        printf("new pty@%i\n", instance.mfd);

        // Configure pty
        struct termios config;
        ret = tcgetattr(instance.mfd, &config);
        config.c_iflag &=
            ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        config.c_oflag &= ~OPOST;
        config.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        config.c_cflag &= ~(CSIZE | PARENB);
        config.c_cflag |= CS8;
        config.c_cc[VMIN] = 0;
        config.c_cc[VTIME] = 0;
        tcsetattr(instance.mfd, TCSAFLUSH, &config);
        fcntl(instance.mfd, F_SETFL, fcntl(instance.mfd, F_GETFL) | O_NONBLOCK);

        // Chmod sfd
        ret = fchmod(instance.sfd, S_IRWXU | S_IRWXG | S_IRWXO);
        if (ret != 0)
            printf("fchmod failure! code %i\n", ret);

        // Remove past symlink
        ret = unlink(location.c_str());
        if (ret != 0)
            printf("unlink failure! code %i\n", ret);

        // Create new symlink
        ret = symlink(ptsname(instance.mfd), location.c_str());
        if (ret != 0)
            printf("symlink failure! code %i\n", ret);
    }

    /**
     * Close open pty
     */
    void ClosePty() {
        // Close previous pty
        if (instance.mfd != 0)
            close(instance.mfd);
        else
            printf("No pty open to close!\n");
        instance.mfd = 0;
        instance.sfd = 0;
    }

public:
    PtyOutput(BaseDevice* _device, const char* _location,
              bool _retain_pty = false)
        : BaseOutput(_device), location(_location), retain_pty(_retain_pty) {

        CreatePty();
        SetDevice(device);
    }

    void HandleEvents() override {
        if (device == NULL)
            return;

        if (instance.tx_sending)
            return;

        if (!instance.tx_allow)
            return;

        ssize_t len;
        pollfd pfds[1] = {{instance.mfd, POLLIN, 0}};

        // Poll pty fd
        poll(pfds, 1, -1);
        if (!(pfds->revents & POLLIN))
            return;

        // Read from pty fd into buffer queue
        len = read(instance.mfd, instance.tx_buffer,
                   device->GetOutEndpointPacketSize());

        if (len == -1) {
            if (errno != EAGAIN)
                printf("Error reading from pty fd! code %i\n", errno);
            return;
        }

        if (len == 0)
            return;

        // Make sure device still exists before sending
        if (device == NULL)
            return;

        // Send to USB
        if (!instance.tx_allow)
            return;

        instance.tx_sending = true;
        libusb_fill_bulk_transfer(instance.tx_transfer, device->GetUsbHandle(),
                                  device->GetOutEndpoint(), instance.tx_buffer,
                                  (int)len, TransmitCallback, &instance,
                                  TransferTimeout);

        int ret = libusb_submit_transfer(instance.tx_transfer);
        if (ret < 0) {
            printf("Failed to submit TX transfer. code %i (%s)\n", ret,
                   libusb_error_name(ret));
        }
    }

    void SetDevice(BaseDevice* _device) override {
        if (_device == NULL)
            return;
        device = _device;

        int ret;

        // Attempt to create pty
        CreatePty();

        // Allocate transfers
        instance.rx_transfer = libusb_alloc_transfer(0);
        instance.tx_transfer = libusb_alloc_transfer(0);

        // Set up transfers
        if (device->GetInEndpointPacketSize() > sizeof(instance.rx_buffer))
            throw PtyError();
        if (device->GetOutEndpointPacketSize() > sizeof(instance.tx_buffer))
            throw PtyError();

        libusb_fill_bulk_transfer(instance.rx_transfer, device->GetUsbHandle(),
                                  device->GetInEndpoint(), instance.rx_buffer,
                                  device->GetInEndpointPacketSize(),
                                  ReceiveCallback, &instance, TransferTimeout);

        // Allow tx transfers again
        instance.tx_allow = true;

        // Submit rx transfer
        ret = libusb_submit_transfer(instance.rx_transfer);
        if (ret < 0) {
            printf("libusb_submit_transfer failure! code %i (%s)\n", ret,
                   libusb_error_name(ret));
            throw error::LibUsbErrorException("Failed to submit transfer", ret);
        }
    }

    void RemoveDevice() override {
        device = NULL;
        instance.tx_allow = false;

        if (!retain_pty)
            ClosePty();

        if (instance.rx_transfer != NULL) {
            printf("RemoveDevice() called with active transfer. This is "
                   "dangerous, use EndTransfers first!\n");
            EndTransfers();
        }
        if (instance.tx_transfer != NULL) {
            printf("RemoveDevice() called with active transfer. This is "
                   "dangerous, use EndTransfers first!\n");
            EndTransfers();
        }
    }

    void EndTransfers(std::function<void(int)> callback = NULL) override {
        instance.tx_allow = false;
        instance.tx_sending = false;
        if (callback != NULL)
            SetTransferCompletionCallback(callback);
        if (instance.rx_transfer != NULL) {
            libusb_cancel_transfer(instance.rx_transfer);
        } else {
            printf("RX transfer is already null.\n");
        }
        if (instance.tx_transfer != NULL) {
            libusb_cancel_transfer(instance.tx_transfer);
        } else {
            printf("TX transfer is already null.\n");
        }

        if (instance.rx_transfer == NULL && instance.tx_transfer == NULL)
            if (instance.transfer_end_callback != NULL)
                instance.transfer_end_callback(1);
    }

    void
    SetTransferCompletionCallback(std::function<void(int)> callback) override {
        instance.transfer_end_callback = callback;
    }
};

} // namespace pty
} // namespace output
} // namespace uss