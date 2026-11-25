# MiniDiDi · 缩微滴滴

综合设计与实践 C 课程项目：五车自主运输、全局调度、网络数字孪生和 Three.js 3D 展示。

**当前可部署五车仿真系统，Android 调试 APK 已构建成功；真实 MATLAB 运行、手机传感器实测和完整现场联调仍未完成。**

快速查看：[Windows 部署](#windows-启动) · [Linux 部署](#linux-部署) · [MATLAB 接入](#matlab-模拟车) · [手机接入](#android-手机端) · [还差什么](#还差什么部件和工作)

![实际运行界面；未连接的外部设备如实显示未接入](docs/assets/dashboard.jpg)

## 部署前需要准备

本说明面向课程现场部署：一台服务端电脑、一个 MATLAB 环境、一台 Android 手机，同一局域网即可。浏览器界面和 REST API 由同一个 C++ 进程提供，运行期间保持服务端进程开启。

| 用途 | 需要准备 | 说明 |
| --- | --- | --- |
| 运行五车仿真 | Windows 或 Linux 电脑、支持 WebGL 的浏览器 | WebGL 不可用时自动回退到 2D |
| Windows 源码编译 | Git、支持 C++17 的 MinGW-w64，g++ 加入 PATH | 本机已测 g++ 16.1.0；脚本另有 Visual Studio C++/CMake 备用路线 |
| Linux 源码编译 | Git、C++17 编译器、CMake 3.16+ | 该路线已在 Linux CI 验证 |
| 自动测试与实验 | Python 3 | 仅使用 Python 标准库 |
| MATLAB 模拟车 | 可使用的 MATLAB 安装与许可证 | 建议 R2020b 或更新版本；不调用 Navigation/Robotics 工具箱；仍需实测 |
| 手机端运行 | Android 8.0/API 26 及以上手机 | 完整验收应准备带加速度计、陀螺仪、地磁和 GNSS 的设备 |
| 手机端自行编译 | Android Studio 或 Gradle 8.9、JDK 17、SDK 35、Build Tools 34.0.0 | AGP 固定为 8.7.3；有 APK 时可直接安装 |
| 多端通信 | 同一 Wi-Fi/局域网，设备可访问电脑端口 | 默认 8080；远端设备使用电脑的局域网 IP |

C++ 依赖和 Three.js 已随源码保存，服务端和网页运行时不依赖 CDN。首次克隆、Android 构建下载依赖、下载 CI 产物需要网络。

## Windows 启动

双击项目根目录的 **启动演示.cmd**，浏览器访问 <http://localhost:8080>。启动即自动生成订单并派车，不需要人工切换模式或遥控车辆。

本项目目录已编译时可直接运行；从 GitHub 克隆后首次启动需要 MinGW-w64 的 `g++`，或者 Visual Studio C++ 与 CMake。图形资源及 C++ 依赖已保存在仓库内，演示不依赖 CDN 或 npm 安装。

本机已有桌面 `MiniDiDi` 文件夹时，直接在该文件夹打开 PowerShell。新电脑先使用有仓库访问权限的 GitHub 账号克隆：

```powershell
git clone https://github.com/LUCIFA666/MiniDiDi.git
cd MiniDiDi
g++ --version
```

下面所有命令均在项目根目录执行。Git 源码不包含预编译 Windows EXE；当前本地交付目录和完整项目压缩包附带 `build/minididi.exe`，可跳过首次编译。Windows 主路线使用 MinGW-w64，本机没有验证 Visual Studio 构建环境。

```powershell
# 编译并执行核心测试
.\scripts\build.ps1 -Test
# 默认仅本机可访问；双击启动脚本会启用局域网，供手机连接
.\scripts\start.ps1
.\scripts\start.ps1 -Lan
# 如已安装 MATLAB 并加入 PATH，可在启动时同步拉起 MATLAB 模拟车
.\scripts\start.ps1 -Lan -WithMatlab
```

端口被占用可加 `-Port 8081`。关闭服务端窗口或按 Ctrl+C 停止运行。局域网连接需要系统已有网络访问权限；本项目不自动修改防火墙设置。

如果 PowerShell 执行策略阻止脚本，可以通过以下命令启动；参数只作用于本次 PowerShell 进程：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\start.ps1 -Lan
```

浏览器访问 <http://localhost:8080/api/health>，应返回包含 `"ok": true` 和 `"service": "MiniDiDi"` 的 JSON。启动窗口会列出电脑 IPv4 地址，选择与手机处于同一网络的地址，例如 `http://192.168.1.10:8080`。

## Linux 部署

安装好 Git、C++ 编译器、CMake 和 Python 3 后，在项目根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure

# 允许本机和局域网设备连接
./build/minididi --host 0.0.0.0 --port 8080 --web web
```

服务端电脑访问 <http://localhost:8080>，其他设备访问 `http://服务端局域网IP:8080`。仅本机使用时将 `--host` 改为 `127.0.0.1`；更换端口用 `--port 8081`。本路线为前台进程部署，关闭终端或按 Ctrl+C 即停止服务。

## 已实现的组成部分

| 模块 | 实现 |
| --- | --- |
| 数字世界 | C++17；23×19 栅格地图、静态建筑、五辆自主车、仿真时钟、自动订单、接送状态机 |
| 调度 | 最近空车基线、Hungarian 批量均衡分配；相同随机种子的成对对比 |
| 路径与避让 | A*；56 步时空预约；禁止占用同格、迎面对穿及静态障碍碰撞 |
| 展示 | 本地 Three.js 3D、轨迹与订单标记、车队与指标；WebGL 不可用时自动转 2D |
| MATLAB | 独立进程 M01；差速运动学、比例航向控制、REST 位姿同步、断线恢复 |
| Android | 原生 Java App；加速度、陀螺仪、地磁、旋转向量、GNSS；基础 ENU 惯性位姿估计 |
| 工程 | 一键启动、CMake、自动测试、固定工作负载实验、GitHub Actions、分工和验收文档 |

MATLAB 和手机都是实际外部设备端，未运行时界面显示“未接入”。五辆自主数字车仍独立工作。手机 P01 在**独立的物理位姿实验区**中显示，坐标以真实米计，不把人的随机走动当作城市道路上的自动出租车。MATLAB M01 则接入城市道路、实际承担运输任务。

## MATLAB 模拟车

在 MATLAB 中切换至 `clients/matlab` 并运行：

```matlab
test_controller
run_twin('http://127.0.0.1:8080')
% 不同电脑时使用服务端局域网地址
% run_twin('http://192.168.1.10:8080')
```

使用基础 MATLAB 的 `webwrite` 和绘图，不依赖 Navigation/Robotics 工具箱。推荐 R2020b 或更新版本。服务端负责路径与空间预约；MATLAB 独立积分运动模型，只有回报到达端点后数字车才推进。关闭 MATLAB 图窗停止客户端。断线时 M01 保留占位和未完成任务，重连后继续，避免把网络中断伪装为车辆自动到达。

成功标志：MATLAB 出现轨迹图，网页车队增加第六辆城市车辆 `M01`、设备卡片显示已连接，M01 的实际轨迹与数字映像同步并能完成订单。上述 `.m` 文件和 `test_controller` 尚需在真实 MATLAB 环境执行，网络协议测试不能替代这一步。

## Android 手机端

用 Android Studio 打开 `clients/android`，使用 **JDK 17、Android SDK 35、Gradle 8.9** 同步工程并构建 `app`。项目使用 Android Gradle Plugin 8.7.3，无第三方 App 库。

SDK 配套安装 Build Tools 34.0.0。工程未附带 Gradle Wrapper，命令行构建前需自行准备 Gradle 8.9，然后在项目根目录运行：

```text
gradle -p clients/android assembleDebug
```

自行构建的 APK 位于 `clients/android/app/build/outputs/apk/debug/`。首次 [GitHub Actions 构建](https://github.com/LUCIFA666/MiniDiDi/actions/runs/45584776761) 已通过，可在该页面的 Artifacts 下载 `MiniDiDiPhone-debug-apk`。当前桌面项目和交付压缩包另附 `artifacts/MiniDiDiPhone-debug.apk`，Git 源码仓库不提交该二进制文件。APK 构建成功不代表已完成手机真机测试。

下载 Artifacts 后先解压，将 APK 传到手机安装，按系统提示授予这一次安装所需权限。自行构建的文件名为 `app-debug.apk`。CI 产物过期时，可在仓库 Actions 中手动运行 `Build and integration checks` 重新生成，或本地编译。

1. 电脑用 `-Lan`（Linux 使用 `--host 0.0.0.0`）启动，电脑与手机连接同一局域网；先用手机浏览器访问 `http://电脑IP:8080/api/health`，确认可达。
2. 打开“MiniDiDi 手机车”，填写启动窗口显示的 `http://电脑IP:8080`，授予运行时定位权限。
3. 演示前静置并按一次归零，然后连接。保持 App 在前台，携带手机运动。
4. 网页手机卡片会显示真实位置、航向和采样帧数。无 GNSS 权限或无有效室外定位时继续使用惯性估计。

这是课程原型：短时积分有漂移，GNSS 室内可能无效，不宣称完成 SLAM。读取缺失的传感器会显示缺失状态；没有旋转向量时用重力与地磁估计姿态。

手机应填写电脑的局域网 IP，不能用手机自己的 `localhost` 连接电脑。若局域网监听已开启仍不可达，检查设备网络、路由器客户端隔离和电脑端口的网络访问权限。

## 完整系统怎么验收

正式自主运行片段开始前，完成地址、权限、原点和设备连接设置；考核过程中不靠遥控或人工切换模式完成任务。

1. 启动服务端后，V01–V05 自动接送，完成订单数持续增长。
2. 在真实 MATLAB 中执行控制器测试和 `run_twin`，观察 M01 实际运动、避障与接送。
3. 真机 App 接入后 P01 显示在线，携带手机移动时网页位姿、航向和帧数更新。
4. 连续运行至少 2 分钟，记录是否穿越建筑、车辆重叠、迎面对穿或异常暂停。
5. 自主展示后单独检查断线超过 5 秒的状态变化及自动重连，保存真实联调记录。
6. 导出本次运行数据，保留运行截图和视频，整理答辩材料。

MATLAB/手机未接入时，五辆数字车仍可独立演示；这不表示外部设备模块已经完成真实验收。

## 还差什么部件和工作

下表列出需要准备、确认或实测的项目，并不表示已经确认小组没有相应设备。

| 优先级 | 部件或工作 | 当前已有 | 还需完成 |
| --- | --- | --- | --- |
| 必做验收 | MATLAB 环境与许可证 | 模拟车源码、控制器测试和协议测试 | 在有 MATLAB 的电脑上执行测试，确认运动控制、避障、实际接送和同步，保存版本与运行记录 |
| 必做验收 | Android 手机及传感器 | APK、采集代码、基础位姿估计器 | 准备/确认带加速度计、陀螺仪、地磁和 GNSS 的手机，完成安装、权限、静止/直行/转弯和断线恢复实测 |
| 必做验收 | 多端网络与整套联调 | REST 接口、心跳重连、局域网启动脚本 | 服务端、真实 MATLAB 和手机同时运行，确认 IP/端口可达，持续自主运行并留存证据 |
| 按验收口径确认 | 手机与城市地图的坐标关系 | P01 在独立物理位姿实验区显示 | 若要求手机进入同一城市道路并参与任务，补充场地坐标标定、地图映射及任务联动；当前未实现 |
| 必交材料 | 正式报告与展示材料 | 架构、接口、分工、实验数据、答辩提纲 | 整理 DOCX/PDF 报告、正文不超过 5 页的 PPT、真实运行图片/短视频；视频总长建议不超过 5 分钟 |
| 建议改进 | 调度效果与拥堵场景 | 两种策略、三个种子的原始对比 | 当前批量均衡策略未稳定降低等待/里程；可调目标函数和参数、扩大场景验证，保留真实结果 |
| 进阶选做 | 树莓派实体无人车与视觉感知 | 本次已完成的挑战方向是 3D 展示 | 若选实体车方向，另需小车/树莓派平台、摄像头、运动控制与供电配套、硬件驱动及视觉/定位算法；本轮未实现 |

默认五辆车是数字车，可在一台电脑中自主运行。P01 当前不参与城市出租车派单；若需要手机与城市道路进一步集成，应先按老师的验收口径确认，再补相应映射与任务功能。

**当前收尾顺序：真实 MATLAB 验收 → 手机真机验收 → 全模块联调 → 报告、PPT 和演示视频。**

## 验证与实验

```powershell
.\scripts\build.ps1 -Test
python tests/api_tests.py
python scripts/benchmark.py --seeds 1 7 42 --steps 1200
```

Linux：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`，`cmake --build build -j2`，`ctest --test-dir build --output-on-failure`，随后运行上述 Python 脚本。

实验 JSON 保留原始数值。平均等待仅统计已上车订单，因此同时报告完成率、订单总数和未上车累计等待；不能仅凭一个种子或一个均值断言策略优越。`docs/VALIDATION.md` 记录本机实际验证情况和剩余真机检查。

已运行的六组策略实验均完成 80/80 单、碰撞计数为 0，当前批量均衡策略未稳定优于最近空车基线。时空预约是有限窗口规划，不保证任意地图上的多车问题都能求解。运行状态保存在内存，重置/重启会清空本次运行历史，需要的数据应提前通过网页导出。

## 常见问题

| 现象 | 处理方式 |
| --- | --- |
| 找不到 g++ 或首次编译失败 | 准备支持 C++17 的 MinGW-w64 并加入 PATH；确认 `g++ --version` 可运行，再执行构建脚本 |
| 8080 端口被占用 | Windows 加 `-Port 8081`，Linux 改 `--port 8081`，各客户端同步修改 |
| 手机无法连接 | 检查局域网监听、电脑 IP、同网可达性和端口权限；先在手机浏览器打开健康接口 |
| MATLAB 显示 Reconnecting | 确认服务端在线、URL 正确、健康接口可达，查看服务端是否报错；重置后客户端会重新注册 |
| P01 不动或漂移 | 检查 App 在前台、传感器缺失状态、定位权限和 GNSS 条件；归零后短时测试并记录误差 |
| 网页出现 2D 地图 | 浏览器可能未提供 WebGL；切换支持 WebGL 的浏览器检查 3D |
| GitHub 或 APK 下载页打不开 | 仓库是私有的，需登录有访问权限的账号；下载 Artifacts 后先解压 |

## 仓库结构与协作

```text
server/              世界、调度、A*、预约、HTTP 服务
web/                 3D/2D 界面与本地 Three.js
clients/matlab/      MATLAB 运动模型与网络客户端
clients/android/     Android 传感器 App
tests/               C++、HTTP、手机估计算法测试
scripts/             编译、启动和实验脚本
experiments/         实际运行生成的策略比较结果
docs/                要求对应表、接口、架构、五人分工、现场验收
.github/workflows/   C++ 测试及 Android APK 构建
third_party/         固定版本的 C++ 库与许可证
```

详见 [要求对应表](docs/REQUIREMENTS.md)、[接口](docs/API.md)、[五人分工](docs/TEAM.md)、[现场演示](docs/DEMO.md) 和 [开源来源](docs/SOURCES.md)。

## GitHub

GitHub 私有仓库：[LUCIFA666/MiniDiDi](https://github.com/LUCIFA666/MiniDiDi)，主分支为 `main`。仓库不保存访问令牌或账号密码。登录有访问权限的 GitHub 账号后可以查看源码及 Actions 构建结果。

课程课件建议 Gitee 私有仓库，本项目按当前用户要求使用 GitHub；如老师要求仍需 Gitee，可增加第二个 remote。

项目原创代码采用 MIT 许可证，第三方代码保留各自版权与许可证。请小组逐项阅读、测试并理解代码，用真实的联调结果和个人贡献准备课程报告与答辩。
