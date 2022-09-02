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

// References:
// Defines from the WCH driver
//  * https://github.com/WCHSoftGroup/ch341ser_linux/blob/main/driver/ch341.h
// Defines from the Linux kernel
//  Defines from linux/include/uapi/linux/usb/ch9.h &&
//  Defines from linux/drivers/usb/serial/ch341.c (3.18)
//   * https://github.com/torvalds/linux/blob/b2776bf7149bddd1f4161f14f79520f17fc1d71d/drivers/usb/serial/ch341.c

#define USB_DIR_IN 0x80
#define USB_DIR_OUT 0x0

// Generic defines
#define CH34X_DEFAULT_BAUD 9600

#define CH34X_CTL_OUT (USB_DIR_OUT | 0x40)
#define CH34X_CTL_IN (USB_DIR_IN | 0x40)

#define CH34X_C1 0xa1 // "CMD_C1", seems to be init

#define CH34X_CMD_VERSION 0x5f // "CMD_C3"

#define CH34X_CMD_REG_READ 0x95  // "CMD_R"
#define CH34X_CMD_REG_WRITE 0x9a // "CMD_W"

#define CH34X_MODEM_WRITE 0xa4 // "CMD_C2"
#define CH34X_MODEM_DTR 0x20   // "CH341_CTO_D"
#define CH34X_MODEM_RTS 0x40   // "CH341_CTO_R"

// This is used for a bad way to calculate baudrate; use a better way
#define CH34X_FLAWED_BAUDRATE_FACTOR 1532620800
#define CH34X_FLAWED_BAUDRATE_DIVMAX 3

#define CH34X_LCR_ENPA 0x08                      // "CH341_L_PO", enable parity
#define CH34X_LCR_ENRX 0x80                      // "CH341_L_ER", enable rx
#define CH34X_LCR_ENTX 0x40                      // "CH341_L_ET", enable tx
#define CH34X_LCR_CS8 0x03                       // "CH341_L_D8", cs8
#define CH34X_LCR_CS7 0x02                       // "CH341_L_D7", cs7
#define CH34X_LCR_CS6 0x01                       // "CH341_L_D6", cs6
#define CH34X_LCR_CS5 0x00                       // "CH341_L_D5", cs5
#define CH34X_LCR_SB2 0x04                       // "CH341_L_SB", 2 stop bits
#define CH34X_LCR_PAODD (CH34X_LCR_ENPA)         // odd parity
#define CH34X_LCR_PAEVEN (CH34X_LCR_ENPA | 0x10) // even parity
#define CH34X_LCR_PAMARK (CH34X_LCR_ENPA | 0x20) // mark parity
#define CH34X_LCR_PASPACE (CH34X_LCR_ENPA | 0x20 | 0x10) // space parity