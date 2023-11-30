//
// Created by TedHuang on 2023/11/27.
//
#include <cstdlib>
#include <linux/time.h>
#include <unistd.h>
#include <android/native_window.h>
#include <jni.h>

#define LIBUVC_HAS_JPEG

#include "libuvc/libuvc.h"
#include "libuvc/libuvc_internal.h"
#include "UVCPreview.h"

#define MAX_FRAME 4
#define PREVIEW_PIXEL_BYTES 4    // RGBA/RGBX
#define FRAME_POOL_SZ (MAX_FRAME + 2)

UVCPreview::UVCPreview(uvc_device_handle_t *deviceHandle)
        : mDeviceHandle(deviceHandle),
          mPreviewWindow(nullptr),
          requestWidth(DEFAULT_PREVIEW_WIDTH),
          requestHeight(DEFAULT_PREVIEW_HEIGHT),
          requestMinFps(DEFAULT_PREVIEW_FPS_MIN),
          requestMaxFps(DEFAULT_PREVIEW_FPS_MAX),
          requestMode(DEFAULT_PREVIEW_MODE),
          requestBandwidth(DEFAULT_BANDWIDTH),
          frameWidth(DEFAULT_PREVIEW_WIDTH),
          frameHeight(DEFAULT_PREVIEW_HEIGHT),
          frameBytes(DEFAULT_PREVIEW_WIDTH * DEFAULT_PREVIEW_HEIGHT * 2),    // YUYV
          frameMode(0),
          previewBytes(DEFAULT_PREVIEW_WIDTH * DEFAULT_PREVIEW_HEIGHT * PREVIEW_PIXEL_BYTES),
          previewFormat(WINDOW_FORMAT_RGBA_8888),
          bIsRunning(false),
          mFrameCallbackFunc(nullptr),
          callbackPixelBytes(2) {
    pthread_cond_init(&previewSync, nullptr);
    pthread_mutex_init(&previewMutex, nullptr);
    pthread_mutex_init(&poolMutex, nullptr);
}

UVCPreview::~UVCPreview() {
    if (mPreviewWindow)
        ANativeWindow_release(mPreviewWindow);
    mPreviewWindow = nullptr;
    clearPreviewFrame();
    clearPool();
    pthread_mutex_destroy(&previewMutex);
    pthread_cond_destroy(&previewSync);
    pthread_mutex_destroy(&poolMutex);
}

uvc_frame_t *UVCPreview::getFrame(size_t dataBytes) {
    uvc_frame_t *frame = nullptr;
    pthread_mutex_lock(&poolMutex);
    {
        if (!mFramePool.isEmpty()) {
            frame = mFramePool.last();
        }
    }
    pthread_mutex_unlock(&poolMutex);
    if (!frame)
        frame = uvc_allocate_frame(dataBytes);
    return frame;
}

void UVCPreview::recycleFrame(uvc_frame_t *frame) {
    pthread_mutex_lock(&poolMutex);
    if (mFramePool.size() < FRAME_POOL_SZ) {
        mFramePool.put(frame);
        frame = nullptr;
    }
    pthread_mutex_unlock(&poolMutex);
    if (frame)
        uvc_free_frame(frame);
}

void UVCPreview::initPool(size_t dataBytes) {
    clearPool();
    pthread_mutex_lock(&poolMutex);

    for (int i = 0; i < FRAME_POOL_SZ; ++i)
        mFramePool.put(uvc_allocate_frame(dataBytes));

    pthread_mutex_unlock(&poolMutex);
}

void UVCPreview::clearPool() {
    pthread_mutex_lock(&poolMutex);

    const int n = mFramePool.size();
    for (int i = 0; i < n; i++)
        uvc_free_frame(mFramePool[i]);
    mFramePool.clear();

    pthread_mutex_unlock(&poolMutex);
}

inline bool UVCPreview::isRunning() const { return bIsRunning; }

