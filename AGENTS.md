# AGENTS.md - Kail 位置模拟器开发指南

本文档为 Kail 位置模拟器 Android 应用程序的编码代理提供全面指南。请遵循这些指南以保持代码库的一致性和质量。

## 项目概述
- **语言**：Kotlin
- **架构**：MVVM (Model-View-ViewModel)
- **UI 框架**：Jetpack Compose
- **数据库**：Room
- **构建系统**：Gradle with Kotlin DSL and version catalogs
- **最低 SDK**：27 (Android 8.1)
- **目标 SDK**：36 (Android 15)
- **编译 SDK**：36

## C++ 步态模拟模块

### 概述
项目包含一个 C++ 实现的步态模拟模块 (`kail_gait_sim`)，用于生成真实的传感器数据来增强位置模拟的真实性。

### 功能特性
- **传感器模拟**: 加速度计、步数计数器、步数检测器
- **步态模式**: 行走 (Walk)、跑步 (Run)、快跑 (FastRun)
- **参数配置**: 步频、模式、启用状态
- **动态配置**: 支持运行时重新加载配置文件
- **JNI 接口**: 提供完整的 Kotlin/Java 调用接口

### 文件结构
```
app/src/main/cpp/
├── CMakeLists.txt          # 构建配置
├── simulate.h/.cpp         # 核心模拟算法
├── config.h/.cpp           # 配置管理
└── gait_jni.cpp            # JNI 接口
```

### 配置格式
```ini
steps_per_minute=120.0
mode=walk
enable=1
```

### 使用方式
```kotlin
// 初始化
GaitSimulator.init(configPath)

// 更新参数
GaitSimulator.updateParams(150.0f, 1, true) // 150步/分钟，跑步模式，启用

// 处理传感器事件
GaitSimulator.processEvent(timestampNs, sensorData, TYPE_ACCELEROMETER)

// 重新加载配置
GaitSimulator.reloadConfig(System.nanoTime())

// 销毁
GaitSimulator.destroy()
```

### 构建要求
- CMake 3.10+
- C++17 标准
- Android NDK
- Dobby hook 库

## 构建、检查和测试命令

### 构建
```bash
# 构建调试 APK
./gradlew assembleDebug

# 构建发布 APK
./gradlew assembleRelease

# 清理构建
./gradlew clean build

# 构建特定变体
./gradlew assembleFlavorDebug
```

### 测试
```bash
# 运行所有单元测试
./gradlew test

# 运行所有仪器化测试
./gradlew connectedAndroidTest

# 运行特定模块的测试
./gradlew :app:testDebugUnitTest

# 运行单个测试类
./gradlew :app:testDebugUnitTest --tests "com.kail.location.ExampleUnitTest"

# 运行单个测试方法
./gradlew :app:testDebugUnitTest --tests "com.kail.location.ExampleUnitTest.addition_isCorrect"

# 在连接设备上运行仪器化测试
./gradlew :app:connectedDebugAndroidTest
```

### 检查和代码质量
```bash
# 运行 lint 检查
./gradlew lint

# 运行特定变体的 lint
./gradlew lintDebug

# 运行 Kotlin linter (ktlint)
./gradlew ktlintCheck

# 使用 ktlint 格式化代码
./gradlew ktlintFormat

# 运行 detekt 静态分析
./gradlew detekt
```

### 代码覆盖率
```bash
# 生成测试覆盖率报告
./gradlew createDebugCoverageReport

# 在浏览器中查看覆盖率（生成后）
open app/build/reports/coverage/debug/index.html
```

## 代码风格指南

### Kotlin 风格
- 使用 `kotlin.code.style=official`（在 `gradle.properties` 中配置）
- 遵循 Kotlin 编码约定
- 使用 4 个空格进行缩进
- 最大行长度：120 个字符
- 在多行构造中使用尾随逗号

### 命名约定

#### 类和接口
```kotlin
// Activities
class LocationSimulationActivity : AppCompatActivity()

// ViewModels
class LocationSimulationViewModel : ViewModel()

// Repositories
class HistoryRepository(private val historyDao: HistoryDao)

// Data classes
data class LocationData(
    val latitude: Double,
    val longitude: Double
)

// Composables
@Composable
fun LocationPickerScreen() {
    // ...
}
```

