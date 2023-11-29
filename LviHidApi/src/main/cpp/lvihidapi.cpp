#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <jni.h>
#include <cassert>
#include <string>
#include "hidapi_libusb.h"
#include "logs.h"
#include "lvicamera.h"

#define ID_CASCADE_DONT_CARE 0
#define ID_CASCADE_IN       1
#define ID_CASCADE_OUT      2

#define HEADER_STR 6
#define MAX_STR 256

int readHidData(hid_device *handle, unsigned char *buf, int offset) {
    int res = hid_read_timeout(handle, buf + offset, MAX_STR, 500);
    return res;
}

int requestHidData(hid_device *handle, int fd) {
    lviCascadeRptPkt_t pkt;
    pkt.report_id = 0;
    pkt.Cmd = LVI_HIDC_SET;
    pkt.report_len = 1;
    pkt.pktCnt = 0;
    pkt.pktNum = 1;
    pkt.dat[0] = 1;
    auto *buf = new unsigned char[HEADER_STR + MAX_STR];
    memset(buf, 0, HEADER_STR + MAX_STR);
    int i = 0;
    buf[i++] = pkt.report_id;
    buf[i++] = pkt.Cmd;
    buf[i++] = pkt.report_len;
    buf[i++] = pkt.report_len >> 8;
    buf[i++] = pkt.pktCnt;
    buf[i++] = pkt.pktNum;
    buf[i++] = 1;

    int res = hid_write(handle, buf, HEADER_STR + MAX_STR);
    if (0 > res)
        LOGD("Fd: %d, HID write data 1 failed...", fd);
    else
        LOGD("Fd: %d, HID write data 1 success!!", fd);
    delete[] buf;
    return res;
}

int stopHidData(hid_device *handle, int fd) {
    lviCascadeRptPkt_t pkt;
    pkt.report_id = 0;
    pkt.Cmd = LVI_HIDC_SET;
    pkt.report_len = 1;
    pkt.pktCnt = 0;
    pkt.pktNum = 1;
    pkt.dat[0] = 0;
    auto *buf = new unsigned char[HEADER_STR + MAX_STR];
    memset(buf, 0, HEADER_STR + MAX_STR);
    int i = 0;
    buf[i++] = pkt.report_id;
    buf[i++] = pkt.Cmd;
    buf[i++] = pkt.report_len;
    buf[i++] = pkt.report_len >> 8;
    buf[i++] = pkt.pktCnt;
    buf[i++] = pkt.pktNum;
    buf[i++] = 0;

    int res = hid_write(handle, buf, HEADER_STR + MAX_STR);
    if (0 > res)
        LOGD("Fd: %d, HID write data 0 failed...", fd);
    else
        LOGD("Fd: %d, HID write data 0 success!!", fd);
    delete[] buf;
    return res;
}

void printPkt(lviCascadeRptPkt_t pkt) {
    int i = 0;
    LOGD("report_id: %d", pkt.report_id);
    LOGD("Cmd: %d", pkt.Cmd);
    LOGD("report_len: %d", pkt.report_len);
    LOGD("pktCnt: %02x", pkt.pktCnt);
    LOGD("pktNum: %02x", pkt.pktNum);
    for (i = 0; i < HEADER_STR; i++)
        LOGD("dat: %02x", pkt.dat[i]);
    LOGD("\n");
}

bool mergePkt(
        int &lastPktCnt,
        lviCascadeRptPkt_t pkt,
        uint8_t **data,
        int &size
) {
//    printPkt(pkt);
    if (0 == pkt.pktCnt) {
        delete[] *data;
        *data = new uint8_t[pkt.report_len];
        memcpy(*data, pkt.dat, pkt.report_len);
        size = pkt.report_len;
        lastPktCnt = 0;
    } else if ((lastPktCnt + 1) == pkt.pktCnt) {
        uint8_t tmp[size];
        memcpy(tmp, *data, size);
        delete[] *data;
        *data = new uint8_t[size + pkt.report_len];
        memcpy(*data, tmp, size);
        memcpy(*data + size, pkt.dat, pkt.report_len);
        size += pkt.report_len;
        lastPktCnt++;
    } else {
        size = 0;
    }
//    LOGD("lastPktCnt: %d, pkt.pktCnt: %d, pkt.pktNum: %d", lastPktCnt, pkt.pktCnt, pkt.pktNum);
    return (pkt.pktNum != (lastPktCnt + 1));
}