int UVCPreview::setPreviewSize(int width, int height, int minFps, int maxFps, int mode,
                               float bandwidth) {
    int result = 0;
    if ((requestWidth != width) || (requestHeight != height) || (requestMode != mode)) {
        requestWidth = width;
        requestHeight = height;
        requestMinFps = minFps;
        requestMaxFps = maxFps;
        requestMode = mode;
        requestBandwidth = bandwidth;

        uvc_stream_ctrl_t ctrl;
        result =
                uvc_get_stream_ctrl_format_size_fps(
                        mDeviceHandle, &ctrl,
                        !requestMode ? UVC_FRAME_FORMAT_YUYV : UVC_FRAME_FORMAT_MJPEG,
                        requestWidth, requestHeight,
                        requestMinFps, requestMaxFps
                );
    }

    return result;
}

int UVCPreview::setPreviewDisplay(ANativeWindow *previewWindow) {
    pthread_mutex_lock(&previewMutex);

    if (mPreviewWindow != previewWindow) {
        if (mPreviewWindow)
            ANativeWindow_release(mPreviewWindow);
        mPreviewWindow = previewWindow;
        if (mPreviewWindow)
            ANativeWindow_setBuffersGeometry(
                    mPreviewWindow,
                    frameWidth, frameHeight, previewFormat
            );
    }

    pthread_mutex_unlock(&previewMutex);
    return EXIT_SUCCESS;
}

int UVCPreview::setFrameCallback(JNIEnv *env, jobject frameCallbackObj, int pixelFormat) {
    // Capture used...
    if (frameCallbackObj) {
        mPixelFormat = pixelFormat;
//        callbackPixelFormatChanged();
    }
}

int UVCPreview::startPreview() {
    int result = EXIT_FAILURE;
    if (!isRunning()) {
        bIsRunning = true;
        pthread_mutex_lock(&previewMutex);

        if (mPreviewWindow)
            result = pthread_create(
                    &previewPthread,
                    nullptr,
                    previewThreadFunc,
                    (void *) this
            );

        pthread_mutex_unlock(&previewMutex);

        if (result != EXIT_SUCCESS) {
            bIsRunning = false;
            pthread_mutex_lock(&previewMutex);

            pthread_cond_signal(&previewSync);

            pthread_mutex_unlock(&previewMutex);
        }
    }

    return result;
}

int UVCPreview::stopPreview() {
    if (isRunning()) {
        bIsRunning = false;
        pthread_cond_signal(&previewSync);
        pthread_join(previewPthread, nullptr);
    }

    pthread_mutex_lock(&previewMutex);
    if (mPreviewWindow) {
        ANativeWindow_release(mPreviewWindow);
        mPreviewWindow = nullptr;
    }
    pthread_mutex_unlock(&previewMutex);

    return EXIT_SUCCESS;
}

void *UVCPreview::previewThreadFunc(void *vptrArgs) {
    auto *preview = reinterpret_cast<UVCPreview *>(vptrArgs);
    if (preview) {
        uvc_stream_ctrl_t ctrl;
        int result = preview->preparePreview(&ctrl);
        if (!result)
            preview->doPreview(&ctrl);
    }
    pthread_exit(nullptr);
}

int UVCPreview::preparePreview(uvc_stream_ctrl_t *ctrl) {
    uvc_error_t result;
    result = uvc_get_stream_ctrl_format_size_fps(
            mDeviceHandle, ctrl,
            !requestMode ? UVC_FRAME_FORMAT_YUYV : UVC_FRAME_FORMAT_MJPEG,
            requestWidth, requestHeight,
            requestMinFps, requestMaxFps
    );
    if (!result) {
        uvc_frame_desc_t *frame_desc;
        result = uvc_get_frame_desc(mDeviceHandle, ctrl, &frame_desc);
        if (!result) {
            frameWidth = frame_desc->wWidth;
            frameHeight = frame_desc->wHeight;
            pthread_mutex_lock(&previewMutex);

            if (mPreviewWindow)
                ANativeWindow_setBuffersGeometry(
                        mPreviewWindow,
                        frameWidth, frameHeight, previewFormat
                );

            pthread_mutex_unlock(&previewMutex);
        } else {
            frameWidth = requestWidth;
            frameHeight = requestHeight;
        }
        frameMode = requestMode;
        frameBytes = frameWidth * frameHeight * (!requestMode ? 2 : 4);
        previewBytes = frameWidth * frameHeight * PREVIEW_PIXEL_BYTES;
    }

    return result;
}