#### 变量和属性
```kotlin
// 私有属性
private var currentLocation: Location? = null
private val _locationState = MutableStateFlow<Location?>(null)
val locationState: StateFlow<Location?> = _locationState

// 参数
fun updateLocation(latitude: Double, longitude: Double)

// 局部变量
val formattedAddress = formatAddress(location)
```

#### 函数和方法
```kotlin
// 常规函数
fun calculateDistance(from: Location, to: Location): Double

// 扩展函数
fun Location.format(): String

// 可组合函数
@Composable
fun LocationDisplay(location: Location)

// 回调函数
fun onLocationChanged(location: Location)
```

### 导入组织
```kotlin
// 分组 1: Android 框架导入
import android.Manifest
import android.content.Context
import android.location.Location

// 分组 2: 第三方库
import androidx.compose.runtime.Composable
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.StateFlow

// 分组 3: 项目导入
import com.kail.location.models.LocationData
import com.kail.location.utils.LocationUtils
```

### 包结构
```
com.kail.location/
├── data/
│   ├── local/          # Room DAOs 和实体
│   └── remote/         # API 服务
├── models/             # 数据类
├── repositories/       # 数据仓库
├── service/            # 后台服务
├── utils/              # 工具类
├── viewmodels/         # ViewModels
└── views/              # Activities 和 Composables
    ├── base/           # 基类
    ├── common/         # 共享 UI 组件
    ├── locationsimulation/
    ├── navigation/
    └── theme/          # 主题
```

### 注释和文档

#### KDoc 注释
```kotlin
/**
 * 位置模拟活动类
 * 主界面，负责显示位置模拟的各项功能
 *
 * Features:
 * - GPS location spoofing
 * - Route simulation
 * - Joystick control
 */
class LocationSimulationActivity : AppCompatActivity() {

    /**
     * 初始化用户界面
     * 设置 Compose 内容并配置主题
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        // Implementation
    }
}
```

#### 内联注释
```kotlin
// 仅在必要时更新位置，避免频繁刷新
if (shouldUpdateLocation()) {
    updateLocation(newLocation)
}
```

### 架构模式

#### MVVM 实现
```kotlin
// ViewModel
class LocationViewModel : ViewModel() {
    private val _location = MutableStateFlow<Location?>(null)
    val location: StateFlow<Location?> = _location.asStateFlow()

    fun updateLocation(newLocation: Location) {
        _location.value = newLocation
    }
}

// Activity/Composable
@Composable
fun LocationScreen(viewModel: LocationViewModel = viewModel()) {
    val location by viewModel.location.collectAsState()

    // UI implementation
}
```

#### Repository 模式
```kotlin
class LocationRepository @Inject constructor(
    private val locationDao: LocationDao,
    private val locationService: LocationService
) {
    fun getLocations(): Flow<List<Location>> = locationDao.getAllLocations()

    suspend fun saveLocation(location: Location) {
        locationDao.insertLocation(location)
    }
}
```

### 错误处理

#### Try-Catch 块
```kotlin
fun loadLocationData(): LocationData? {
    return try {
        val data = locationService.fetchData()
        LocationData(data.latitude, data.longitude)
    } catch (e: IOException) {
        Log.e(TAG, "Failed to load location data", e)
        null
    } catch (e: SecurityException) {
        Log.w(TAG, "Location permission denied")
        null
    }
}
```

#### Result 模式
```kotlin
sealed class LocationResult {
    data class Success(val location: Location) : LocationResult()
    data class Error(val message: String) : LocationResult()
}

fun getCurrentLocation(): LocationResult {
    return try {
        val location = locationManager.getLastKnownLocation()
        LocationResult.Success(location)
    } catch (e: Exception) {
        LocationResult.Error("Unable to get location: ${e.message}")
    }
}
```

### Compose 指南

#### 状态管理
```kotlin
@Composable
fun LocationControls(
    isEnabled: Boolean,
    onToggle: (Boolean) -> Unit
) {
    var localEnabled by remember { mutableStateOf(isEnabled) }

    LaunchedEffect(isEnabled) {
        localEnabled = isEnabled
    }

    Switch(
        checked = localEnabled,
        onCheckedChange = { checked ->
            localEnabled = checked
            onToggle(checked)
        }
    )
}
```

