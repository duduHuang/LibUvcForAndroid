//
// Created by TedHuang on 2023/11/27.
//

#pragma interface

#ifndef LVILIBUVCPROJECT_UVCCAMERA_H
#define LVILIBUVCPROJECT_UVCCAMERA_H

class UVCCamera {
    char *mUsbFs;
    int mFd;
    uvc_context_t *mContext;
    uvc_device_t *mDevice;
    uvc_device_handle_t *mDeviceHandle;
    UVCPreview *mPreview;

public:
    UVCCamera();

    ~UVCCamera();

    int init(const char *usbFs);

    int exit();

    int connect(int vid, int pid, int fd, int busNum, int devAddress);

    int release();

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

#endif //LVILIBUVCPROJECT_UVCCAMERA_H
