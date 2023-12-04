package com.luxvisions.lvilibuvcproject.data.model.usb

import android.annotation.SuppressLint
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import android.os.Build
import android.os.Handler
import android.text.TextUtils
import android.util.Log
import android.util.SparseArray
import java.lang.ref.WeakReference
import java.nio.charset.StandardCharsets
import java.util.concurrent.ConcurrentHashMap
import kotlin.experimental.and

class USBDataSource {
    private val mCtrlBLocks: ConcurrentHashMap<UsbDevice, UsbControlBlock> = ConcurrentHashMap()
    private val mHasPermission: SparseArray<WeakReference<UsbDevice>> = SparseArray()
    private lateinit var mWeakContext: WeakReference<Context>
    private lateinit var mUsbManager: UsbManager
    private lateinit var mOnDeviceConnectListener: OnDeviceConnectListener
    private var mPermissionIntent: PendingIntent? = null
    private val mDeviceFilterList: List<DeviceFilter> = ArrayList()
    private val mUsbDevices = ArrayList<UsbDevice>()
    private val mAsyncHandler: Handler by lazy {
        HandlerThreadHandler.createHandler(sTAG)
    }
    private val mSecAsyncHandler: Handler by lazy {
        HandlerThreadHandler.createHandler("sec + $sTAG")
    }
    private var isDestroyed = false
    private var mDeviceCounts = 0

    fun init(context: Context, listener: OnDeviceConnectListener) {
        mWeakContext = WeakReference(context)
        mUsbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager
        mOnDeviceConnectListener = listener
    }

    fun destroy() {
        unregister()
        if (!isDestroyed) {
            isDestroyed = true
            val keys = mCtrlBLocks.keys()
            try {
                while (keys.hasMoreElements()) {
                    val ctrlBlock = mCtrlBLocks.remove(keys.nextElement())
                    ctrlBlock?.close()
                }
            } catch (e: Exception) {
                Log.e(sTAG, "destroy:", e)
            }
            mCtrlBLocks.clear()
            try {
                mAsyncHandler.looper.quit()
                mSecAsyncHandler.looper.quit()
            } catch (e: Exception) {
                Log.e(sTAG, "destroy:", e)
            }
        }
    }

    @Synchronized
    fun register() {
        if (isDestroyed) throw IllegalStateException("already destroyed")
        if (null == mPermissionIntent) {
            val context = mWeakContext.get()
            if (null != context) {
                val intent = Intent(ACTION_USB_PERMISSION)
                mPermissionIntent = PendingIntent.getBroadcast(
                    context,
                    0,
                    intent,
                    PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_IMMUTABLE
                )
                val filter = IntentFilter(ACTION_USB_PERMISSION)
                filter.addAction(UsbManager.ACTION_USB_ACCESSORY_DETACHED)
                context.registerReceiver(mUsbReceiver, filter)
            }
            mDeviceCounts = 0
            mAsyncHandler.postDelayed(mDeviceCheckRunnable, 1000)
        }
    }

    @Synchronized
    fun unregister() {
        mDeviceCounts = 0
        if (!isDestroyed)
            mAsyncHandler.removeCallbacks(mDeviceCheckRunnable)
        if (null != mPermissionIntent) {
            try {
                mWeakContext.get()?.unregisterReceiver(mUsbReceiver)
            } catch (e: Exception) {
                Log.w(sTAG, e.message.toString())
            }
            mPermissionIntent = null
        }
    }

    @Synchronized
    fun isRegistered(): Boolean {
        return !isDestroyed && mPermissionIntent != null
    }

    fun getDeviceList(filter: DeviceFilter): List<UsbDevice>? {
        if (isDestroyed) return null
        val deviceList = mUsbManager.deviceList
        val result = ArrayList<UsbDevice>()
        if (deviceList != null) {
            for (device in deviceList.values) {
                if (filter.matches(device) && !filter.bIsExclude) {
                    result.add(device)
                    mUsbDevices.add(device)
                }
            }
        }
        Log.i(sTAG, "Device count: ${result.size}")
        return result
    }

