# REST 接口 v1

默认地址 `http://127.0.0.1:8080`，UTF-8 JSON。局域网启动用 `scripts/start.ps1 -Lan`。服务用于可信课程实验网络，不包含账号系统或互联网部署配置。请求体最多 64KiB。

| 方法 | 路径 | 用途 |
| --- | --- | --- |
| GET | `/api/health` | 健康与版本 |
| GET | `/api/state` | 地图、时钟、车辆、最近 100 笔订单、指标、事件、设备状态 |
| GET | `/api/map` | 固定地图、障碍、米/格比例、版本 |
| POST | `/api/control` | `paused`、`speed`（0.5–15）、`strategy` |
| POST | `/api/reset` | `seed`、`strategy`，清空世界并使设备会话失效 |
| POST | `/api/orders` | 手动添加一笔有效道路起终点订单；现场演示不需要此操作 |
| POST | `/api/compare` | `seed`、`steps`（100–3000）；独立成对基准 |
| POST | `/api/devices/register` | 注册 `kind: matlab` 或 `kind: phone` |
| POST | `/api/devices/matlab` | 实际连续位姿与指令回报，返回车辆和下一个指令 |
| POST | `/api/devices/phone` | 手机 ENU 位姿、传感器与可选 GNSS |
| GET | `/api/export` | 导出完整订单历史与世界快照 |

错误输入返回 `400 {"error":"说明"}`，无效会话返回 401，过大请求由 HTTP 库返回 413。设备会话用于区分客户端和重置后的旧包，不替代互联网身份验证；默认一个 MATLAB 和一个手机设备，新注册替换同类旧会话。

## MATLAB

请求：

```json
{"kind":"matlab"}
```

注册响应含 `session`、`id: M01`、`map` 和 `vehicle`。车辆关键字段：

```json
{
  "id":"M01", "kind":"matlab", "cell":{"x":0,"y":1},
  "position":{"x":0.0,"y":1.0}, "yaw":0,
  "waypoint":{"x":1,"y":1}, "command_id":1,
  "has_command":true, "order":1, "connected":true
}
```

实际数值由注册时世界决定；不要硬编码示例位置。

每 100ms 发布：

```json
{
  "session":"注册返回的字符串", "seq":1, "command_id":1,
  "pose":{"x":0.2,"y":1.0,"yaw":0.0}
}
```

`seq` 必须在本会话严格递增。`command_id` 对应服务端发出的端点，位姿距预约线段不超过 0.16m。到端点距离小于 0.04m 才完成格子推进。响应的 `vehicle` 包含当前指令，`paused` 可供客户端暂停运动；仍应发送心跳。5 秒无消息标为断线，位置和通道保留。401、重置或网络恢复后重新注册，按服务端当前位姿继续，不自行跳到未到达的端点。

## 手机

`POST /api/devices/register`：`{"kind":"phone"}`；响应含 `session`、`id: P01`、`zone: phone-lab`。

```json
{
  "session":"注册返回的字符串", "seq":1,
  "pose":{"east":0.15,"north":-0.03,"yaw":0.2},
  "sensors":{
    "accelerometer":[0.1,0.0,9.81],
    "linear_acceleration":[0.1,0.0,0.0],
    "gyroscope":[0.0,0.0,0.01],
    "magnetic":[25.0,4.0,-38.0],
    "rotation_vector_available":true,
    "linear_acceleration_available":true
  },
  "gps":{}
}
```

单位分别为 m、rad、m/s²、rad/s、μT。有效 GNSS 对象包含 `latitude`、`longitude`、`accuracy`、`timestamp_ms`；没有定位时为空对象。5 秒无消息变为离线，保留最后位姿，不补造轨迹。

## 指标定义

`mean_wait_ticks`：所有已上车订单的 `(picked-arrival)` 均值；`mean_journey_ticks`：已完成订单的 `(finished-arrival)` 均值；`outstanding_wait_ticks`：尚未上车订单截至当前时刻的等待总和；`total_distance`：所有城市车辆已经实际走过的格子边数，每格 1m；`collisions`：独立运动安全检查发现的冲突数量。`yield_events` 是被阻挡等待的车步次数，不是碰撞数。`/api/state` 的订单可能截断，报告应使用 `/api/export`。
