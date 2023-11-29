//
// Created by TedHuang on 2023/10/25.
//

#ifndef LVICASCADINGVIDEOPROJECT_LVICAMERA_H
#define LVICASCADINGVIDEOPROJECT_LVICAMERA_H

typedef enum {
    LVI_HIDC_INVALID,
    LVI_HIDC_SET,
    LVI_HIDC_GET,
    LVI_HIDC_NOTIFY,
    LVI_HIDC_MAX = 0xff,
} lviHIDCCmd_t;

typedef union __attribute__ ((packed)) {
    uint8_t byte[256 + 6];
    struct {
        uint8_t report_id: 8;
        lviHIDCCmd_t Cmd: 8;
        uint16_t report_len: 16;
        uint8_t pktCnt: 8;
        uint8_t pktNum: 8;
        uint8_t dat[256]; // <---256 bytes
    };
} lviCascadeRptPkt_t;

typedef struct NPU_HeadInfo {
    uint16_t usXCoordnate;
    uint16_t usYCoordnate;
    uint16_t usWidth;
    uint16_t usHeight;
    uint16_t usID;
} m_NPU_HEAD;

typedef struct _vdo_win_t {
    int32_t x, y;
    int32_t width, height;
} vdo_win_t;

typedef struct NPU_COORDINATE {
    int16_t sConfidence;
    uint16_t usXCoordnate;
    uint16_t usYCoordnate;
    uint16_t usWidth;
    uint16_t usHeight;
    m_NPU_HEAD xNpuHeadInfo;
} m_NPU_COORDINATE;

typedef struct NPU_CASCADE {
    uint16_t cascade_FrameDelay;
    uint16_t cascade_PersonNum;
} m_NPU_CASCADE;

typedef struct _xNPUDetResult_ {
    struct {
        vdo_win_t xBody;
        vdo_win_t xHead;
        uint32_t ulConfidence;
        uint16_t usPersonID;
    } xObjs[8];
    uint8_t ucDetObjNum;
    uint16_t usFrameDelay; /* for AI cascade */
} xNPUDetResult;

#endif //LVICASCADINGVIDEOPROJECT_LVICAMERA_H
