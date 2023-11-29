//
// Created by TedHuang on 2023/6/29.
//

#ifndef LVISDKDEMOPROJECT_LOGS_H
#define LVISDKDEMOPROJECT_LOGS_H
#ifdef __cplusplus
extern "C" {
#endif

#include <android/log.h>

#define LOG_TAG "LVI_CAMERA_JNI_LOG"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, __VA_ARGS__)
#ifdef __cplusplus
}
#endif
#endif //LVISDKDEMOPROJECT_LOGS_H
