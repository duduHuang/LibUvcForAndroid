package com.luxvisions.lvilibuvcproject.data.model

import android.content.Context
import android.view.SurfaceView
import androidx.fragment.app.FragmentActivity
import com.luxvisions.lvilibuvcproject.base.UiEvent
import com.luxvisions.lvilibuvcproject.base.UiState

class MainContract {

    sealed class Event : UiEvent {
        data class InitUSBData(
            val context: Context,
            val activity: FragmentActivity,
        ) : Event()
        data class StartCamera(
            val context: Context,
            val activity: FragmentActivity,
            val surfaceView: SurfaceView,
            val filerIndex: Int
        ) : Event()
        data object Release : Event()
        data class HidData(
            val context: Context,
            val activity: FragmentActivity,
        ) : Event()
    }

    data class State(
        val usbState: UsbState
    ) : UiState

    sealed class UsbState {
        data object Initial: UsbState()
    }
}