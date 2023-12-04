package com.luxvisions.lvilibuvcproject.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import com.luxvisions.lvilibuvcproject.data.model.USBRepository
import com.luxvisions.lvilibuvcproject.data.model.usb.USBCameraDataSource
import com.luxvisions.lvilibuvcproject.data.model.usb.USBDataSource

class MainViewModelFactory: ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(MainViewModel::class.java))
            return MainViewModel(
                mUSBRepository = USBRepository(
                    usbDataSource = USBDataSource(),
                    usbCameraDataSource = USBCameraDataSource()
                )
            ) as T
        throw java.lang.IllegalArgumentException("Unknown ViewModel class")
    }
}