# 开源来源与复用说明

## 实际随仓库分发的依赖

| 组件 | 固定版本 | 官方来源 | 许可证 | 用途 |
| --- | --- | --- | --- | --- |
| cpp-httplib | v0.58.0 | https://github.com/yhirose/cpp-httplib/tree/v0.58.0 | MIT，见 third_party/cpp-httplib.LICENSE | HTTP 服务与静态资源 |
| nlohmann/json | v3.12.0 | https://github.com/nlohmann/json/tree/v3.12.0 | MIT，见 third_party/nlohmann-json.LICENSE | JSON 解析与输出 |
| Three.js | 0.170.0 | https://github.com/mrdoob/three.js/tree/r170 | MIT，见 web/vendor/three.LICENSE | 3D 渲染与 OrbitControls |

版本固定为了可复现，未声称它们是最新版本。原始依赖文件未做代码改写，下载来源和 SHA-256 见 dependency-checksums.json。客户端 UI、服务端世界/调度/规划、MATLAB 模型及 Android 估计算法由本工程新写，不能把第三方 HTTP、JSON、图形库列为原创算法。

## 附件列出的参考资料

- [Ridehail](https://github.com/tomslee/ridehail)：MIT，参考网约车仿真场景与界面组织；未直接复制其 Python 引擎。
- [Multi-Agent Vehicle Fleet Optimization System](https://github.com/Marshmallo-White/-Multi-Agent-Vehicle-Fleet-Optimization-System)：附件中的寻路/调度参考链接；本工程未复用该仓库代码，不推定其许可证。
- [android-imu-2-x](https://github.com/kaist-uvr-lab/android-imu-2-x)：MIT，参考手机传感器传输方向；本工程采用自写 REST 客户端，未复制其 App。
- [MathWorks Motion Planning](https://www.mathworks.com/help/nav/motion-planning.html)：附件建议的官方路径规划资料；本项目避免依赖额外工具箱。
- [MathWorks webwrite](https://www.mathworks.com/help/matlab/ref/webwrite.html)：核对 JSON REST 请求方式。
- [Android Motion Sensors](https://developer.android.com/develop/sensors-and-location/sensors/sensors_motion)：传感器含义、坐标与单位。
- [Android SensorManager](https://developer.android.com/reference/android/hardware/SensorManager)：旋转矩阵与姿态接口。
- [Android Location Permissions](https://developer.android.com/develop/sensors-and-location/location/permissions)：前台定位权限。
- [AGP 8.7 compatibility](https://developer.android.com/build/releases/agp-8-7-0-release-notes)：JDK 17、Gradle 8.9、SDK 35 兼容信息。
- [Three.js Installation](https://threejs.org/manual/pages/installation.html)：ES module/import map 方式。

本项目的研究/课程说明不将库的功能或参考资料里的结果当作本组实测成果。
