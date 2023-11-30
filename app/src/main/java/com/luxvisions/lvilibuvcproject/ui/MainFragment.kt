package com.luxvisions.lvilibuvcproject.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.luxvisions.lvilibuvcproject.databinding.FragmentMainBinding

class MainFragment : Fragment() {
    private var _mFragmentMainBinding: FragmentMainBinding? = null
    private val mFragmentMainBinding get() = _mFragmentMainBinding!!
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        _mFragmentMainBinding = FragmentMainBinding.inflate(inflater, container, false)
        return mFragmentMainBinding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
    }

    override fun onStart() {
        super.onStart()
    }

    override fun onStop() {
        super.onStop()
    }

    override fun onDestroyView() {
        _mFragmentMainBinding = null
        super.onDestroyView()
    }

    companion object {
        fun newInstance() = MainFragment()
    }
}