    @Synchronized
    fun requestPermission(device: UsbDevice): Boolean {
        var result = false
        if (isRegistered()) {
            if (mUsbManager.hasPermission(device)) {
//                processConnect(device)
                result = true
            } else {
                try {
                    mUsbManager.requestPermission(device, mPermissionIntent)
                } catch (e: Exception) {
                    Log.w(sTAG, e)
                    processCancel(device)
                    result = true
                }
            }
        } else {
            processCancel(device)
            result = true
        }

        return result
    }

    @Synchronized
    fun requestPermission(device: List<UsbDevice>): Boolean {
        var result = false
        if (isRegistered()) {
            var counter = 0
            for (i in device.indices) {
                if (mUsbManager.hasPermission(device[i]))
                    counter++
            }
            if (counter == device.size) {
//                processConnect(device)
                result = true
            } else {
                try {
                    for (i in device.indices)
                        mUsbManager.requestPermission(device[i], mPermissionIntent)
                } catch (e: Exception) {
                    Log.w(sTAG, e)
                    result = true
                }
            }
        } else {
            result = true
        }

        return result
    }

    fun openDevice(device: UsbDevice): UsbControlBlock {
        if (hasPermission(device)) {
            var result = mCtrlBLocks[device]
            if (null == result) {
                result = UsbControlBlock(this, device)
                mCtrlBLocks[device] = result
            }
            return result
        } else
            throw SecurityException("Has no permission")
    }

    private fun hasPermission(device: UsbDevice): Boolean {
        if (isDestroyed) return false
        return updatePermission(device, mUsbManager.hasPermission(device))
    }

    private fun updatePermission(device: UsbDevice, hasPermission: Boolean): Boolean {
        synchronized(mHasPermission) {
            if (hasPermission) {
                val deviceKey = getDeviceKey(device)
                if (null == mHasPermission.get(deviceKey)) {
                    val weakReference = WeakReference(device)
                    mHasPermission.put(deviceKey, weakReference)
                }
            } else
                mHasPermission.clear()
        }
        return hasPermission
    }

    private fun getDeviceKey(device: UsbDevice?): Int {
        return if (device != null) getDeviceKeyName(device, null, true).hashCode()
        else 0
    }

    fun getDeviceKey(device: UsbDevice?, serial: String, useNewAPI: Boolean): Int {
        return if (device != null) getDeviceKeyName(device, serial, useNewAPI).hashCode()
        else 0
    }

    private fun getDeviceList(): List<UsbDevice> {
        if (isDestroyed) throw IllegalStateException("already destroyed")
        return getDeviceList(mDeviceFilterList)
    }

    private fun getDeviceList(filters: List<DeviceFilter>): List<UsbDevice> {
        if (isDestroyed) throw IllegalStateException("already destroyed")
        val deviceList = mUsbManager.deviceList
        val result = ArrayList<UsbDevice>()
        if (null != deviceList) {
            if (filters.isEmpty()) {
                result.addAll(deviceList.values)
            } else {
                deviceList.values.forEach { device ->
                    for (i in filters.indices) {
                        if (filters[i].matches(device)) {
                            if (!filters[i].bIsExclude)
                                result.add(device)
                            break
                        }
                    }
                }
            }
        }
        return result
    }

    private fun processConnect(device: UsbDevice) {
        if (isDestroyed) return
        updatePermission(device, true)
        mAsyncHandler.post {
            var ctrlBlock = mCtrlBLocks[device]
            var isCreateNew = false
            if (null == ctrlBlock) {
                ctrlBlock = UsbControlBlock(this, device)
                Log.i(sTAG, "ctrl block fd: ${ctrlBlock.getFileDescriptor()}")
                mCtrlBLocks[device] = ctrlBlock
                isCreateNew = true
            }
            mOnDeviceConnectListener.onConnect(device, ctrlBlock, isCreateNew, 0)
        }
    }