void UVCPreview::doPreview(uvc_stream_ctrl_t *ctrl) {
    uvc_frame_t *frame = nullptr;
    uvc_frame_t *frameMjpeg = nullptr;
    uvc_error_t result = uvc_start_streaming_bandwidth(
            mDeviceHandle, ctrl, uvcPreviewFrameCallback,
            (void *) this, requestBandwidth, 0
    );
    if (!result) {
        clearPreviewFrame();
        if (frameMode) { // MJPEG mode
            while (isRunning()) {
                frameMjpeg = waitPreviewFrame();
                if (frameMjpeg) {
                    frame = getFrame(frameMjpeg->width * frameMjpeg->height * 2);
                    result = uvc_mjpeg2yuyv(frameMjpeg, frame);
                    recycleFrame(frameMjpeg);
                    if (!result) {
                        frame = drawPreviewOne(frame, &mPreviewWindow, uvc_any2rgbx, 4);
//                        addCaptureFrame(frame);
                    } else
                        recycleFrame(frame);
                }
            }
        } else { // YUYV mode
            while (isRunning()) {
                frame = waitPreviewFrame();
                if (frame) {
                    frame = drawPreviewOne(frame, &mPreviewWindow, uvc_any2rgbx, 4);
//                    addCaptureFrame(frame);
                }
            }
        }
        uvc_stop_streaming(mDeviceHandle);
    } else {
        uvc_perror(result, "failed start streaming");
    }
}

uvc_frame_t *UVCPreview::drawPreviewOne(
        uvc_frame_t *frame,
        ANativeWindow **window,
        convFunc_t func,
        int pixelBytes
) {
    int b = 0;
    pthread_mutex_lock(&previewMutex);
    b = *window != nullptr;
    pthread_mutex_unlock(&previewMutex);
    if (b) {
        uvc_frame_t *converted;
        if (func) {
            converted = getFrame(frame->width * frame->height * pixelBytes);
            if (converted) {
                b = func(frame, converted);
                if (!b) {
                    pthread_mutex_lock(&previewMutex);
                    copyToSurface(converted, window);
                    pthread_mutex_unlock(&previewMutex);
                }
                recycleFrame(converted);
            }
        } else {
            pthread_mutex_lock(&previewMutex);
            copyToSurface(frame, window);
            pthread_mutex_unlock(&previewMutex);
        }
    }
    return frame;
}

void UVCPreview::callbackPixelFormatChanged() {
//    mFrameCallbackFunc = nullptr;
//    const size_t sz = requestWidth * requestHeight;
//    switch (mPixelFormat) {
//        case PIXEL_FORMAT_RAW:
//            LOGI("PIXEL_FORMAT_RAW:");
//            callbackPixelBytes = sz * 2;
//            break;
//        case PIXEL_FORMAT_YUV:
//            LOGI("PIXEL_FORMAT_YUV:");
//            callbackPixelBytes = sz * 2;
//            break;
//        case PIXEL_FORMAT_RGB565:
//            LOGI("PIXEL_FORMAT_RGB565:");
//            mFrameCallbackFunc = uvc_any2rgb565;
//            callbackPixelBytes = sz * 2;
//            break;
//        case PIXEL_FORMAT_RGBX:
//            LOGI("PIXEL_FORMAT_RGBX:");
//            mFrameCallbackFunc = uvc_any2rgbx;
//            callbackPixelBytes = sz * 4;
//            break;
//        case PIXEL_FORMAT_YUV20SP:
//            LOGI("PIXEL_FORMAT_YUV20SP:");
//            mFrameCallbackFunc = uvc_yuyv2iyuv420SP;
//            callbackPixelBytes = (sz * 3) / 2;
//            break;
//        case PIXEL_FORMAT_NV21:
//            LOGI("PIXEL_FORMAT_NV21:");
//            mFrameCallbackFunc = uvc_yuyv2yuv420SP;
//            callbackPixelBytes = (sz * 3) / 2;
//            break;
//    }
}

