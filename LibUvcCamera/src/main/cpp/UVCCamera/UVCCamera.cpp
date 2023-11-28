//
// Created by TedHuang on 2023/11/27.
//
#include <cstdlib>
#include <linux/time.h>
#include <unistd.h>
#include <cstring>
#include <android/native_window.h>
#include <jni.h>
#include "libuvc/libuvc.h"
#include "libuvc/libuvc_internal.h"
#include "UVCPreview.h"
#include "UVCCamera.h"

UVCCamera::UVCCamera() : mFd(0), mUsbFs(nullptr), mContext(nullptr), mDevice(nullptr),
                         mDeviceHandle(nullptr) {

}

UVCCamera::~UVCCamera() {
    if (mUsbFs) {
        free(mUsbFs);
        mUsbFs = nullptr;
    }
}

int UVCCamera::init(const char *usbFs) {
    if (mUsbFs)
        free(mUsbFs);
    mUsbFs = strdup(usbFs);
    if (nullptr == mContext) {
        uvc_error_t result = uvc_init(&mContext, nullptr);
        if (result)
            return -1;
    }
    return EXIT_SUCCESS;
}

int UVCCamera::exit() {
    if (mContext) {
        uvc_exit(mContext);
        mContext = nullptr;
    }
    return EXIT_SUCCESS;
}

int UVCCamera::connect(
        int vid, int pid, int fd,
        int busNum, int devAddress
) {
    uvc_error_t result = UVC_ERROR_BUSY;
    if (!mDeviceHandle && fd) {
        fd = dup(fd);
        result = uvc_wrap(fd, mContext, &mDeviceHandle);
        if (!result) {
            mDevice = mDeviceHandle->dev;
            mFd = fd;
            mPreview = new UVCPreview(mDeviceHandle);
        } else {
            uvc_unref_device(mDevice);
            mDevice = nullptr;
            mDeviceHandle = nullptr;
            close(fd);
        }
    } else {

    }
    return result;
}

int UVCCamera::release() {
    if (mDeviceHandle) {
        delete mPreview;
        mPreview = nullptr;
        uvc_close(mDeviceHandle);
        mDeviceHandle = nullptr;
    }
    if (mDevice) {
        uvc_unref_device(mDevice);
        mDevice = nullptr;
    }
    if (mUsbFs) {
        close(mFd);
        mFd = 0;
        free(mUsbFs);
        mUsbFs = nullptr;
    }
    return EXIT_SUCCESS;
}

int UVCCamera::setPreviewSize(
        int width, int height,
        int minFps, int maxFps,
        int mode, float bandwidth) {
    int result = EXIT_FAILURE;
    if (mPreview)
        result = mPreview->setPreviewSize(
                width, height,
                minFps, maxFps,
                mode, bandwidth
                );
    return result;
}

int UVCCamera::setPreviewDisplay(ANativeWindow *previewWindow) {
    int result = EXIT_FAILURE;
    if (mPreview)
        result = mPreview->setPreviewDisplay(previewWindow);
    return result;
}

int UVCCamera::setFrameCallback(JNIEnv *env, jobject frameCallbackObj, int pixelFormat) {
    int result = EXIT_FAILURE;
    if (mPreview)
        result = mPreview->setFrameCallback(env, frameCallbackObj, pixelFormat);
    return result;
}

int UVCCamera::startPreview() {
    int result = EXIT_FAILURE;
    if (mPreview)
        result = mPreview->startPreview();
    return result;
}

int UVCCamera::stopPreview() {
    int result = EXIT_FAILURE;
    if (mPreview)
        result = mPreview->stopPreview();
    return result;
}
