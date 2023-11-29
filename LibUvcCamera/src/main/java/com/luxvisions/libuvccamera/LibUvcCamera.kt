package com.luxvisions.libuvccamera

import android.view.Surface

class LibUvcCamera {
    private val mNativePtr: Long

    init {
        mNativePtr = nativeCreate()
    }

    fun destroy() {
        nativeDestroy(mNativePtr)
    }

    fun init(usbFsStr: String) {
        nativeInit(mNativePtr, usbFsStr)
    }

    fun release() {
        nativeRelease(mNativePtr)
    }

    fun connect(
        vid: Int, pid: Int, fd: Int,
        busNum: Int, devAddress: Int
    ) {
        nativeConnect(
            mNativePtr,
            vid, pid, fd,
            busNum, devAddress
        )
    }

    fun setPreviewSize(
        width: Int, height: Int,
        minFps: Int, maxFps: Int,
        mode: Int, bandwidth: Float
    ) {
        nativeSetPreviewSize(
            mNativePtr,
            width, height,
            minFps, maxFps,
            mode, bandwidth
        )
    }

    fun setPreviewDisplay(surface: Surface) {
        nativeSetPreviewDisplay(mNativePtr, surface)
    }

    fun startPreview() {
        nativeStartPreview(mNativePtr)
    }

    fun stopPreview() {
        nativeStopPreview(mNativePtr)
    }

    fun setFrameCallback(iFrameCallback: IFrameCallback, pixelFormat: Int) {
        nativeSetFrameCallback(mNativePtr, iFrameCallback, pixelFormat)
    }

    /**
     * A native method that is implemented by the 'libuvccamera' native library,
     * which is packaged with this application.
     */
    private external fun nativeCreate(): Long
    private external fun nativeDestroy(idCamera: Long): Int
    private external fun nativeInit(idCamera: Long, usbFsStr: String): Int
    private external fun nativeRelease(idCamera: Long): Int
    private external fun nativeConnect(
        idCamera: Long,
        vid: Int, pid: Int, fd: Int,
        busNum: Int, devAddress: Int
    ): Int
    private external fun nativeSetPreviewSize(
        idCamera: Long,
        width: Int, height: Int,
        minFps: Int, maxFps: Int,
        mode: Int, bandwidth: Float
    ): Int
    private external fun nativeSetPreviewDisplay(idCamera: Long, surface: Surface): Int
    private external fun nativeStartPreview(idCamera: Long): Int
    private external fun nativeStopPreview(idCamera: Long): Int
    private external fun nativeSetFrameCallback(
        idCamera: Long,
        iFrameCallback: IFrameCallback,
        pixelFormat: Int
    ): Int

    companion object {
        // Used to load the 'libuvccamera' library on application startup.
        init {
            System.loadLibrary("libuvccamera")
        }
    }
}