#### 副作用
```kotlin
@Composable
fun LocationScreen(viewModel: LocationViewModel) {
    val location by viewModel.location.collectAsState()

    // Handle side effects
    LaunchedEffect(Unit) {
        viewModel.startLocationUpdates()
    }

    DisposableEffect(Unit) {
        onDispose {
            viewModel.stopLocationUpdates()
        }
    }
}
```

### 测试指南

#### 单元测试
```kotlin
class LocationUtilsTest {

    @Test
    fun `calculate distance between two points`() {
        // Given
        val point1 = Location("").apply {
            latitude = 37.7749
            longitude = -122.4194
        }
        val point2 = Location("").apply {
            latitude = 34.0522
            longitude = -118.2437
        }

        // When
        val distance = LocationUtils.calculateDistance(point1, point2)

        // Then
        assertTrue(distance > 0)
    }
}
```

#### Compose 测试
```kotlin
@RunWith(AndroidJUnit4::class)
class LocationScreenTest {

    @get:Rule
    val composeTestRule = createComposeRule()

    @Test
    fun locationScreen_displaysLocation() {
        // Given
        val testLocation = Location("").apply {
            latitude = 37.7749
            longitude = -122.4194
        }

        // When
        composeTestRule.setContent {
            LocationScreen(testLocation = testLocation)
        }

        // Then
        composeTestRule.onNodeWithText("37.7749, -122.4194")
            .assertIsDisplayed()
    }
}
```

### 依赖注入

#### Hilt（推荐）
```kotlin
@HiltAndroidApp
class LocationApplication : Application()

@HiltViewModel
class LocationViewModel @Inject constructor(
    private val repository: LocationRepository
) : ViewModel()

@AndroidEntryPoint
class LocationSimulationActivity : AppCompatActivity()
```

### 安全最佳实践

#### 权限处理
```kotlin
class LocationPermissionManager(private val context: Context) {

    fun hasLocationPermission(): Boolean {
        return ContextCompat.checkSelfPermission(
            context,
            Manifest.permission.ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED
    }

    fun requestLocationPermission(activity: Activity) {
        ActivityCompat.requestPermissions(
            activity,
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION),
            LOCATION_PERMISSION_REQUEST_CODE
        )
    }
}
```

#### 敏感数据
- 永远不要记录敏感的位置数据
- 对持久化数据使用加密存储
- 验证所有来自外部来源的输入

### 性能指南

#### 内存管理
```kotlin
// 为协程使用 viewModelScope
viewModelScope.launch {
    // Coroutine work
}

// 清理资源
override fun onCleared() {
    super.onCleared()
    // Cancel ongoing operations
}
```

#### Compose 性能
```kotlin
// 为昂贵的计算使用 remember
val formattedLocation = remember(location) {
    formatLocation(location)
}

// 为派生状态使用 derivedStateOf
val isLocationValid = remember(location) {
    derivedStateOf {
        location != null &&
        location.latitude >= -90 && location.latitude <= 90 &&
        location.longitude >= -180 && location.longitude <= 180
    }
}
```

### Git 工作流

#### 提交消息
```
feat: add location joystick control
fix: resolve crash when GPS disabled
docs: update AGENTS.md guidelines
refactor: extract location formatting utility
test: add unit tests for location calculations
```

#### 分支命名
```
feature/add-joystick-control
bugfix/crash-on-gps-disabled
refactor/location-utils
```

### 代码审查清单
- [ ] 代码遵循既定的模式和约定
- [ ] 为新功能添加单元测试
- [ ] 使用 Compose 测试 UI 组件
- [ ] 实现正确的错误处理
- [ ] 防止内存泄漏
- [ ] 正确处理权限
- [ ] 使用 KDoc 正确记录代码
- [ ] 不记录敏感数据
- [ ] 构建通过所有检查（lint、测试等）

### 工具配置

#### IDE 设置
- 使用 Android Studio 或 IntelliJ IDEA
- 启用 Kotlin 官方代码风格
- 配置 ktlint 进行代码格式化
- 设置 detekt 进行静态分析

#### 预提交钩子
```bash
# 提交前运行 lint 和测试
./gradlew ktlintCheck
./gradlew testDebugUnitTest
```

随着代码库的发展和新模式的出现，应更新此文档。始终参考现有代码以获取正确实现的示例。</content>
<parameter name="filePath">AGENTS.md