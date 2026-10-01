# e-puck Mini 多机器人控制中心

**版本 0.5.0 | Windows x64 | Qt 6 / C++17 | 蓝牙 SPP / COM 串口**

用于 e-puck Mini 的桌面控制工作站。单机模式提供驱动、LED、传感器查看和可视化路径规划；多机模式将同步组控制和每台设备的左右轮分控放在同一个工作区。各设备独立连接、独立接收数据，选择某台机器人不会断开其他机器人。

[下载 Windows 软件包](https://github.com/1405264556/epuck_mini_control/releases/latest) · [版本修改记录](CHANGELOG.md) · [协调算法接口](docs/multi_robot_control.md) · [通信与验收说明](docs/communication_and_validation.md)

## 快速启动

1. 下载 Release ZIP，完整解压其中的 `epuck_mini_control` 文件夹，不要只提取 EXE。
2. 双击文件夹第一级的 **启动 e-puck Mini.cmd** 或 `epuck_mini_control.exe`。
3. 机器人开机，并在 Windows 完成蓝牙配对；确认设备管理器存在对应 SPP COM 端口。
4. 关闭官方 Monitor、串口助手及其他占用相同端口的程序。
5. 点击左侧“后台扫描 COM 端口”，识别完成后连接选中设备，或点击“连接全部”。
6. 顶部选择“单机”或“多机”，根据数据时效和连接状态确认设备正常。

本机工程第一级提供启动脚本和 Windows 启动快捷方式，正式 EXE 为 `release/epuck_mini_control/epuck_mini_control.exe`。软件包包含 Qt 和 MinGW 运行库，不需要安装 Qt。外部路径算法需要 Python/MATLAB 运行环境。

## 0.5.0 更新

- 浅色工作站界面，顶栏直接切换单机/多机模式；多机控制区独占底部宽工作区，不再挤在单机侧栏。
- 同步组速度、转向支持滑块、步进和数值输入；设备表逐台显示数据时效、左右轮输入、下发值、应用和停止。
- 批量轮速采用共同序列号、单调时钟截止时间和每端口调度器；合并连续输入，停止优先，避免命令积压。
- 手动控制使用 100 ms 心跳；切换模式、隐藏控制、失去窗口焦点时停止手动运动；指令超过 1 秒未更新时串口线程发送零速。
- 修正官方 SerCom 指令字节及二进制结束符；补齐原始加速度、编码器和三路麦克风接收。
- 每字段有效标记和时间戳，正确接收零值并保留其他通道；修正接近传感器负值误转为大数。
- 传感器与图像使用有边界、超时和恢复的独占查询事务；摄像头仅查看时采集，避免与遥测回复混帧。
- 轨迹来自编码器里程计，不把下发速度当成实际运动；自动算法在编码器反馈过期时暂停。
- A* 后台计算，Python/MATLAB 路径规划异步执行；日志面板最近 1000 条，完整历史写入文件。
- 修正 LED 开启和全部关闭；增加协议、控制、UI 和压力测试，提供 Release 打包脚本与根目录启动入口。

## 界面布置

| 区域 | 单机模式 | 多机模式 |
| --- | --- | --- |
| 顶栏 | 品牌、模式切换、扫描、连接、断开、协调、烧录、急停 | 模式切换不改变已有连接 |
| 左侧 | 机器人列表和连接状态；扫描、连接、连接全部、全部断开 | 管理整个设备队列 |
| 中央 | XY 画布、目标、规划路径、里程计轨迹、虚拟障碍 | 所有设备位姿、轨迹和端口标签 |
| 右侧 | 当前设备的传感器及驱动/LED/路径规划 | 队列时效概览和选中设备详细传感器 |
| 底部 | 状态栏；“视图”菜单可显示日志 | 全宽“同步与分控 / 协调算法”工作区和状态栏 |

较小窗口的长面板可以滚动；日志默认隐藏，保留控制和画布空间。“视图 / 重置布局”可恢复当前模式布局。

### 单机器人控制

- **驱动 / 摇杆**：按住拖动控制差速运动，释放停止。
- **驱动 / 滑块**：左右轮分别输入或拖动，范围 -1000..1000 步/秒；非零输入持续保持，按急停结束。
- **LED**：环形 0..7、机身灯 8、前灯 9；显示本软件请求的灯状态，不是硬件回读。
- **路径规划**：选内置 A*、Python 或 MATLAB，在画布选目标点或输入 XY；规划后开始跟踪，支持暂停、继续和回原点。
- **虚拟障碍**：通过画布工具或右键菜单连续放置；A* 以 8 cm 膨胀半径避障。修改目标或障碍后重新规划。
- **画布清理**：清除路径、轨迹、虚拟障碍和预览，不重置里程计原点；回原点指地图坐标 (0,0)。

画布单位 cm，+X 向右，+Y 向上。拖动机器人用于人工设定地图初始位姿，不是机器人真的移动。初始位置须与实验场地对应；目前没有外部定位系统。

### 多设备同步与分控

1. 连接多台设备，顶栏切换“多机”。设备是否在线与当前选中无关。
2. “同步”列勾选组成员，设置速度和转向，点“应用到同步组”。方向按钮按下运动、释放停止。
3. 同表逐台输入左右轮速度，点该行应用或停止；其他设备保持各自命令。
4. 取消勾选正在手动驱动的设备会停止该设备；切换单机模式会结束多机手动运动。
5. 点设备行、列表或画布机器人可选择详细传感器对象，不会切换连接或中断其他设备通信。
6. “协调算法”对同步勾选组运行队形保持、群集目标跟随或兼容 C++ 插件；需至少两台设备及有效编码器。

同批轮速在主机端按共同截止时间调度，但 Windows、蓝牙和固件仍有各自延迟，**不等于硬件时钟级同步**。“已下发”是本机命令值，不能证明机器人已执行。精确同步需固件端执行时间戳、时钟同步及命令确认。

手动轮速会退出当前自动算法；参与设备断线会停止协调；编码器超过 1 秒未更新会暂停。软件保护不能代替物理急停和安全隔离。

## 传感器接入

右侧下拉框选择类型，另有原始数据表。字段独立显示“未接入 / 实时 / 过期”，不会把零值认作没收到数据。

| 信号 | 当前 COM / BTcom 接入 | 含义 |
| --- | --- | --- |
| 8 路红外接近 | 连续轮询，负校准噪声归零，自适应平滑 | ADC 响应，不是 cm/mm 距离 |
| 加速度球坐标 | 连续轮询 | 固件幅度、方向、倾角 |
| 原始 XYZ 加速度 | 扩展连续轮询 | ADC，未标定不冒充 m/s² |
| 左右编码器 | 扩展连续轮询 | 有符号 16 位步数，处理回绕，驱动位姿 |
| 三路麦克风 | 扩展连续轮询 | 原始值和 0..1 幅度，不是录音波形 |
| 摄像头 | 查看时按需请求 | 默认 40×40 灰度，2 s 超时恢复 |
| Selector | 握手读取一次 | 不是持续轮询；时间戳为读取时刻 |
| ToF、陀螺仪、电池、IR 遥控 | 数据结构/部分解析预留，COM 默认不轮询 | 需确定硬件与固件协议，未收到显示未接入 |

默认发送 N + A + a + Q + u + NUL，接收 44 字节：接近 16、球坐标 12、原始加速度 6、编码器 4、麦克风 6。旧固件只返回 28 字节时可退回 N + A + NUL，但缺少编码器不能执行当前闭环路径/协调控制。

每端口一个待完成遥测事务，默认间隔 60 ms、接收超时 500 ms，迟到字节进入重新同步。显示按需刷新，不为每个通知重建 UI。字节与时序见 [通信说明](docs/communication_and_validation.md)。

## 文件组成

```text
robot_m/
  启动 e-puck Mini.cmd          工程/软件包第一级快捷启动
  e-puck Mini 使用说明.url      在线文档快捷方式
  CMakeLists.txt                版本及依赖
  CMakePresets.json             本机 Debug/Release 预设
  README.md / CHANGELOG.md      使用说明与版本记录
  LICENSE / vcpkg.json          许可和依赖清单
  src/
    main.cpp / app/             入口、全局对象、元类型注册
    comm/                      扫描、设备线程、协议、批量指令调度
    core/                      管理、字段时效、编码器位姿
    control/                   编队、群集、A*、异步规划、策略插件
    ui/                        主窗口、单机/多机工作区、传感器、画布
    util/                      日志、数学和线程工具
  resources/                   图标、资源、浅色工作站样式
  tests/                       GoogleTest/Qt Test 协议、控制、UI、压力测试
  docs/                        算法接口、通信时序与验收
  examples/algorithms/          Python/MATLAB 协调协议原型
  tools/package.ps1            Qt 部署、运行库复制和 ZIP 打包
  build/                       构建与测试报告，不提交
  release/epuck_mini_control/   独立启动目录，不提交源码仓库
  release/*-win64.zip           软件包，通过 GitHub Release 分发
```

### 关键模块

- SerialManager：有限并发扫描、连接和线程管理，批量轮速共同编号与截止时间。
- SerialPortWorker：单设备握手、事务接收、图像组帧、超时恢复、指令看门狗。
- MotorCommandScheduler：端口线程按单调时钟发送最新命令，丢弃旧序号，停止优先。
- SensorData：字段有效性、独立时间戳和部分更新合并；零值也是有效数据。
- RobotInstance / RobotManager：命令和连接状态、编码器里程计、全部实例管理。
- MultiRobotCoordinator：20 Hz 自动控制、手动/自动仲裁、断线和反馈时效保护。
- PathPlanningJob：后台 A*、异步外部规划，不在 UI 线程等待。
- MultiRobotDriveWidget / SensorDisplayWidget：同步及分控表、按设备和可见类型刷新。

## 算法与烧录扩展

路径脚本读取 JSON 文件，含 start、goal、obstacles、bounds、obstacle_radius；标准输出返回 [[x,y], ...]，范围 [-200,200] cm，至少两点，超时 30 秒。PATH 中的 python 执行 Python；matlab -batch 调用与文件同名的 MATLAB 函数。

协调插件使用 IMultiRobotStrategy，置于 EXE 同级 algorithms/；DLL 必须匹配 Qt、编译器及位数。当前 step() 在主线程同步执行，应短于 40 ms；复杂优化须自行异步处理并返回最新完成解。内置避障计算和外部 NDJSON 协议不等于已集成安全层或可直接运行的 Python/MATLAB 协调适配器。

烧录窗口提供多目标选择。现有单目标 DFU 依赖兼容 USB DFU 硬件/固件，不能因为勾选 COM 设备就认定会烧录到它。**多机批量烧录/OTA 队列仍是预留接口**。

## 源码构建与打包

依赖 Qt 6.5+（Core/Gui/Widgets/SerialPort/Bluetooth/Charts/Network/Concurrent/Test）、Eigen 3.4、CMake 3.21+、Ninja 和匹配 Qt 的 MinGW。测试额外依赖 GoogleTest，libusb 可选。

本机 Qt 位于 F:/Qt/6.10.3/mingw_64，MinGW 位于 F:/Tools/mingw64/mingw64。其他机器请调整构建预设和打包参数。

```powershell
$env:PATH='F:\Tools\cmake-3.31.0-windows-x86_64\bin;F:\Qt\6.10.3\mingw_64\bin;F:\Tools\mingw64\mingw64\bin;' + $env:PATH
cmake --preset mingw-debug
cmake --build --preset mingw-debug --parallel 6
$env:QT_QPA_PLATFORM='offscreen'
.\build\mingw-debug\epuck_tests.exe
Remove-Item Env:QT_QPA_PLATFORM
cmake --preset mingw-release
cmake --build --preset mingw-release --parallel 6
powershell -ExecutionPolicy Bypass -File .\tools\package.ps1
```

测试插件位置在配置时从 qmake 查询，无窗口 UI 测试显式加载 Windows 字体。报告和截图在 build/；软件包不包含测试用模拟设备。

## 排错与限制

- **扫描不到**：确认配对和端口，检查占用；扫描并发有限，蓝牙入站空端口超时属正常。可手动输入端口。
- **连接失败**：查看握手和日志，不能仅凭串口打开判断连接成功。
- **接近值波动**：红外 ADC 受光照、材质、反射及校准影响，不能当公制距离。
- **摄像头空白**：需固件支持图像命令，查看时才采集；占用该端口查询带宽，其他设备独立接收。
- **轨迹不动/算法暂停**：检查编码器时效和扩展命令；没有编码器不制造虚假运动。
- **精度**：默认轮径 4.1 cm、轮距 5.3 cm、每圈 1000 步，须按实物标定；漂移需外部定位纠正。
- **实机验收**：自动测试不能替代多台机器人长期在线、蓝牙时延和执行同步测试，见验收文档。

日志在 %LOCALAPPDATA%\EPFL\e-puck Mini Control\epuck_mini_control.log，可通过“视图”打开日志面板。许可证见 [MIT LICENSE](LICENSE)。
