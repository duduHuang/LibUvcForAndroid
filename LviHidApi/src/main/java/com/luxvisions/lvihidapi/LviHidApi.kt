package com.luxvisions.lvihidapi

import android.graphics.Bitmap
import android.util.Log
import java.io.FileNotFoundException
import java.io.FileOutputStream

class LviHidApi {
    private var priFd = 0
    private var secFd = 0
    fun initHidApi(): Int {
        return nativeHidApiInit()
    }

    fun exitHidApi(): Int {
        return nativeHidApiExit()
    }

    fun startHidApi(
        vendorId: Int,
        productId: Int,
        fileDescriptor: Int,
        requestHidData: Boolean
    ): ByteArray {
        if (0 == priFd)
            priFd = fileDescriptor
        else if (0 == secFd && fileDescriptor != priFd)
            secFd = fileDescriptor
        val primary = nativeHidApiStart(vendorId, productId, fileDescriptor, requestHidData)
        Log.i(sTAG, "$fileDescriptor size = ${primary.size}")
//        val counter = primary[1]
//        Log.i(sTAG, "$fileDescriptor data[0] = ${primary[0]}, data[1] = ${primary[1]}")
//        val byteArray = primary.copyOfRange(1, primary.size)
//        writeFile(byteArray, "${fileDescriptor}_${counter}")
        return primary
    }

    fun stopHidApi(
        vendorId: Int,
        productId: Int,
        fileDescriptor: Int,
        secFileDescriptor: Int
    ) {
        var ret = nativeHidApiStop(
            vendorId, productId, fileDescriptor
        )
        Log.i(sTAG, "Stop $fileDescriptor ret = $ret")
        ret = nativeHidApiStop(
            vendorId, productId, secFileDescriptor
        )
        Log.i(sTAG, "Stop $secFileDescriptor ret = $ret")
    }

    fun autoFraming(byteArray: ByteArray, fileDescriptor: Int): ByteArray {
        return nativeHidApiAutoFraming(byteArray, fileDescriptor)
    }

    fun writeFile(byteArray: ByteArray, name: String) {
        try {
            val filename = "/data/data/com.luxvisions.lvilibuvceoproject/$name.bin"
            val outputStreamWriter = FileOutputStream(filename)
            outputStreamWriter.write(byteArray)
            outputStreamWriter.close()
        } catch (e: FileNotFoundException) {
            Log.i(sTAG, "write file failed: ${e.printStackTrace()}")
        }
    }

    fun writeImage(bitmap: Bitmap, name: String) {
        try {
            val filename = "/data/data/com.luxvisions.lvilibuvceoproject/$name.jpg"
            val outputStreamWriter = FileOutputStream(filename)
            bitmap.compress(Bitmap.CompressFormat.JPEG, 100, outputStreamWriter)
            outputStreamWriter.flush()
            outputStreamWriter.close()
        } catch (e: FileNotFoundException) {
            Log.i(sTAG, "write image failed: ${e.printStackTrace()}")
        }
    }

    /**
     * A native method that is implemented by the 'lvihidapi' native library,
     * which is packaged with this application.
     */
    private external fun nativeHidApiInit(): Int

    private external fun nativeHidApiExit(): Int

    private external fun nativeHidApiStart(
        vendorId: Int,
        productId: Int,
        fileDescriptor: Int,
        requestHidData: Boolean
    ): ByteArray

    private external fun nativeHidApiStop(
        vendorId: Int,
        productId: Int,
        fileDescriptor: Int
    ): Int

    private external fun nativeHidApiAutoFraming(
        byteArray: ByteArray,
        fileDescriptor: Int
    ) : ByteArray

    companion object {
        private val sTAG = LviHidApi::class.java.name
        // Used to load the 'lvihidapi' library on application startup.
        init {
            System.loadLibrary("lvihidapi")
        }
    }
}