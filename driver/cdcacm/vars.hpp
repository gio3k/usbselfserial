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

// Defines from USB Serial for Android
// https://github.com/mik3y/usb-serial-for-android/blob/master/usbSerialForAndroid/src/main/java/com/hoho/android/usbserial/driver/CdcAcmSerialDriver.java
#define SET_LINE_CODING 0x20 // USB CDC 1.1 section 6.2
#define GET_LINE_CODING 0x21
#define SET_CONTROL_LINE_STATE 0x22
#define SEND_BREAK 0x23

// Defines from the Linux kernel
// Defines from linux/include/uapi/linux/usb/ch9.h
#define USB_TYPE_CLASS (0x01 << 5)
#define USB_RECIP_INTERFACE 0x01
#define USB_CLASS_COMM 2
#define USB_CLASS_CDC_DATA 0x0a

#define USB_DIR_IN 0x80
#define USB_DIR_OUT 0x0

// Defines from linux/drivers/usb/class/cdc-acm.h
#define USB_RT_ACM (USB_TYPE_CLASS | USB_RECIP_INTERFACE)