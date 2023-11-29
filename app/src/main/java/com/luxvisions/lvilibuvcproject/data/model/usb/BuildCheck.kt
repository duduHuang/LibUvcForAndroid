package com.luxvisions.lvilibuvcproject.data.model.usb

import android.os.Build

class BuildCheck {
    companion object {
        private fun check(value: Int): Boolean {
            return Build.VERSION.SDK_INT >= value
        }

        fun isLollipop(): Boolean {
            return check(21)
        }

        fun isMarshmallow(): Boolean {
            return check(23)
        }
    }
}