package com.luxvisions.lvilibuvcproject.data.model.usb

import android.os.Handler
import android.os.HandlerThread
import android.os.Looper

class HandlerThreadHandler constructor(looper: Looper) : Handler(looper) {
    constructor(looper: Looper, callback: Callback) : this(looper) {
        Handler(looper, callback)
    }

    companion object {
        private val TAG = HandlerThreadHandler::class.java.name
        fun createHandler(): HandlerThreadHandler {
            return createHandler("HandlerThreadHandler")
        }

        fun createHandler(callback: Callback): HandlerThreadHandler {
            return createHandler("HandlerThreadHandler", callback)
        }

        fun createHandler(name: String): HandlerThreadHandler {
            val threadHandler = HandlerThread(name)
            threadHandler.start()
            return HandlerThreadHandler(threadHandler.looper)
        }

        fun createHandler(name: String, callback: Callback): HandlerThreadHandler {
            val threadHandler = HandlerThread(name)
            threadHandler.start()
            return HandlerThreadHandler(threadHandler.looper, callback)
        }
    }
}