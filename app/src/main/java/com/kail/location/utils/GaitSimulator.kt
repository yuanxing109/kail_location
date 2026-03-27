package com.kail.location.utils

import android.util.Log

/**
 * 步态模拟器JNI接口
 * 提供与C++步态模拟库的接口
 */
object GaitSimulator {

    private const val TAG = "GaitSimulator"
    private var isInitialized = false

    /**
     * 初始化步态模拟器
     * @param configPath 配置文件路径
     * @return 0表示成功，-1表示失败
     */
    fun init(configPath: String): Int {
        return try {
            KailLog.i(null, TAG, "开始初始化 GaitSimulator，配置文件: $configPath")
            val result = nativeInit(configPath)
            if (result == 0) {
                isInitialized = true
                KailLog.i(null, TAG, "GaitSimulator 初始化成功")
            } else {
                KailLog.e(null, TAG, "GaitSimulator 初始化失败，返回码: $result")
            }
            result
        } catch (e: Exception) {
            KailLog.e(null, TAG, "初始化 GaitSimulator 时发生异常: ${e.message}")
            KailLog.e(null, TAG, "异常详情: ${e.stackTraceToString()}")
            -1
        }
    }

    /**
     * 更新步态参数
     * @param stepsPerMinute 步频（步/分钟）
     * @param mode 模式（0=Walk, 1=Run, 2=FastRun）
     * @param enable 是否启用
     */
    fun updateParams(stepsPerMinute: Float, mode: Int, enable: Boolean) {
        if (!isInitialized) {
            KailLog.w(null, TAG, "尝试更新参数但模拟器未初始化")
            return
        }
        try {
            KailLog.d(null, TAG, "更新参数: spm=$stepsPerMinute, mode=$mode, enable=$enable")
            nativeUpdateParams(stepsPerMinute, mode, enable)
        } catch (e: Exception) {
            KailLog.e(null, TAG, "更新参数时发生异常: ${e.message}")
        }
    }

    /**
     * 处理传感器事件
     * @param timestampNs 时间戳（纳秒）
     * @param data 传感器数据数组
     * @param type 传感器类型
     */
    fun processEvent(timestampNs: Long, data: FloatArray, type: Int) {
        if (!isInitialized) {
            KailLog.w(null, TAG, "尝试处理事件但模拟器未初始化")
            return
        }
        try {
            // 高频日志，只输出到控制台，不保存到文件
            KailLog.d(null, TAG, "处理传感器事件: type=$type, data.size=${data.size}", isHighFrequency = true)
            nativeProcessEvents(timestampNs, data, type)
        } catch (e: Exception) {
            KailLog.e(null, TAG, "处理传感器事件时发生异常: ${e.message}")
        }
    }

    /**
     * 重新加载配置
     * @param nowNs 当前时间（纳秒）
     * @return true表示配置已更新
     */
    fun reloadConfig(nowNs: Long): Boolean {
        if (!isInitialized) {
            KailLog.w(null, TAG, "尝试重新加载配置但模拟器未初始化")
            return false
        }
        return try {
            KailLog.d(null, TAG, "重新加载配置文件")
            val reloaded = nativeReloadConfig(nowNs)
            if (reloaded) {
                KailLog.i(null, TAG, "配置文件已重新加载")
            } else {
                KailLog.d(null, TAG, "配置文件无需更新")
            }
            reloaded
        } catch (e: Exception) {
            KailLog.e(null, TAG, "重新加载配置时发生异常: ${e.message}")
            false
        }
    }

    /**
     * 销毁模拟器
     */
    fun destroy() {
        if (!isInitialized) {
            KailLog.w(null, TAG, "尝试销毁但模拟器未初始化")
            return
        }
        try {
            KailLog.i(null, TAG, "销毁 GaitSimulator")
            nativeDestroy()
            isInitialized = false
            KailLog.i(null, TAG, "GaitSimulator 已销毁")
        } catch (e: Exception) {
            KailLog.e(null, TAG, "销毁 GaitSimulator 时发生异常: ${e.message}")
        }
    }

    // Native methods
    private external fun nativeInit(configPath: String): Int
    private external fun nativeUpdateParams(stepsPerMinute: Float, mode: Int, enable: Boolean)
    private external fun nativeProcessEvents(timestampNs: Long, data: FloatArray, type: Int)
    private external fun nativeReloadConfig(nowNs: Long): Boolean
    private external fun nativeDestroy()
}