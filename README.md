# MiniDiDi · 缩微滴滴

综合设计与实践 C 课程项目：五车自主运输、全局调度、网络数字孪生和 Three.js 3D 展示。

![实际运行界面；未连接的外部设备如实显示未接入](docs/assets/dashboard.jpg)

## Windows 启动

双击项目根目录的 **启动演示.cmd**，浏览器访问 <http://localhost:8080>。启动即自动生成订单并派车，不需要人工切换模式或遥控车辆。

本项目目录已编译时可直接运行；从 GitHub 克隆后首次启动需要 MinGW-w64 的 `g++`，或者 Visual Studio C++ 与 CMake。图形资源及 C++ 依赖已保存在仓库内，演示不依赖 CDN 或 npm 安装。

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

## Android 手机端

用 Android Studio 打开 `clients/android`，使用 **JDK 17、Android SDK 35、Gradle 8.9** 同步工程并构建 `app`。项目使用 Android Gradle Plugin 8.7.3，无第三方 App 库。也可安装好 Gradle 后运行：

```text
gradle -p clients/android assembleDebug
```

自行构建的 APK 位于 `clients/android/app/build/outputs/apk/debug/`。首次 [GitHub Actions 构建](https://github.com/LUCIFA666/MiniDiDi/actions/runs/45584776761) 已通过，可在该页面的 Artifacts 下载 `MiniDiDiPhone-debug-apk`。当前桌面项目和交付压缩包另附 `artifacts/MiniDiDiPhone-debug.apk`，Git 源码仓库不提交该二进制文件。APK 构建成功不代表已完成手机真机测试。

1. 电脑用 `-Lan` 启动，电脑与手机连接同一 Wi-Fi。
2. 在 App 填写启动窗口显示的 `http://电脑IP:8080`，授予运行时定位权限。
3. 演示前静置并按一次归零，然后连接。保持 App 在前台，携带手机运动。
4. 网页手机卡片会显示真实位置、航向和采样帧数。无 GNSS 权限或无有效室外定位时继续使用惯性估计。

这是课程原型：短时积分有漂移，GNSS 室内可能无效，不宣称完成 SLAM。读取缺失的传感器会显示缺失状态；没有旋转向量时用重力与地磁估计姿态。

## 验证与实验

```powershell
.\scripts\build.ps1 -Test
python tests/api_tests.py
python scripts/benchmark.py --seeds 1 7 42 --steps 1200
```

Linux：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`，`cmake --build build -j2`，`ctest --test-dir build --output-on-failure`，随后运行上述 Python 脚本。

实验 JSON 保留原始数值。平均等待仅统计已上车订单，因此同时报告完成率、订单总数和未上车累计等待；不能仅凭一个种子或一个均值断言策略优越。`docs/VALIDATION.md` 记录本机实际验证情况和剩余真机检查。

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
