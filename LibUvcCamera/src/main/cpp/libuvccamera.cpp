#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cassert>
#include <string>
#include <jni.h>
#include "logs.h"
#include "UVCCamera/UVCCamera.h"
#include "libuvccamera.h"

JNIEXPORT ID_TYPE JNICALL nativeCreate(JNIEnv *env, jobject) {
#if LOCAL_DEBUG
    LOGD("Create...");
#endif
    auto *camera = new UVCCamera();
    return reinterpret_cast<ID_TYPE>(camera);
}

JNIEXPORT jint JNICALL nativeDestroy(JNIEnv *env, jobject, ID_TYPE idCamera) {
#if LOCAL_DEBUG
    LOGD("Destroy...");
#endif
    auto *camera = reinterpret_cast<UVCCamera *>(idCamera);
    delete camera;
    return JNI_OK;
}

JNIEXPORT jint JNICALL nativeInit(
        JNIEnv *env, jobject,
        ID_TYPE idCamera,
        jstring usbFsStr
) {
#if LOCAL_DEBUG
    LOGD("Init...");
#endif
    int result = JNI_ERR;
    auto *camera = reinterpret_cast<UVCCamera *>(idCamera);
    const char *cUsbFs = env->GetStringUTFChars(usbFsStr, JNI_FALSE);
    if (camera)
        result = camera->init(cUsbFs);
    env->ReleaseStringUTFChars(usbFsStr, cUsbFs);
    return result;
}

JNIEXPORT jint JNICALL nativeRelease(JNIEnv *env, jobject, ID_TYPE idCamera) {
#if LOCAL_DEBUG
    LOGD("Release...");
#endif
    auto *camera = reinterpret_cast<UVCCamera *>(idCamera);
    if (camera)
        camera->release();
    return JNI_OK;
}

JNIEXPORT jint JNICALL nativeConnect(
        JNIEnv *env, jobject,
        ID_TYPE idCamera,
        jint vid, jint pid, jint fd,
        jint busNum, jint devAddress
) {
#if LOCAL_DEBUG
    LOGD("Connect...");
#endif
    int result = JNI_ERR;
    auto *camera = reinterpret_cast<UVCCamera *>(idCamera);
    if (camera && fd > 0)
        result = camera->connect(vid, pid, fd, busNum, devAddress);
    return result;
}

static JNINativeMethod gMethods[] = {
        {"nativeCreate",  "()J", (void *) nativeCreate},
        {"nativeDestroy", "(J)I", (void *) nativeDestroy},
        {"nativeInit",    "(JLjava/lang/String;)I", (void *) nativeInit},
        {"nativeRelease", "(J)I", (void *) nativeRelease},
        {"nativeConnect", "(JIIIII)I", (void *) nativeConnect},
};

static const char *const kClassPathName = "com/luxvisions/lvicamera/LviCamera";

static int registerNativeMethods(
        JNIEnv *env,
        const char *className,
        int numMethods
) {
    jclass clazz;
    clazz = env->FindClass(className);
    if (clazz == nullptr) {
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

static int registerFunctions(JNIEnv *env) {
    LOGD("register [%s]%d", __FUNCTION__, __LINE__);
    return registerNativeMethods(
            env,
            kClassPathName,
            sizeof(gMethods) / sizeof(gMethods[0])
    );
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("onLoader");
    JNIEnv *env = nullptr;
    jint result = -1;

    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        LOGD("ERROR: GetEnv failed\n");
        goto bail;
    }
    assert(env != nullptr);
    if (registerFunctions(env) < 0) {
        LOGE(" onLoader ERROR: Preview native registration failed\n");
        goto bail;
    }
    LOGD("onLoader register ok ! [%s]%d", __FUNCTION__, __LINE__);
    result = JNI_VERSION_1_4;

    bail:
    return result;
}