int receiveHidData(
        int fileDescriptor,
        uint8_t **resultData,
        bool requestHIDData
) {
    int res = 0;
    hid_device *handle;
    bool isLooping = true;
    lviCascadeRptPkt_t pkt;
    int lastPktCnt = 0;
    int size = 0;
    int i = 0;
    const int ASPEED_DEVICE_INTERFACE = 7;
    const int READ_TIME_OUT_THRESHOULD = 30;
    memset(pkt.byte, 0, sizeof(pkt.byte));

    handle = hid_libusb_wrap_sys_device(fileDescriptor, ASPEED_DEVICE_INTERFACE);
    if (handle == nullptr) {
        LOGD("Unable to open fd:%d device", fileDescriptor);
        return size;
    }

//    deviceInfo(handle);

    // Set the hid_read() function to be non-blocking.
//    hid_set_nonblocking(handle, 1);
    if (requestHIDData) {
        stopHidData(handle, fileDescriptor);
        usleep(2000 * 1000);
        requestHidData(handle, fileDescriptor);
        usleep(2000 * 1000);
    }
    res = 0;

    while (isLooping) {
        int count = 0;
        while (res == 0) {
            res = readHidData(handle, pkt.byte, count);
            if (res == 0) {
                LOGD("%d waiting...\n", fileDescriptor);
            } else if (res < 0) {
                LOGD("Fd: %d unable to read(): %ls\n", fileDescriptor, hid_error(handle));
                break;
            } else if (sizeof(pkt.byte) > count + res) {
//                LOGD("Invalid data... res = %d", res + count);
                count += res;
                res = 0;
            } else if (sizeof(pkt.byte) < count + res) {
                LOGD("overflow data... res = %d", res + count);
                res = 0;
                count = 0;
            } else if (sizeof(pkt.byte) == count + res) {
                break;
            }
//            LOGD("buf[0]: %d, buf[1]: %d", buf[0], buf[1]);
//            LOGD("buf[4]: %d, buf[5]: %d", buf[4], buf[5]);
            i++;
            if (i >= READ_TIME_OUT_THRESHOULD) { /* 10 tries by 500 ms - 5 seconds of waiting*/
                i = 0;
                LOGD("FileDescriptor: %d read() timeout\n", fileDescriptor);
                usleep(2000 * 1000);
                res = 0;
                break;
            }
        }

        if (res > 0) {
            if (ID_CASCADE_DONT_CARE == pkt.byte[0] && LVI_HIDC_NOTIFY == pkt.byte[1]) {
                isLooping = mergePkt(lastPktCnt, pkt, resultData, size);
//                LOGD("FileDescriptor: %d, resultData size: %d, isLooping: %d",
//                     fileDescriptor, size, isLooping
//                );
//                if (0 != size)
//                    LOGD("FileDescriptor: %d, *resultData [0]: %d, *resultData [1]: %d",
//                         fileDescriptor, ((*resultData)[0]), ((*resultData)[1])
//                    );

                if (0 == size)
                    isLooping = true;

                if (requestHIDData && 0 != size) {
                    if (0 != ((*resultData)[1])) {
                        usleep(2000 * 1000);
                        isLooping = true;
                    }
                }
            }
            res = 0;
            i = 0;
        }
        memset(pkt.byte, 0, sizeof(pkt.byte));
    }
    // Close the device
    hid_close(handle);

    return size;
}

