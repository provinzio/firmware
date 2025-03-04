#ifndef __USB_DESCRIPTOR_GENERIC_HID_REPORT_H__
#define __USB_DESCRIPTOR_GENERIC_HID_REPORT_H__

// Includes:

    #include "usb_api.h"
    #include "usb_interfaces/usb_interface_generic_hid.h"

// Macros:

    #define USB_GENERIC_HID_REPORT_DESCRIPTOR_LENGTH (sizeof(UsbGenericHidReportDescriptor))

    #define USB_GENERIC_HID_REPORT_DESCRIPTOR_VENDOR_USAGE_PAGE_INDEX 0x00
    #define USB_GENERIC_HID_REPORT_DESCRIPTOR_VENDOR_USAGE_COLLECTION 0x01
    #define USB_GENERIC_HID_REPORT_DESCRIPTOR_VENDOR_USAGE_DATA_IN    0x02
    #define USB_GENERIC_HID_REPORT_DESCRIPTOR_VENDOR_USAGE_DATA_OUT   0x03

// Variables:

    static USB_DESC_STORAGE_TYPE UsbGenericHidReportDescriptor[] = {
        HID_RI_USAGE_PAGE(16, (0xFF00 | USB_GENERIC_HID_REPORT_DESCRIPTOR_VENDOR_USAGE_PAGE_INDEX)),
        HID_RI_USAGE(8, USB_GENERIC_HID_REPORT_DESCRIPTOR_VENDOR_USAGE_COLLECTION),
        HID_RI_COLLECTION(8, HID_RI_COLLECTION_APPLICATION),

            // Input flowing from device to host
            HID_RI_USAGE(8, USB_GENERIC_HID_REPORT_DESCRIPTOR_VENDOR_USAGE_DATA_IN),
            HID_RI_LOGICAL_MINIMUM(8, 0x00),
            HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
            HID_RI_REPORT_SIZE(8, 8),
            HID_RI_REPORT_COUNT(8, USB_GENERIC_HID_INTERRUPT_IN_PACKET_SIZE),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

            // Output flowing from host to device
            HID_RI_USAGE(8, USB_GENERIC_HID_REPORT_DESCRIPTOR_VENDOR_USAGE_DATA_OUT),
            HID_RI_LOGICAL_MINIMUM(8, 0x00),
            HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
            HID_RI_REPORT_SIZE(8, 8),
            HID_RI_REPORT_COUNT(8, USB_GENERIC_HID_INTERRUPT_OUT_PACKET_SIZE),
            HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

        HID_RI_END_COLLECTION(0)
    };

#endif
