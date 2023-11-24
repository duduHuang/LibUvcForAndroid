package com.luxvisions.libuvccamera

class LibUvcCamera {

    fun init() {
        nativeInit()
    }

    fun release() {
        nativeRelease()
    }

    /**
     * A native method that is implemented by the 'libuvccamera' native library,
     * which is packaged with this application.
     */
    private external fun nativeInit(): Int
    private external fun nativeRelease(): Int

    companion object {
        // Used to load the 'libuvccamera' library on application startup.
        init {
            System.loadLibrary("libuvccamera")
        }
    }
}