void deviceInfo(hid_device *handle) {
    int res;
    wchar_t wStr[MAX_STR];
    // Read the Manufacturer String
    res = hid_get_manufacturer_string(handle, wStr, MAX_STR);
    if (res < 0)
        LOGD("Unable to read manufacturer string");
    LOGD("Manufacturer String: %ls", wStr);

    // Read the Product String
    res = hid_get_product_string(handle, wStr, MAX_STR);
    if (res < 0)
        LOGD("Unable to read product string");
    LOGD("Product String: %ls\n", wStr);

    // Read the Serial Number String
    res = hid_get_serial_number_string(handle, wStr, MAX_STR);
    if (res < 0)
        LOGD("Unable to read serial number string");
    LOGD("Serial Number String: (%d) %ls\n", wStr[0], wStr);

    // Read Indexed String 1
    res = hid_get_indexed_string(handle, 1, wStr, MAX_STR);
    if (res < 0)
        LOGD("Unable to read indexed string");
    LOGD("Indexed String 1: %ls\n", wStr);
}

JNIEXPORT jint JNICALL jHidApiInit(
        JNIEnv *env, jobject
) {
    int res = hid_init();
    if (res < 0)
        LOGD("init hid failed");

    return res;
}

JNIEXPORT jint JNICALL jHidApiExit(
        JNIEnv *env, jobject
) {
    int res = hid_exit();
    if (res < 0)
        LOGD("exit hid failed");

    return res;
}

JNIEXPORT jbyteArray JNICALL jHidApiStart(
        JNIEnv *env, jobject,
        jint vendorId,
        jint productId,
        jint fileDescriptor,
        jboolean jRequestHidData
//        jint secFileDescriptor
) {
    uint8_t *resultData = nullptr;
    bool requestHIDData = jRequestHidData;
//    unrooted_usb_description(fileDescriptor);

    int size = receiveHidData(fileDescriptor, &resultData, requestHIDData);

    jbyteArray jCharArray = env->NewByteArray(size);
    jbyte *ptr = env->GetByteArrayElements(jCharArray, nullptr);
    memcpy(ptr, resultData, size);
    env->SetByteArrayRegion(jCharArray, 0, size, ptr);
    env->ReleaseByteArrayElements(jCharArray, ptr, 0);

    // Finalize the hidapi library
    delete[] resultData;

    return jCharArray;
}

JNIEXPORT jint JNICALL jHidApiStop(
        JNIEnv *env, jobject,
        jint vendorId,
        jint productId,
        jint fileDescriptor
) {
    int res = 0;
    hid_device *handle;
    const int ASPEED_DEVICE_INTERFACE = 7;

    handle = hid_libusb_wrap_sys_device(fileDescriptor, ASPEED_DEVICE_INTERFACE);
    if (handle == nullptr) {
        LOGD("Unable to open fd:%d device", fileDescriptor);
        return -1;
    }
    res = stopHidData(handle, fileDescriptor);

    hid_close(handle);

    return res;
}

void printNPUDetResult(xNPUDetResult *obj, int filtered_box) {
    LOGD("obj->xObjs[filtered_box].ulConfidence: %d", obj->xObjs[filtered_box].ulConfidence);
    LOGD("obj->xObjs[filtered_box].xBody.x: %d", obj->xObjs[filtered_box].xBody.x);
    LOGD("obj->xObjs[filtered_box].xBody.y: %d", obj->xObjs[filtered_box].xBody.y);
    LOGD("obj->xObjs[filtered_box].xBody.width: %d", obj->xObjs[filtered_box].xBody.width);
    LOGD("obj->xObjs[filtered_box].xBody.height: %d", obj->xObjs[filtered_box].xBody.height);
    LOGD("obj->xObjs[filtered_box].xHead.x: %d", obj->xObjs[filtered_box].xHead.x);
    LOGD("obj->xObjs[filtered_box].xHead.y: %d", obj->xObjs[filtered_box].xHead.y);
    LOGD("obj->xObjs[filtered_box].xHead.width: %d", obj->xObjs[filtered_box].xHead.width);
    LOGD("obj->xObjs[filtered_box].xHead.height: %d", obj->xObjs[filtered_box].xHead.height);
    LOGD("obj->xObjs[filtered_box].usPersonID: %d", obj->xObjs[filtered_box].usPersonID);
}

