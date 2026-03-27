package com.kail.location.utils

import android.app.Application
import com.baidu.location.LocationClient
import com.baidu.mapapi.CoordType
import com.baidu.mapapi.SDKInitializer
import com.kail.location.utils.KailLog
import androidx.preference.PreferenceManager

/**
 * 自定义 Application，负责应用级初始化。
 * 包含崩溃日志写入以及百度地图/定位 SDK 的设置。
 */
class GoApplication : Application() {
    companion object {
        const val APP_NAME = "KailLocation"
        private const val KEY_BAIDU_MAP_KEY = "setting_baidu_map_key"
    }

    /**
     * 将崩溃信息写入文件。
     *
     * @param ex 抛出的异常对象。
     */
    private fun writeCrashToFile(ex: Throwable) {
        try {
            val logPath = getExternalFilesDir("Logs") ?: return
            val crashFile = java.io.File(logPath, "crash_${System.currentTimeMillis()}.txt")
            val pw = java.io.PrintWriter(crashFile)
            ex.printStackTrace(pw)
            pw.flush()
            pw.close()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private var mDefaultHandler: Thread.UncaughtExceptionHandler? = null

    /**
     * 应用启动回调。
     * 初始化崩溃处理、XLog 以及百度地图/定位 SDK。
     */
    override fun onCreate() {
        super.onCreate()

        // 先检查日志开关状态并强制输出
        val prefs = PreferenceManager.getDefaultSharedPreferences(this)
        val logEnabled = prefs.getBoolean("setting_log_enabled", false)
        android.util.Log.d("GoApplication", "日志开关状态: $logEnabled")

        kotlin.runCatching {
            android.util.Log.d("GoApplication", "开始加载 kail_gait_sim 库")
            KailLog.i(this, APP_NAME, "开始加载 kail_gait_sim 库")
            System.loadLibrary("kail_gait_sim")
            android.util.Log.d("GoApplication", "kail_gait_sim 库加载成功")
            KailLog.i(this, APP_NAME, "kail_gait_sim 库加载成功")

            // Initialize GaitSimulator with config path
            val configDir = getExternalFilesDir("config")
            if (configDir != null && !configDir.exists()) {
                configDir.mkdirs()
                android.util.Log.d("GoApplication", "创建配置目录: ${configDir.absolutePath}")
                KailLog.d(this, APP_NAME, "创建配置目录: ${configDir.absolutePath}")
            }

            val configPath = configDir?.absolutePath + "/gait_config.txt"
            android.util.Log.d("GoApplication", "准备初始化 GaitSimulator，配置文件路径: $configPath")
            KailLog.d(this, APP_NAME, "准备初始化 GaitSimulator，配置文件路径: $configPath")

            val initResult = GaitSimulator.init(configPath)
            if (initResult == 0) {
                android.util.Log.d("GoApplication", "GaitSimulator 初始化成功")
                KailLog.i(this, APP_NAME, "GaitSimulator 初始化成功")
            } else {
                android.util.Log.e("GoApplication", "GaitSimulator 初始化失败，返回码: $initResult")
                KailLog.e(this, APP_NAME, "GaitSimulator 初始化失败，返回码: $initResult")
            }
        }.onFailure {
            android.util.Log.e("GoApplication", "加载 kail_gait_sim 失败: ${it.message}")
            android.util.Log.e("GoApplication", "异常详情: ${it.stackTraceToString()}")
            KailLog.e(this, APP_NAME, "加载 kail_gait_sim 失败: ${it.message}")
            KailLog.e(this, APP_NAME, "异常详情: ${it.stackTraceToString()}")
        }

        mDefaultHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            writeCrashToFile(throwable)
            throwable.printStackTrace()
            mDefaultHandler?.uncaughtException(thread, throwable)
        }

        // 百度地图 7.5 开始，要求必须同意隐私政策，默认为false
        SDKInitializer.setAgreePrivacy(this, true)
        // 百度定位 7.5 开始，要求必须同意隐私政策，默认为false(官方说可以统一为以上接口，但实际测试并不行，定位还是需要单独设置)
        LocationClient.setAgreePrivacy(true)

        try {
            // 读取自定义 Key
            val prefs = PreferenceManager.getDefaultSharedPreferences(this)
            val customKey = prefs.getString(KEY_BAIDU_MAP_KEY, "")
            if (!customKey.isNullOrEmpty()) {
                SDKInitializer.setApiKey(customKey)
                LocationClient.setKey(customKey)
            }

            // 在使用 SDK 各组间之前初始化 context 信息，传入 ApplicationContext
            SDKInitializer.initialize(this)
            SDKInitializer.setCoordType(CoordType.BD09LL)
        } catch (e: Throwable) {
            KailLog.e(this, APP_NAME, "Baidu Map SDK init failed: ${e.message}")
        }
    }
}