    private fun setCtrlBlock(device: List<UsbDevice>, i: Int): Boolean {
        var isCreateNew = false
        var ctrlBlock = mCtrlBLocks[device[i]]
        if (null == ctrlBlock) {
            ctrlBlock = UsbControlBlock(this, device[i])
            Log.i(sTAG, "ctrl block fd: ${ctrlBlock.getFileDescriptor()}")
            mCtrlBLocks[device[i]] = ctrlBlock
            isCreateNew = true
        }
        return isCreateNew
    }

    private fun processConnect(device: List<UsbDevice>) {
        if (isDestroyed) return
        for (i in device.indices)
            updatePermission(device[i], true)
        mAsyncHandler.post {
            val i = 0
            val isCreateNew = setCtrlBlock(device, i)
            mOnDeviceConnectListener.onConnect(
                device[i],
                mCtrlBLocks[device[i]]!!,
                isCreateNew,
                i
            )
        }
        if (1 < device.size) {
            mSecAsyncHandler.post {
                val i = 1
                val isCreateNew = setCtrlBlock(device, i)
                mOnDeviceConnectListener.onConnect(
                    device[i],
                    mCtrlBLocks[device[i]]!!,
                    isCreateNew,
                    i
                )
            }
        }
    }

    private fun processCancel(device: UsbDevice) {
        if (isDestroyed) return
        updatePermission(device, false)
        mAsyncHandler.post {
            mOnDeviceConnectListener.onCancel(device)
        }
    }

    private fun processAttach(device: UsbDevice) {
        if (isDestroyed) return
        mAsyncHandler.post {
            mOnDeviceConnectListener.onAttach(device)
        }
    }

    private fun processDeAttach(device: UsbDevice) {
        if (isDestroyed) return
        mAsyncHandler.post {
            mOnDeviceConnectListener.onDeAttach(device)
        }
    }

    private val mDeviceCheckRunnable = object : Runnable {
        override fun run() {
            if (isDestroyed) return
            val devices = getDeviceList()
            val hasPermissionCounts = mHasPermission.size()
            synchronized(mHasPermission) {
                mHasPermission.clear()
                devices.forEach {
                    hasPermission(it)
                }
            }
            if (devices.size > mDeviceCounts || mHasPermission.size() > hasPermissionCounts) {
                mDeviceCounts = devices.size
                for (i in 0..<mDeviceCounts) {
                    val device = devices[i]
                    mAsyncHandler.post { mOnDeviceConnectListener.onAttach(device) }
                }
            }
            mAsyncHandler.postDelayed(this, 2000)
        }
    }