JNIEXPORT jbyteArray JNICALL jHidApiAutoFraming(
        JNIEnv *env, jobject,
        jbyteArray jByteArray,
        jint fileDescriptor
) {
    auto *pData = reinterpret_cast<unsigned char *>(env->GetByteArrayElements(jByteArray, nullptr));
    float Rsrc_w = 1;//1280 / 1440.0f;
    float Rsrc_h = 1;//695.6f / 640.0f;
    int filtered_box = 0;
    jsize size = env->GetArrayLength(jByteArray);

    auto *obj = static_cast<xNPUDetResult *>(malloc(sizeof(xNPUDetResult)));
//    LOGD("jByteArray size:%d", size);
//    LOGD("pData[1]:%d, pData[2]:%d, Rsrc_w:%f, Rsrc_h:%f", pData[1], pData[2], Rsrc_w, Rsrc_h);
//    LOGD("obj->xObjs[filtered_box].xHead.y: %d, obj->xObjs[filtered_box].xBody.y: %d", obj->xObjs[filtered_box].xHead.y, obj->xObjs[filtered_box].xBody.y);
    obj->ucDetObjNum = pData[3] + (pData[4] << 8);
    LOGD("fileDescriptor:%d, ucDetObjNum: %d", fileDescriptor, obj->ucDetObjNum);
    auto *tmp = pData + 5;
    for (int i = 0; i < obj->ucDetObjNum; i++) {
        auto *inference = (m_NPU_COORDINATE *) (tmp);

        obj->xObjs[filtered_box].ulConfidence = (uint32_t) inference->sConfidence;
        obj->xObjs[filtered_box].xBody.x = (uint32_t) (inference->usXCoordnate * Rsrc_w);
        obj->xObjs[filtered_box].xBody.y = (uint32_t) (inference->usYCoordnate * Rsrc_h);
        obj->xObjs[filtered_box].xBody.width = (uint32_t) (inference->usWidth * Rsrc_w);
        obj->xObjs[filtered_box].xBody.height = (uint32_t) (inference->usHeight * Rsrc_h);
        obj->xObjs[filtered_box].xHead.x = (uint16_t) (inference->xNpuHeadInfo.usXCoordnate *
                                                       Rsrc_w);
        obj->xObjs[filtered_box].xHead.y = (uint16_t) (inference->xNpuHeadInfo.usYCoordnate *
                                                       Rsrc_h);
        obj->xObjs[filtered_box].xHead.width = (uint16_t) (inference->xNpuHeadInfo.usWidth *
                                                           Rsrc_w);
        obj->xObjs[filtered_box].xHead.height = (uint16_t) (inference->xNpuHeadInfo.usHeight *
                                                            Rsrc_h);
        obj->xObjs[filtered_box].usPersonID = (uint16_t) inference->xNpuHeadInfo.usID;

//        LOGD("i: %d", i);
//        printNPUDetResult(obj, filtered_box);
        filtered_box += 1;
        tmp += sizeof(m_NPU_COORDINATE);
    }

    env->ReleaseByteArrayElements(jByteArray, reinterpret_cast<jbyte *>(pData), 0);

//    jbyteArray jCharArray = env->NewByteArray(sizeof(xNPUDetResult));
//    jbyte *ptr = env->GetByteArrayElements(jCharArray, nullptr);
//    memcpy(ptr, obj, sizeof(xNPUDetResult));
//    env->SetByteArrayRegion(jCharArray, 0, sizeof(xNPUDetResult), ptr);
//    env->ReleaseByteArrayElements(jCharArray, ptr, 0);

    // Finalize the hidapi library
    delete[] obj;

    return jByteArray;
}

static JNINativeMethod gMethods[] = {
        {"nativeHidApiInit",        "()I",      (void *) jHidApiInit},
        {"nativeHidApiExit",        "()I",      (void *) jHidApiExit},
        {"nativeHidApiStart",       "(IIIZ)[B", (void *) jHidApiStart},
        {"nativeHidApiStop",        "(III)I",   (void *) jHidApiStop},
        {"nativeHidApiAutoFraming", "([BI)[B",  (void *) jHidApiAutoFraming},
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