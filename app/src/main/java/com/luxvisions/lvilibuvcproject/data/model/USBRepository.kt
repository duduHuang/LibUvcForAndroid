package com.luxvisions.lvilibuvcproject.data.model

import android.content.Context
import android.hardware.usb.UsbDevice
import android.view.Surface
import com.luxvisions.libuvccamera.IFrameCallback
import com.luxvisions.lvilibuvcproject.data.model.usb.DeviceFilter
import com.luxvisions.lvilibuvcproject.data.model.usb.OnDeviceConnectListener
import com.luxvisions.lvilibuvcproject.data.model.usb.USBCameraDataSource
import com.luxvisions.lvilibuvcproject.data.model.usb.USBDataSource

class USBRepository(
    private val usbDataSource: USBDataSource,
    private val usbCameraDataSource: USBCameraDataSource
) {
    fun init(context: Context, onDeviceConnectListener: OnDeviceConnectListener) {
        usbDataSource.init(context, onDeviceConnectListener)
        usbCameraDataSource.init("USBCamera")
    }

    fun destroy() {
        usbDataSource.destroy()
        usbCameraDataSource.destroy()
    }

    fun register() {
        usbDataSource.register()
    }

    fun unregister() {
        usbDataSource.unregister()
        usbCameraDataSource.release()
    }

    fun isRegister(): Boolean {
        return usbDataSource.isRegistered()
    }

    fun getDeviceList(filter: DeviceFilter): List<UsbDevice>? {
        return usbDataSource.getDeviceList(filter)
    }

    fun requestPermission(device: UsbDevice): Boolean {
        return usbDataSource.requestPermission(device)
    }

    fun requestPermission(device: List<UsbDevice>): Boolean {
        return usbDataSource.requestPermission(device)
    }

    fun openDevice(device: UsbDevice): USBDataSource.UsbControlBlock {
        return usbDataSource.openDevice(device)
    }

    fun openCamera(ctrlBlock: USBDataSource.UsbControlBlock) {
        usbCameraDataSource.connect(
            ctrlBlock.getVendorId(),
            ctrlBlock.getProductId(),
            ctrlBlock.getFileDescriptor(),
            ctrlBlock.getBusNum(),
            ctrlBlock.getDevNum()
        )
    }

    fun setPreviewSize(
        width: Int, height: Int,
        minFps: Int, maxFps: Int,
        mode: Int, bandwidth: Float
    ) {
        usbCameraDataSource.setPreviewSize(
            width, height, minFps, maxFps, mode, bandwidth
        )
    }

    fun setPreviewDisplay(surface: Surface) {
        usbCameraDataSource.setPreviewDisplay(surface)
    }

    fun startPreview() {
        usbCameraDataSource.startPreview()
    }

    fun stopPreview() {
        usbCameraDataSource.stopPreview()
    }

    fun setFrameCallback(iFrameCallback: IFrameCallback, pixelFormat: Int) {
        usbCameraDataSource.setFrameCallback(iFrameCallback, pixelFormat)
    }
}