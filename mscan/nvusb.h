#ifndef _NVUSB_H_
#define _NVUSB_H_

typedef enum {
    MSDC_VENDOR_STATE_IDEL,
    MSDC_VENDOR_STATE_SCAN,
    MSDC_VENDOR_STATE_AUTOCAL,
    MSDC_VENDOR_STATE_AUTOCAL_MOVE,
    MSDC_VENDOR_STATE_NOT_CAL
} MSDC_VENDOR_STATE_ENUM_t;

typedef enum {
    MSDC_VENDOR_DATA_STATUS_NOT_READY,
    MSDC_VENDOR_DATA_STATUS_READY,
    MSDC_VENDOR_DATA_STATUS_OVERSPEED,
    MSDC_VENDOR_DATA_STATUS_INVALID_pb
} MSDC_VENDOR_DATA_STATUS_ENUM_t;

typedef enum {
    MSDC_VENDOR_COLOR_GRAY,
    MSDC_VENDOR_COLOR_COLOR
} MSDC_VENDOR_COLOR_ENUM_t;

typedef enum {
    MSDC_VENDOR_DPI_L,
    MSDC_VENDOR_DPI_M
} MSDC_VENDOR_DPI_ENUM_t;

typedef struct {
    MSDC_VENDOR_DATA_STATUS_ENUM_t Status;
    MSDC_VENDOR_COLOR_ENUM_t Color;
    MSDC_VENDOR_DPI_ENUM_t Dpi;
    int Width;
    int Height;
    int Addr;
} MSDC_VENDOR_DATAINFO_STRUCT_t;

#endif // _NVUSB_H_