    private val mUsbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (isDestroyed) return
            val device = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                intent.getParcelableExtra(
                    UsbManager.EXTRA_DEVICE,
                    UsbDevice::class.java
                )
            else
                intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
            if (null != device) {
                when (intent.action) {
                    ACTION_USB_PERMISSION -> {
                        synchronized(this) {
                            if (
                                intent.getBooleanExtra(
                                    UsbManager.EXTRA_PERMISSION_GRANTED,
                                    false
                                )
                            ) {
//                                processConnect(device)
                            } else {
                                processCancel(device)
                            }
                        }
                    }

                    UsbManager.ACTION_USB_ACCESSORY_ATTACHED -> {
                        updatePermission(device, hasPermission(device))
                        processAttach(device)
                    }

                    UsbManager.ACTION_USB_ACCESSORY_DETACHED -> {
                        mCtrlBLocks.remove(device)?.close()
                        mDeviceCounts = 0
                        processDeAttach(device)
                    }

                    else -> {}
                }
            } else {
                // device is null
            }
        }
    }

    class UsbControlBlock : Cloneable {
        private val mWeakMonitor: WeakReference<USBDataSource>
        private val mWeakDevice: WeakReference<UsbDevice>
        private val mConnection: UsbDeviceConnection
        private val mInfo: UsbDeviceInfo
        private var mBusNum = 0
        private var mDevNum = 0
        private val mInterfaces = SparseArray<SparseArray<UsbInterface>>()

        constructor(monitor: USBDataSource, device: UsbDevice) {
            mWeakMonitor = WeakReference(monitor)
            mWeakDevice = WeakReference(device)
            mConnection = monitor.mUsbManager.openDevice(device)
            mInfo = updateDeviceInfo(monitor.mUsbManager, device)
            val name = device.deviceName
            val id = device.deviceId
            val v = if (!TextUtils.isEmpty(name))
                name.split("/")
            else null
            mBusNum = if (null != v)
                v[v.size - 2].toInt()
            else 0

            mDevNum = if (null != v)
                v[v.size - 1].toInt()
            else 0
            if (null != mConnection) {
                val desc = mConnection.fileDescriptor
                val rawDesc = mConnection.rawDescriptors
                Log.i(
                    sTAG,
                    "name = $name, Id = $id, desc = $desc, busnum = $mBusNum, devnum = $mDevNum, rawDesc = $rawDesc"
                )
            } else Log.e(sTAG, "could not connect to device $name")
        }

        constructor(src: UsbControlBlock) {
            val monitor = src.mWeakMonitor.get()
            val device = src.mWeakDevice.get()
            mConnection = monitor!!.mUsbManager.openDevice(device)
            mInfo = updateDeviceInfo(monitor.mUsbManager, device!!)
            mWeakMonitor = WeakReference(monitor)
            mWeakDevice = WeakReference(device)
            mBusNum = src.mBusNum
            mDevNum = src.mDevNum
        }

        override fun clone(): UsbControlBlock {
            return UsbControlBlock(this)
        }

        fun getUSBMonitor(): USBDataSource? {
            return mWeakMonitor.get()
        }

        fun getDevice(): UsbDevice? {
            return mWeakDevice.get()
        }

        fun getDeviceName(): String {
            return mWeakDevice.get()?.deviceName ?: ""
        }

        fun getDeviceProtocol(): Int {
            return mWeakDevice.get()?.deviceProtocol ?: 0
        }

        fun getDeviceKeyNameWithSerial(): String {
            return getDeviceKeyName(mWeakDevice.get(), mInfo.serial, false)
        }

        @Synchronized
        fun getFileDescriptor(): Int {
            return mConnection.fileDescriptor
        }

        fun getVendorId(): Int {
            return mWeakDevice.get()?.vendorId ?: 0
        }

        fun getProductId(): Int {
            return mWeakDevice.get()?.productId ?: 0
        }

        fun getVersion(): String {
            return mInfo.version
        }

        fun getBusNum(): Int {
            return mBusNum
        }

        fun getDevNum(): Int {
            return mDevNum
        }

        fun getInterfaceCount(): Int {
            return mWeakDevice.get()?.interfaceCount ?: 0
        }

        @Synchronized
        fun getInterface(interfaceId: Int): UsbInterface {
            var interfaceSparseArray = mInterfaces[interfaceId]
            if (interfaceSparseArray == null) {
                interfaceSparseArray = SparseArray()
                mInterfaces.put(interfaceId, interfaceSparseArray)
            }
            var usbInterface = interfaceSparseArray[0]
            if (usbInterface == null) {
                val device = mWeakDevice.get()
//                usbInterface = device!!.getInterface(interfaceId)
                for (i in 0..<device!!.interfaceCount) {
                    if (device.getInterface(i).id == interfaceId && device.getInterface(i).alternateSetting == 0) {
                        usbInterface = device.getInterface(i)
                        break
                    }
                }
                if (usbInterface != null)
                    interfaceSparseArray.append(0, usbInterface)
            }
            return usbInterface
        }

        @Synchronized
        private fun claimInterface(usbInterface: UsbInterface) {
            mConnection.claimInterface(usbInterface, true)
        }

        fun writeData(usbInterface: UsbInterface, endpoint: UsbEndpoint, byteArray: ByteArray) {
            claimInterface(usbInterface)
            val res = mConnection.bulkTransfer(endpoint, byteArray, byteArray.size, 0)
            if (res == -1)
                Log.i(sTAG, "Error happened while writing data. No ACK")
            releaseInterface(usbInterface)
        }

        @OptIn(ExperimentalStdlibApi::class)
        fun readData(endpoint: UsbEndpoint) {
            val size = 256 + 6
            val byteArray = ByteArray(size)
            val res = mConnection.bulkTransfer(endpoint, byteArray, size, 5000)
            if (res == 0)
                Log.i(sTAG, "waiting...")
            else if (res < 0)
                Log.i(sTAG, "Unable to read()")
            else {
                Log.i(sTAG, "Data size: $res")
            }
            for (i in 0..4)
                Log.i(sTAG, " ${byteArray[i].toHexString()}")
        }

        @Synchronized
        private fun releaseInterface(usbInterface: UsbInterface) {
            val interfaceSparseArray = mInterfaces[usbInterface.id]
            interfaceSparseArray?.removeAt(interfaceSparseArray.indexOfValue(usbInterface))
            if (interfaceSparseArray.size() == 0)
                mInterfaces.remove(usbInterface.id)
            mConnection.releaseInterface(usbInterface)
        }

        @Synchronized
        fun close() {
            for (i in 0..<mInterfaces.size()) {
                val interfaceSparseArray = mInterfaces.valueAt(i)
                if (interfaceSparseArray != null) {
                    for (j in 0..<interfaceSparseArray.size()) {
                        mConnection.releaseInterface(interfaceSparseArray.valueAt(j))
                    }
                    interfaceSparseArray.clear()
                }
            }
            mInterfaces.clear()
            mConnection.close()
            val monitor = mWeakMonitor.get()
            if (monitor != null) {
                monitor.mOnDeviceConnectListener.onDisconnect(mWeakDevice.get()!!, this)
                monitor.mCtrlBLocks.remove(getDevice())
            }
        }

        override fun equals(other: Any?): Boolean {
            return if (other == null) {
                false
            } else {
                if (other is UsbControlBlock) {
                    val device = other.getDevice()
                    if (device == null)
                        mWeakDevice.get() == null
                    else
                        device == mWeakDevice.get()
                } else if (other is UsbDevice) {
                    other == mWeakDevice.get()
                } else {
                    super.equals(other)
                }
            }
        }

        private fun updateDeviceInfo(manager: UsbManager, device: UsbDevice): UsbDeviceInfo {
            val info = UsbDeviceInfo()
            info.clear()
            if (BuildCheck.isLollipop()) {
                info.manufacturer = device.manufacturerName.toString()
                info.product = device.productName.toString()
                info.serial = device.serialNumber.toString()
            }
            if (BuildCheck.isMarshmallow())
                info.usbVersion = device.version
            if (manager.hasPermission(device)) {
                val connection = manager.openDevice(device)
                val desc = connection.rawDescriptors
                if (TextUtils.isEmpty(info.usbVersion))
                    info.usbVersion = String.format(
                        "%x.%02x",
                        (desc[3] and 0xff.toByte()).toInt(),
                        (desc[2] and 0xff.toByte()).toInt(),
                    )
                if (TextUtils.isEmpty(info.version))
                    info.version = String.format(
                        "%x.%02x",
                        (desc[13] and 0xff.toByte()).toInt(),
                        (desc[12] and 0xff.toByte()).toInt(),
                    )
                if (TextUtils.isEmpty(info.serial))
                    info.serial = connection.serial
                val languages = ByteArray(256)
                var languageCount = 0
                try {
                    val result = connection.controlTransfer(
                        USB_REQ_STANDARD_DEVICE_GET,
                        USB_REQ_GET_DESCRIPTOR,
                        USB_DT_STRING shl 8,
                        0,
                        languages,
                        languages.size,
                        0
                    )
                    if (0 < result)
                        languageCount = (result - 2) / 2
                    if (0 < languageCount) {
                        if (TextUtils.isEmpty(info.manufacturer))
                            info.manufacturer = getString(
                                connection,
                                desc[14].toInt(),
                                languageCount,
                                languages
                            )
                        if (TextUtils.isEmpty(info.product))
                            info.product = getString(
                                connection,
                                desc[15].toInt(),
                                languageCount,
                                languages
                            )
                        if (TextUtils.isEmpty(info.serial))
                            info.serial = getString(
                                connection,
                                desc[16].toInt(),
                                languageCount,
                                languages
                            )
                    }
                } finally {
                    connection.close()
                }
            }
            if (TextUtils.isEmpty(info.manufacturer))
                info.manufacturer = USBVendorId.vendorName(device.vendorId)
            if (TextUtils.isEmpty(info.manufacturer))
                info.manufacturer = String.format("%04x", device.vendorId)
            if (TextUtils.isEmpty(info.product))
                info.product = String.format("%04x", device.productId)
            return info
        }

        private fun getString(
            connection: UsbDeviceConnection,
            id: Int,
            languageCount: Int,
            languages: ByteArray
        ): String {
            val byteArray = ByteArray(256)
            var result = ""
            for (i in 1..languageCount) {
                val ret = connection.controlTransfer(
                    USB_REQ_STANDARD_DEVICE_GET,
                    USB_REQ_GET_DESCRIPTOR,
                    (USB_DT_STRING shl 8) or id,
                    languages[i].toInt(),
                    byteArray,
                    byteArray.size,
                    0
                )
                if ((ret > 2) && (byteArray[0].toInt() == ret) && (byteArray[1].toInt() == USB_DT_STRING)) {
                    result = String(byteArray, 2, ret - 2, StandardCharsets.UTF_16LE)
                    if ("Љ" != result)
                        break
                    else
                        result = ""
                }
            }
            return result
        }
    }

    class UsbDeviceInfo {
        var usbVersion = ""
        var manufacturer = ""
        var product = ""
        var version = ""
        var serial = ""

        fun clear() {
            usbVersion = ""
            manufacturer = ""
            product = ""
            version = ""
            serial = ""
        }

        override fun toString(): String {
            return String.format(
                "UsbDevice: usb_version = $usbVersion, manufacturer = $manufacturer," +
                        " product = $product, version = $version, serial = $serial"
            )
        }
    }

    companion object {
        private val sTAG = USBDataSource::class.java.name
        private const val USB_DIR_IN = 0x80
        private const val USB_TYPE_STANDARD = (0)
        private const val USB_RECIP_DEVICE = 0x00
        private const val USB_REQ_GET_DESCRIPTOR = 0x06
        private const val USB_REQ_STANDARD_DEVICE_GET =
            USB_DIR_IN or USB_TYPE_STANDARD or USB_RECIP_DEVICE
        private const val USB_DT_STRING = 0x03
        private val ACTION_USB_PERMISSION =
            "com.luxvisions.lvilibuvcproject.USB_PERMISSION." + hashCode()

        @SuppressLint("NewApi")
        fun getDeviceKeyName(device: UsbDevice?, serial: String?, useNewAPI: Boolean): String {
            if (device == null) return ""
            val stringBuilder = StringBuilder()
            stringBuilder.append(device.vendorId)
            stringBuilder.append("#") // API >= 12
            stringBuilder.append(device.productId)
            stringBuilder.append("#") // API >= 12
            stringBuilder.append(device.deviceClass)
            stringBuilder.append("#") // API >= 12
            stringBuilder.append(device.deviceSubclass)
            stringBuilder.append("#") // API >= 12
            stringBuilder.append(device.deviceProtocol)
            if (!TextUtils.isEmpty(serial)) {
                stringBuilder.append("#")
                stringBuilder.append(serial)
            }
            if (useNewAPI && BuildCheck.isLollipop()) {
                stringBuilder.append("#")
                if (!TextUtils.isEmpty(serial)) {
                    stringBuilder.append(device.serialNumber)
                    stringBuilder.append("#")
                }
                stringBuilder.append(device.manufacturerName)
                stringBuilder.append("#")
                stringBuilder.append(device.configurationCount)
                stringBuilder.append("#")
                if (BuildCheck.isMarshmallow()) {
                    stringBuilder.append(device.version)
                    stringBuilder.append("#")
                }
            }
            return stringBuilder.toString()
        }
    }
}