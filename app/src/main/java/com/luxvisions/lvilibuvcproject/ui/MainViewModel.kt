package com.luxvisions.lvilibuvcproject.ui

import android.content.Context
import android.hardware.usb.UsbDevice
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.fragment.app.FragmentActivity
import com.luxvisions.libuvccamera.IFrameCallback
import com.luxvisions.lvilibuvcproject.R
import com.luxvisions.lvilibuvcproject.base.BaseViewModel
import com.luxvisions.lvilibuvcproject.data.model.MainContract
import com.luxvisions.lvilibuvcproject.data.model.USBRepository
import com.luxvisions.lvilibuvcproject.data.model.usb.DeviceFilter
import com.luxvisions.lvilibuvcproject.data.model.usb.OnDeviceConnectListener
import com.luxvisions.lvilibuvcproject.data.model.usb.USBDataSource
import java.nio.ByteBuffer

class MainViewModel constructor(
    private val mUSBRepository: USBRepository
) : BaseViewModel<MainContract.Event, MainContract.State>() {
    private lateinit var mFilter: List<DeviceFilter>
    private lateinit var mSurface: Surface
    private var mWidth = 0
    private var mHeight = 0
    private var mFormat = 0
    override fun createInitialState(): MainContract.State {
        return MainContract.State(MainContract.UsbState.Initial)
    }

    override fun handleEvent(event: MainContract.Event) {
        when (event) {
            is MainContract.Event.InitUSBData -> {
                initUSBDataSource(
                    event.context,
                    event.activity,
                    mOnDeviceConnectListener
                )
            }

            is MainContract.Event.StartCamera -> {
                mSurface = event.surfaceView.holder.surface
                requestPermission(event.filerIndex)
//                connectDevice(event.surfaceView, filerIndex =  event.filerIndex)
            }

            is MainContract.Event.HidData -> {

            }

            is MainContract.Event.Release -> {
                release()
            }

            else -> {}
        }
    }

    fun getSurfaceHolderCallback(): SurfaceHolder.Callback {
        return mSurfaceHolderCallback
    }

    private fun initUSBDataSource(
        context: Context,
        activity: FragmentActivity,
        onDeviceConnectListener: OnDeviceConnectListener
    ) {
        mUSBRepository.init(context, onDeviceConnectListener)
        mFilter = DeviceFilter.getDeviceFilters(activity, R.xml.device_filter)
    }

    private fun release() {
        mUSBRepository.stopPreview()
        mUSBRepository.unregister()
        mUSBRepository.destroy()
    }

    private fun requestPermission(filerIndex: Int) {
        mUSBRepository.register()
        val deviceList: List<UsbDevice>? = mUSBRepository.getDeviceList(mFilter[filerIndex])
        Log.i(sTAG, "device list size: ${deviceList?.size}")
        var ret = false
        deviceList?.let {
            while (!ret)
                ret = mUSBRepository.requestPermission(it)
        }
    }

    private fun connectDevice(
        surfaceView: SurfaceView,
        deviceIndex: Int = 0,
        filerIndex: Int
    ) {
        val deviceList: List<UsbDevice>? = mUSBRepository.getDeviceList(mFilter[filerIndex])
        deviceList?.let {
            val device = it[deviceIndex]
            val ctrlBlock = mUSBRepository.openDevice(device)
            mUSBRepository.openCamera(ctrlBlock)
            mUSBRepository.setPreviewSize(
                surfaceView.width,
                surfaceView.height,
                1, 30, 0, 1.0f
            )
            mUSBRepository.setPreviewDisplay(surfaceView.holder.surface)
            mUSBRepository.setFrameCallback(mIFrameCallback, PIXEL_FORMAT_RGBX)
            mUSBRepository.startPreview()
        }
    }

    private val mOnDeviceConnectListener = object : OnDeviceConnectListener {
        override fun onAttach(device: UsbDevice) = Unit

        override fun onDeAttach(device: UsbDevice) = Unit

        override fun onConnect(
            device: UsbDevice,
            ctrlControlBlock: USBDataSource.UsbControlBlock,
            createNew: Boolean,
            index: Int
        ) {
            mUSBRepository.openCamera(ctrlControlBlock)
            mUSBRepository.setPreviewDisplay(mSurface)
            mUSBRepository.setPreviewSize(
                mWidth,
                mHeight,
                1, 30, 0, 1.0f
            )
            mUSBRepository.setFrameCallback(mIFrameCallback, mFormat)
            mUSBRepository.startPreview()
        }

        override fun onDisconnect(
            device: UsbDevice,
            ctrlControlBlock: USBDataSource.UsbControlBlock
        ) {
            TODO("Not yet implemented")
        }

        override fun onCancel(device: UsbDevice) = Unit
    }

    private val mIFrameCallback = object : IFrameCallback {
        override fun onFrame(byteBuffer: ByteBuffer) {
            val byteArray = byteBuffer.array()
            Log.i(sTAG, "byte size: ${byteArray.size}")
        }
    }

    private val mSurfaceHolderCallback = object : SurfaceHolder.Callback {
        override fun surfaceCreated(holder: SurfaceHolder) {
            Log.i(sTAG, "${object {}.javaClass.enclosingMethod?.name}")
            mSurface = holder.surface
        }

        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            Log.i(
                sTAG,
                "[${object {}.javaClass.enclosingMethod?.name}]::" +
                        " format: $format, width: $width height: $height"
            )
            mWidth = width
            mHeight = height
            mFormat = format
        }

        override fun surfaceDestroyed(holder: SurfaceHolder) {
            Log.i(sTAG, "${object {}.javaClass.enclosingMethod?.name}")
        }
    }

    companion object {
        private val sTAG = MainViewModel::class.java.name
        val Factory: MainViewModelFactory = MainViewModelFactory()
        private const val PIXEL_FORMAT_RGBX = 3
    }
}