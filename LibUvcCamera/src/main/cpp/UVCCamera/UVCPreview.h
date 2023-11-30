//
// Created by TedHuang on 2023/11/27.
//

#pragma interface

#ifndef LVILIBUVCPROJECT_UVCPREVIEW_H
#define LVILIBUVCPROJECT_UVCPREVIEW_H

#include "libuvc/libuvc.h"
#include "libuvc/libuvc_internal.h"
#include "objectarray.h"

#define DEFAULT_PREVIEW_WIDTH 240
#define DEFAULT_PREVIEW_HEIGHT 2320
#define DEFAULT_PREVIEW_FPS_MIN 1
#define DEFAULT_PREVIEW_FPS_MAX 25
#define DEFAULT_PREVIEW_MODE 0
#define DEFAULT_BANDWIDTH 1.0f

#define PIXEL_FORMAT_RAW 0        // same as PIXEL_FORMAT_YUV
#define PIXEL_FORMAT_YUV 1
#define PIXEL_FORMAT_RGB565 2
#define PIXEL_FORMAT_RGBX 3
#define PIXEL_FORMAT_YUV20SP 4
#define PIXEL_FORMAT_NV21 5        // YVU420SemiPlanar

typedef uvc_error_t (*convFunc_t)(uvc_frame_t *in, uvc_frame_t *out);

// for callback to Java object
typedef struct {
    jmethodID onFrame;
} FieldsIFrameCallback;

int copyToSurface(uvc_frame_t *frame, ANativeWindow **window);

class UVCPreview {
private:
    uvc_device_handle_t *mDeviceHandle;
    ANativeWindow *mPreviewWindow;
    volatile bool bIsRunning;
    int requestWidth, requestHeight, requestMode;
    int requestMinFps, requestMaxFps;
    float requestBandwidth;
    int frameWidth, frameHeight, frameMode;
    size_t frameBytes;
    pthread_t previewPthread;
    pthread_mutex_t previewMutex;
    pthread_cond_t previewSync;
    ObjectArray<uvc_frame_t *> previewFrames;
    int previewFormat;
    size_t previewBytes;

    int mPixelFormat;
    size_t callbackPixelBytes;
    convFunc_t mFrameCallbackFunc;

    pthread_mutex_t poolMutex;
    ObjectArray<uvc_frame_t *> mFramePool;

    uvc_frame_t *getFrame(size_t dataBytes);

    void recycleFrame(uvc_frame_t *frame);

    void initPool(size_t dataBytes);

    void clearPool();

    static void uvcPreviewFrameCallback(uvc_frame_t *frame, void *vptrArgs);

    static void *previewThreadFunc(void *vptrArgs);

    int preparePreview(uvc_stream_ctrl_t *ctrl);

    void doPreview(uvc_stream_ctrl_t *ctrl);

    void addPreviewFrame(uvc_frame_t *frame);

    uvc_frame_t *waitPreviewFrame();

    void clearPreviewFrame();

    uvc_frame_t *drawPreviewOne(
            uvc_frame_t *frame,
            ANativeWindow **window,
            convFunc_t func,
            int pixelBytes
    );

    void callbackPixelFormatChanged();

public:
    UVCPreview(uvc_device_handle_t *deviceHandle);

    ~UVCPreview();

    inline bool isRunning() const;

    int setPreviewSize(
            int width, int height,
            int minFps, int maxFps,
            int mode, float bandwidth
    );

    int setPreviewDisplay(ANativeWindow *previewWindow);

    int setFrameCallback(JNIEnv *env, jobject frameCallbackObj, int pixelFormat);

    int startPreview();

    int stopPreview();

};

#endif //LVILIBUVCPROJECT_UVCPREVIEW_H
