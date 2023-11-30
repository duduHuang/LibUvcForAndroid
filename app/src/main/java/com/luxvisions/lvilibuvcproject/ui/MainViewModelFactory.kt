package com.luxvisions.lvilibuvcproject.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider

class MainViewModelFactory: ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(MainViewModel::class.java))
            return MainViewModel() as T
        throw java.lang.IllegalArgumentException("Unknown ViewModel class")
    }
}