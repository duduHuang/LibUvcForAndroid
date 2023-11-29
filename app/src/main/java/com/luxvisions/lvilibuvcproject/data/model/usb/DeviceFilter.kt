package com.luxvisions.lvilibuvcproject.data.model.usb

import android.content.Context
import android.content.res.Resources.NotFoundException
import android.hardware.usb.UsbDevice
import android.text.TextUtils
import android.util.Log
import org.xmlpull.v1.XmlPullParser
import org.xmlpull.v1.XmlPullParserException
import java.io.IOException
import java.util.Collections

class DeviceFilter(
    vid: Int, pid: Int, clazz: Int, subClass: Int,
    protocol: Int, manufacturer: String?, product: String?,
    serialNum: String?, isExclude: Boolean
) {
    // USB Vendor ID (or -1 for unspecified)
    val mVendorId: Int

    // USB Product ID (or -1 for unspecified)
    val mProductId: Int

    // USB device or interface class (or -1 for unspecified)
    val mClass: Int

    // USB device subclass (or -1 for unspecified)
    val mSubclass: Int

    // USB device protocol (or -1 for unspecified)
    val mProtocol: Int

    // USB device manufacturer name string (or null for unspecified)
    val mManufacturerName: String?

    // USB device product name string (or null for unspecified)
    val mProductName: String?

    // USB device serial number string (or null for unspecified)
    val mSerialNumber: String?

    // set true if specific device(s) should exclude
    val bIsExclude: Boolean

    constructor(device: UsbDevice, isExclude: Boolean) : this(
        device.vendorId, device.productId,
        device.deviceClass, device.deviceSubclass,
        device.deviceProtocol, null,
        null, null,
        isExclude
    )

    init {
        mVendorId = vid
        mProductId = pid
        mClass = clazz
        mSubclass = subClass
        mProtocol = protocol
        mManufacturerName = manufacturer
        mProductName = product
        mSerialNumber = serialNum
        bIsExclude = isExclude
    }

    fun getDeviceFilters(context: Context, deviceFilterXmlId: Int): List<DeviceFilter> {
        val parser = context.resources.getXml(deviceFilterXmlId)
        val deviceFilters = ArrayList<DeviceFilter>()

        try {
            var eventType = parser.eventType
            while (eventType != XmlPullParser.END_DOCUMENT) {
                if (eventType == XmlPullParser.START_TAG) {
                    val deviceFilter = readEntryOne(context, parser)
                    deviceFilter?.let {
                        deviceFilters.add(it)
                    }
                }
                eventType = parser.next()
            }
        } catch (e: XmlPullParserException) {
            Log.d(TAG, "XmlPullParserException", e)
        } catch (e: IOException) {
            Log.d(TAG, "IOException", e)
        }

        return Collections.unmodifiableList(deviceFilters)
    }

    private fun readEntryOne(context: Context, parser: XmlPullParser): DeviceFilter? {
        var vendorId = -1
        var productId = -1
        var deviceClass = -1
        var deviceSubclass = -1
        var deviceProtocol = -1
        var exclude = false
        var manufacturerName: String? = null
        var productName: String? = null
        var serialNumber: String? = null
        var hasValue = false
        var eventType = parser.eventType
        while (eventType != XmlPullParser.END_DOCUMENT) {
            val str = parser.name
            if (!TextUtils.isEmpty(str) && str.equals("usb-device", true)) {
                if (eventType == XmlPullParser.START_TAG) {
                    hasValue = true
                    vendorId = getAttributeInteger(context, parser, "vendor-id")
                    if (-1 == vendorId) {
                        vendorId = getAttributeInteger(context, parser, "vendorId")
                        if (-1 == vendorId)
                            vendorId = getAttributeInteger(context, parser, "venderId")
                    }
                    productId = getAttributeInteger(context, parser, "product-id")
                    if (-1 == productId)
                        productId = getAttributeInteger(context, parser, "productId")
                    deviceClass = getAttributeInteger(context, parser, "class")
                    deviceSubclass = getAttributeInteger(context, parser, "subclass")
                    deviceProtocol = getAttributeInteger(context, parser, "protocol")
                    manufacturerName = getAttributeString(context, parser, "manufacturer-name")
                    if (TextUtils.isEmpty(manufacturerName))
                        manufacturerName = getAttributeString(context, parser, "manufacture")
                    productName = getAttributeString(context, parser, "product-name")
                    if (TextUtils.isEmpty(productName))
                        productName = getAttributeString(context, parser, "product")
                    serialNumber = getAttributeString(context, parser, "serial-number")
                    if (TextUtils.isEmpty(serialNumber))
                        serialNumber = getAttributeString(context, parser, "serial")
                    exclude = getAttributeBoolean(context, parser, "exclude")
                } else if (eventType == XmlPullParser.END_TAG) {
                    if (hasValue)
                        return DeviceFilter(
                            vendorId, productId, deviceClass,
                            deviceSubclass, deviceProtocol,
                            manufacturerName, productName,
                            serialNumber, exclude
                        )
                }
            }
            eventType = parser.next()
        }
        return null
    }

    private fun getAttributeInteger(
        context: Context,
        parser: XmlPullParser,
        name: String?,
        namespace: String? = null,
        defaultValue: Int = -1
    ): Int {
        var result = defaultValue
        try {
            var str = parser.getAttributeValue(namespace, name)
            if (!TextUtils.isEmpty(str) && str.startsWith("@")) {
                val subStr = str.substring(1)
                val resId = context.resources.getIdentifier(subStr, null, context.packageName)
                if (resId > 0)
                    result = context.resources.getInteger(resId)
            } else {
                var radix = 10
                if (str != null && str.length > 2 && str.startsWith("0x", true)) {
                    radix = 16
                    str = str.substring(2)
                }
                result = Integer.parseInt(str, radix)
            }
        } catch (e: NotFoundException) {
            Log.d(TAG, "NotFoundException", e)
        } catch (e: NumberFormatException) {
            Log.d(TAG, "NumberFormatException", e)
        } catch (e: NullPointerException) {
            Log.d(TAG, "NullPointerException", e)
        }
        return result
    }

    private fun getAttributeString(
        context: Context,
        parser: XmlPullParser,
        name: String?,
        namespace: String? = null,
        defaultValue: String? = null
    ): String? {
        var result = defaultValue
        try {
            result = parser.getAttributeValue(namespace, name)
            result?.let {
                if (!TextUtils.isEmpty(it) && it.startsWith("@")) {
                    val str = it.substring(1)
                    val resId = context.resources.getIdentifier(str, null, context.packageName)
                    if (resId > 0)
                        result = context.resources.getString(resId)
                }
            } ?: run {
                result = defaultValue
            }
        } catch (e: NotFoundException) {
            Log.d(TAG, "NotFoundException", e)
        } catch (e: NumberFormatException) {
            Log.d(TAG, "NumberFormatException", e)
        } catch (e: NullPointerException) {
            Log.d(TAG, "NullPointerException", e)
        }
        return result
    }

    private fun getAttributeBoolean(
        context: Context,
        parser: XmlPullParser,
        name: String?,
        namespace: String? = null,
        defaultValue: Boolean = false
    ): Boolean {
        try {
            var str = parser.getAttributeValue(namespace, name)
            if ("TRUE".equals(str, true)) {
                return true
            } else if ("FALSE".equals(str, true)) {
                return false
            } else if (!TextUtils.isEmpty(str) && str.startsWith("@")) {
                val subStr = str.substring(1)
                val resId = context.resources.getIdentifier(subStr, null, context.packageName)
                if (resId > 0)
                    return context.resources.getBoolean(resId)
            } else {
                var radix = 10
                if (str != null && str.length > 2 && str.startsWith("0x", true)) {
                    radix = 16
                    str = str.substring(2)
                }
                val value = Integer.parseInt(str, radix)
                return value != 0
            }
        } catch (e: NotFoundException) {
            Log.d(TAG, "NotFoundException", e)
        } catch (e: NumberFormatException) {
            Log.d(TAG, "NumberFormatException", e)
        } catch (e: NullPointerException) {
            Log.d(TAG, "NullPointerException", e)
        }
        return defaultValue
    }

    private fun matches(clazz: Int, subClass: Int, protocol: Int): Boolean {
        return ((mClass == -1 || clazz == mClass) &&
                (mSubclass == -1 || subClass == mSubclass) &&
                (mProtocol == -1 || protocol == mProtocol))
    }

    fun matches(device: UsbDevice): Boolean {
        if (mVendorId != -1 && device.vendorId != mVendorId)
            return false
        if (mProductId != -1 && device.productId != mProductId)
            return false
        if (matches(device.deviceClass, device.deviceSubclass, device.deviceProtocol))
            return true
        val counter = device.interfaceCount
        for (i in 0 until counter) {
            val usbInterface = device.getInterface(i)
            if (
                matches(
                    usbInterface.interfaceClass,
                    usbInterface.interfaceSubclass,
                    usbInterface.interfaceProtocol
                )
            )
                return true
        }
        return false
    }

    override fun equals(other: Any?): Boolean {
        if (mVendorId == -1 || mProductId == -1 ||
            mClass == -1 || mSubclass == -1 || mProtocol == -1
        )
            return false
        if (other is DeviceFilter) {
            if (other.mVendorId != mVendorId ||
                other.mProductId != mProductId ||
                other.mClass != mClass ||
                other.mSubclass != mSubclass ||
                other.mProtocol != mProtocol
            )
                return false
            if (other.mManufacturerName != null &&
                mManufacturerName == null || other.mManufacturerName == null &&
                mManufacturerName != null || other.mProductName != null &&
                mProductName == null || other.mProductName == null &&
                mProductName != null || other.mSerialNumber != null &&
                mSerialNumber == null || other.mSerialNumber == null &&
                mSerialNumber != null
            )
                return false
            if (other.mManufacturerName != null &&
                mManufacturerName != null &&
                mManufacturerName != other.mManufacturerName || other.mProductName != null &&
                mProductName != null &&
                mProductName != other.mProductName || other.mSerialNumber != null &&
                mSerialNumber != null &&
                mSerialNumber != other.mSerialNumber
            )
                return false
            return (other.bIsExclude != bIsExclude)
        }
        if (other is UsbDevice) {
            if (bIsExclude ||
                (other.vendorId != mVendorId) ||
                (other.productId != mProductId) ||
                (other.deviceClass != mClass) ||
                (other.deviceSubclass != mSubclass) ||
                (other.deviceProtocol != mProtocol)
            )
                return false
            return true
        }
        return false
    }

    override fun hashCode(): Int {
        return ((mVendorId shl 16 or mProductId) xor (mClass shl 16
                or (mSubclass shl 8) or mProtocol))
    }

    override fun toString(): String {
        return "DeviceFilter[mVendorId=$mVendorId,mProductId=$mProductId," +
                "mClass=$mClass,mSubclass=$mSubclass,mProtocol=$mProtocol," +
                "mManufacturerName=$mManufacturerName,mProductName=$mProductName," +
                "mSerialNumber=$mSerialNumber,isExclude=$bIsExclude]"
    }

    companion object {
        private val TAG = DeviceFilter::class.java.name
    }
}