void UVCPreview::uvcPreviewFrameCallback(uvc_frame_t *frame, void *vptrArgs) {
    auto *preview = reinterpret_cast<UVCPreview *>(vptrArgs);
    if (!preview->isRunning() ||
        !frame || !frame->frame_format ||
        !frame->data ||
        !frame->data_bytes) {
        return;
    }
    if ((frame->frame_format != UVC_FRAME_FORMAT_MJPEG &&
         frame->actual_bytes < preview->frameBytes) ||
        (frame->width != preview->frameWidth) ||
        (frame->height != preview->frameHeight)) {
        return;
    }
    if (preview->isRunning()) {
        uvc_frame_t *copy = preview->getFrame(frame->data_bytes);
        if (!copy)
            return;
        uvc_error_t ret = uvc_duplicate_frame(frame, copy);
        if (ret) {
            preview->recycleFrame(copy);
            return;
        }
        preview->addPreviewFrame(copy);
    }
}

void UVCPreview::addPreviewFrame(uvc_frame_t *frame) {
    pthread_mutex_lock(&previewMutex);

    if (isRunning() && (previewFrames.size() < MAX_FRAME)) {
        previewFrames.put(frame);
        frame = nullptr;
        pthread_cond_signal(&previewSync);
    }

    pthread_mutex_unlock(&previewMutex);

    if (frame)
        recycleFrame(frame);
}

uvc_frame_t *UVCPreview::waitPreviewFrame() {
    uvc_frame_t *frame = nullptr;
    pthread_mutex_lock(&previewMutex);

    if (!previewFrames.size())
        pthread_cond_wait(&previewSync, &previewMutex);
    if ((isRunning() && previewFrames.size()) > 0)
        frame = previewFrames.remove(0);

    pthread_mutex_unlock(&previewMutex);
    return frame;
}

void UVCPreview::clearPreviewFrame() {
    pthread_mutex_lock(&previewMutex);

    for (int i = 0; i < previewFrames.size(); i++)
        recycleFrame(previewFrames[i]);
    previewFrames.clear();

    pthread_mutex_unlock(&previewMutex);
}

static void copyFrame(
        const uint8_t *src, uint8_t *dest,
        const int width, int height,
        const int strideSrc, const int strideDest
) {
    const int h8 = height % 8;
    for (int i = 0; i < h8; i++) {
        memcpy(dest, src, width);
        dest += strideDest;
        src += strideSrc;
    }
    for (int i = 0; i < height; i += 8) {
        memcpy(dest, src, width);
        dest += strideDest;
        src += strideSrc;
        memcpy(dest, src, width);
        dest += strideDest;
        src += strideSrc;
        memcpy(dest, src, width);
        dest += strideDest;
        src += strideSrc;
        memcpy(dest, src, width);
        dest += strideDest;
        src += strideSrc;
        memcpy(dest, src, width);
        dest += strideDest;
        src += strideSrc;
        memcpy(dest, src, width);
        dest += strideDest;
        src += strideSrc;
        memcpy(dest, src, width);
        dest += strideDest;
        src += strideSrc;
        memcpy(dest, src, width);
        dest += strideDest;
        src += strideSrc;
    }
}

int copyToSurface(uvc_frame_t *frame, ANativeWindow **window) {
    int result = 0;
    if (*window) {
        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(*window, &buffer, nullptr) == 0) {
            const uint8_t *src = (uint8_t *) frame->data;
            const int srcW = (int) frame->width * PREVIEW_PIXEL_BYTES;
            const int srcStep = (int) frame->width * PREVIEW_PIXEL_BYTES;
            auto *dest = (uint8_t *) buffer.bits;
            const int destW = buffer.width * PREVIEW_PIXEL_BYTES;
            const int destStep = buffer.stride * PREVIEW_PIXEL_BYTES;
            const int w = srcW < destW ? srcW : destW;
            const int h = frame->height < buffer.height ? (int) frame->height : buffer.height;
            copyFrame(
                    src, dest,
                    w, h,
                    srcStep, destStep
            );
            ANativeWindow_unlockAndPost(*window);
        } else {
            result = -1;
        }
    } else {
        result = -1;
    }

    return result;
}
