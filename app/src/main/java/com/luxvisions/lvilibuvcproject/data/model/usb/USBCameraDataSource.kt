package com.luxvisions.lvilibuvcproject.data.model.usb

import android.view.Surface
import com.luxvisions.libuvccamera.IFrameCallback
import com.luxvisions.libuvccamera.LibUvcCamera
import java.nio.ByteBuffer

class USBCameraDataSource {
    private val mUvcCamera: LibUvcCamera by lazy {
        LibUvcCamera()
    }

    fun destroy() {
        mUvcCamera.destroy()
    }

    fun init(usbFsStr: String) {
        mUvcCamera.init(usbFsStr)
    }

    fun release() {
        mUvcCamera.release()
    }

    fun connect(
        vid: Int, pid: Int, fd: Int,
        busNum: Int, devAddress: Int
    ) {
        mUvcCamera.connect(vid, pid, fd, busNum, devAddress)
    }

    fun setPreviewSize(
        width: Int, height: Int,
        minFps: Int, maxFps: Int,
        mode: Int, bandwidth: Float
    ) {
        mUvcCamera.setPreviewSize(
            DEFAULT_PREVIEW_WIDTH, DEFAULT_PREVIEW_HEIGHT,
            DEFAULT_PREVIEW_MIN_FPS, DEFAULT_PREVIEW_MAX_FPS,
            DEFAULT_PREVIEW_MODE, DEFAULT_BANDWIDTH
        )
    }

    fun setPreviewDisplay(surface: Surface) {
        mUvcCamera.setPreviewDisplay(surface)
    }

    fun startPreview() {
        mUvcCamera.startPreview()
    }

    fun stopPreview() {
        mUvcCamera.stopPreview()
    }

    fun setFrameCallback(iFrameCallback: IFrameCallback, pixelFormat: Int) {
        mUvcCamera.setFrameCallback(iFrameCallback, pixelFormat)
    }

    companion object {
        private val sTAG = USBCameraDataSource::class.java.name
        private const val DEFAULT_PREVIEW_WIDTH = 1280
        private const val DEFAULT_PREVIEW_HEIGHT = 720
        private const val DEFAULT_PREVIEW_MODE = 0
        private const val DEFAULT_PREVIEW_MIN_FPS = 1
        private const val DEFAULT_PREVIEW_MAX_FPS = 30
        private const val DEFAULT_BANDWIDTH = 1.0f
    }
}