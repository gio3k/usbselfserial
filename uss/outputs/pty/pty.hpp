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
#include <cstdio>
#include <string>
#include <sys/fcntl.h> // F_*
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
    int mfd, sfd;
    struct libusb_transfer* rx_transfer;
    uint8_t rx_buffer[1024];
    uint8_t tx_buffer[1024];
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

        if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
            printf("Transfer cancelled! Attempting free. code %i\n", transfer->status);
            libusb_free_transfer(transfer);
            return;
        }

        if (transfer->status == LIBUSB_TRANSFER_ERROR || transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
            printf("Transfer fail! Attempting free. code %i\n", transfer->status);
            libusb_free_transfer(transfer);
        }

        // Make sure transfer completed
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            printf("Transfer unknown fail! Attempting cancel. code %i\n", transfer->status);
            libusb_cancel_transfer(instance->rx_transfer);
            return;
        }

        // Write to pty fd
        write(instance->mfd, instance->rx_buffer, transfer->actual_length);

        // Resubmit transfer
        int ret = libusb_submit_transfer(instance->rx_transfer);
        if (ret < 0) {
            printf("Failed to submit RX transfer! code %i (%s)\n", ret,
                   libusb_error_name(ret));
            libusb_cancel_transfer(instance->rx_transfer);
        }
    }

    void CreatePty() {
        int ret;

        // Open pty
        ret = openpty(&instance.mfd, &instance.sfd, NULL, NULL, NULL);
        if (ret != 0)
            throw PtyError();

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

public:
    PtyOutput(BaseDevice* _device, const char* _location,
              bool _retain_pty = false)
        : BaseOutput(_device), retain_pty(_retain_pty), location(_location) {

        CreatePty();
        SetDevice(device);
    }

    void Update() override {
        if (device == NULL)
            return;

        int ret;

        // Set buffer to fully 0
        memset(instance.tx_buffer, 0, sizeof(instance.tx_buffer));

        // Read from pty fd
        ret = read(instance.mfd, instance.tx_buffer,
                   device->GetOutEndpointPacketSize());
        if (ret == -1) {
            if (errno != EAGAIN)
                printf("Error reading from pty fd! code %i\n", errno);
            return;
        }

        if (ret == 0)
            return;

        // Send to USB
        libusb_bulk_transfer(device->GetUsbHandle(), device->GetOutEndpoint(),
                             instance.tx_buffer, ret, NULL, TransferTimeout);
    }

    void SetDevice(BaseDevice* _device) override {
        if (_device == NULL)
            return;
        device = _device;

        int ret;

        if (!retain_pty)
            CreatePty();

        // Allocate transfers
        instance.rx_transfer = libusb_alloc_transfer(0);

        // Set up transfers
        if (device->GetInEndpointPacketSize() > sizeof(instance.rx_buffer))
            throw PtyError();
        if (device->GetOutEndpointPacketSize() > sizeof(instance.tx_buffer))
            throw PtyError();

        libusb_fill_bulk_transfer(instance.rx_transfer, device->GetUsbHandle(),
                                  device->GetInEndpoint(), instance.rx_buffer,
                                  device->GetInEndpointPacketSize(),
                                  ReceiveCallback, &instance, TransferTimeout);

        // Submit transfers
        ret = libusb_submit_transfer(instance.rx_transfer);
        if (ret < 0) {
            printf("libusb_submit_transfer failure! code %i (%s)\n", ret,
                   libusb_error_name(ret));
            throw error::LibUsbErrorException("Failed to submit transfer", ret);
        }
    }

    void RemoveDevice() override {
        device = NULL;
        if (instance.rx_transfer != NULL)
            libusb_cancel_transfer(instance.rx_transfer);
    }
};

} // namespace pty
} // namespace output
} // namespace uss