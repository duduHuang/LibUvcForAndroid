#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cassert>
#include <string>
#include <jni.h>
#include "logs.h"
#include "lvicamera.h"

JNIEXPORT jint JNICALL jInit(JNIEnv *env, jobject) {
    LOGD("Init...");
}

JNIEXPORT jint JNICALL jRelease(JNIEnv *env, jobject) {
    LOGD("Release...");
}

static JNINativeMethod gMethods[] = {
        {"nativeInit",        "()I",      (void *) jInit},
        {"nativeRelease",        "()I",      (void *) jRelease},
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