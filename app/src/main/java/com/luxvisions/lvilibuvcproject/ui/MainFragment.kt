package com.luxvisions.lvilibuvcproject.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.SurfaceHolder
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import com.luxvisions.lvilibuvcproject.R
import com.luxvisions.lvilibuvcproject.data.model.MainContract
import com.luxvisions.lvilibuvcproject.databinding.FragmentMainBinding
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch

class MainFragment : Fragment() {
    private val mMainViewModel: MainViewModel by activityViewModels { MainViewModel.Factory }
    private var _mFragmentMainBinding: FragmentMainBinding? = null
    private val mFragmentMainBinding get() = _mFragmentMainBinding!!
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _mFragmentMainBinding = FragmentMainBinding.inflate(inflater, container, false)
        return mFragmentMainBinding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        buttonEvent()
        initObservers()
        mFragmentMainBinding.cameraSurfaceView.holder.addCallback(mMainViewModel.getSurfaceHolderCallback())
    }

    override fun onStop() {
        mFragmentMainBinding.cameraSurfaceView.holder.removeCallback(mMainViewModel.getSurfaceHolderCallback())
        super.onStop()
    }

    override fun onDestroyView() {
        _mFragmentMainBinding = null
        super.onDestroyView()
    }

    private fun buttonEvent() {
        mFragmentMainBinding.usbCameraButton.setOnClickListener {
            mMainViewModel.setEvent(MainContract.Event.StartCamera(
                requireContext(),
                requireActivity(),
                mFragmentMainBinding.cameraSurfaceView,
                0
            ))
        }
        mFragmentMainBinding.usbHidButton.setOnClickListener {
            mMainViewModel.setEvent(MainContract.Event.HidData(
                requireContext(),
                requireActivity()
            ))
        }
    }

    private fun initObservers() {
        lifecycleScope.launch {
            mMainViewModel.uiState.collect {
                when (it.usbState) {
                    is MainContract.UsbState.Initial -> {
                        mMainViewModel.setEvent(MainContract.Event.InitUSBData(
                            requireContext(),
                            requireActivity()
                        ))
                        mFragmentMainBinding.fpsTextView.text = getString(R.string.cost_time, 0)
                    }

                    else -> {}
                }
            }
        }
    }

    companion object {
        fun newInstance() = MainFragment()
    }
}