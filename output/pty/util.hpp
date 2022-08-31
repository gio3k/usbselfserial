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
#include "data.hpp"
#include <cstdlib>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <unistd.h>
#include <util.h>

namespace usbselfserial {
namespace output {
namespace pty {

/**
 * Basic create pty function
 * @param data PtyOutputInstanceData ref
 * @param location Location to symlink pty to
 */
void create_pty(PtyOutputInstanceData& data, const char* location) {
    if (location == 0x0)
        throw "No location specified to PtyOutput!";

    // Get mfd & sfd by opening a pty
    int ret = openpty(&data.mfd, &data.sfd, 0, 0, 0);
    if (ret != 0)
        throw "Failed to open pty.";

    // Configure pty
    fcntl(data.mfd, F_SETFL, fcntl(data.mfd, F_GETFL) | O_NONBLOCK);

    struct termios config;
    ret = tcgetattr(data.mfd, &config);
    config.c_iflag &=
        ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    config.c_oflag &= ~OPOST;
    config.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    config.c_cflag &= ~(CSIZE | PARENB);
    config.c_cflag |= CS8;
    config.c_cc[VMIN] = 0;
    config.c_cc[VTIME] = 0;
    tcsetattr(data.mfd, TCSAFLUSH, &config);

    // Chmod sfd
    ret = fchmod(data.sfd, S_IRWXU | S_IRWXG | S_IRWXO);
    if (ret != 0)
        printf("fchmod failure! code %i\n", ret);

    // Remove past symlink
    ret = unlink(location);
    if (ret != 0)
        printf("unlink failure! code %i\n", ret);

    // Get filename for sfd
    data.sfdname = ptsname(data.mfd);

    // Create new symlink
    ret = symlink(data.sfdname, location);
    if (ret != 0)
        printf("symlink failure! code %i\n", ret);
}

} // namespace pty
} // namespace output
} // namespace usbselfserial