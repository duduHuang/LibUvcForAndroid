package com.luxvisions.lvilibuvcproject.data.model.usb

import android.hardware.usb.UsbDevice

interface OnDeviceConnectListener {
    fun onAttach(device: UsbDevice)

    fun onDeAttach(device: UsbDevice)

    fun onConnect(
        device: UsbDevice,
        ctrlControlBlock: USBDataSource.UsbControlBlock,
        createNew: Boolean,
        index: Int
    )

    fun onDisconnect(device: UsbDevice, ctrlControlBlock: USBDataSource.UsbControlBlock)

    fun onCancel(device: UsbDevice)
}