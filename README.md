# 工业电机远程监控系统

基于 Qt 6 的工业电机远程监控客户端，支持 100 台设备独立 TCP 连接、实时遥测、规则化故障诊断、告警管理和历史数据追溯。

## 技术栈

- **C++17** + **Qt 6.8** (Widgets / Network / SQL)
- **SQLite** 持久化存储
- **CMake** 构建系统
- **GoogleTest** 单元测试
- **自定义二进制协议** (长度帧 + CRC32)

## 快速开始

### 环境要求

- Windows 10/11
- Qt 6.5+ (含 MinGW 编译器)
- CMake 3.21+
- Git

### 构建

```bash
# 克隆项目
git clone <repo-url>
cd MotorMonitor

# CMake 配置
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64" -G "MinGW Makefiles"

# 编译
cmake --build build --config Debug
```

### 运行

在 Qt Creator 中打开 `CMakeLists.txt`，直接运行 `motor_monitor_client` 即可。

**客户端会自动启动模拟器**，无需手动操作。

## 功能演示

### 1. 设备连接

```
点击"批量生成" → 创建 10 台设备
选中 MOTOR-0001 → 点击"上线" → 状态变为绿色"在线"
```

### 2. 实时监控

```
点击"启动" → 电机开始运转 → 实时曲线显示温度/转速/电流
全局状态栏显示: 在线: 1/10 | 活动告警: 0 | 速率: 5 帧/秒
```

### 3. 远程控制

```
调节转速 → 点击"设置" → 目标转速更新
点击"停止" → 正常停机
点击"急停" → 紧急停机 (红色警告)
```

### 4. 告警诊断

```
模拟器注入故障 → 告警面板显示告警
点击"确认告警" → 状态变为"已确认"
故障恢复后 → 状态变为"已恢复" → 点击"清除"
```

### 5. 历史查询

```
切换到"历史查询"标签页
选择设备 + 时间范围 → 点击"查询"
支持遥测/告警/控制/事件四种数据
点击"导出CSV" → 导出 UTF-8 CSV 文件
```

## 项目结构

```
src/
├── common/                    # 公共模块
│   ├── domain/                # 领域类型 (device/command/alarm)
│   ├── protocol/              # 二进制协议编解码 + CRC32
│   ├── alarm_engine/          # 告警规则引擎 (8条规则)
│   └── persistence/           # SQLite 仓储
├── client/                    # 客户端
│   ├── network/               # 设备连接管理 (心跳/重连/超时)
│   ├── ui/                    # UI 组件
│   │   ├── main_window.cpp    # 主窗口
│   │   ├── device_list_model.cpp  # 设备列表模型
│   │   ├── realtime_chart.cpp     # 实时曲线图 (QPainter)
│   │   ├── history_page.cpp       # 历史查询 + CSV导出
│   │   ├── log_page.cpp           # 运行日志 (分级/分类/筛选)
│   │   └── device_config_dialog.cpp # 设备配置对话框
│   └── main.cpp               # 入口 (自动启动模拟器)
├── simulator/                 # 模拟器
│   ├── core/motor_model.cpp   # 电机物理模型
│   └── network/               # TCP 服务器 + 会话管理
└── tests/                     # 测试
    └── unit/                  # 56+ 单元测试
```

## 架构

```
┌──────────────┐    TCP (二进制协议)    ┌──────────────────┐
│   客户端 UI   │ ◄──────────────────► │     模拟器        │
│              │                      │                  │
│  ┌────────┐  │  ConnectRequest      │  ┌────────────┐  │
│  │ 设备列表 │  │  Heartbeat          │  │ DeviceServer│  │
│  │ 实时曲线 │  │  TelemetryReport    │  │  ├─Session1 │  │
│  │ 告警面板 │  │  ControlRequest     │  │  ├─Session2 │  │
│  │ 历史查询 │  │  ControlResponse    │  │  └─SessionN │  │
│  │ 运行日志 │  │                      │  └────────────┘  │
│  └────────┘  │                      │                  │
│  ┌────────┐  │                      │  ┌────────────┐  │
│  │ SQLite  │  │                      │  │ MotorModel │  │
│  └────────┘  │                      │  │ (物理模型)  │  │
└──────────────┘                      │  └────────────┘  │
                                      └──────────────────┘
```

## 告警规则

| 规则 | 级别 | 触发条件 | 恢复条件 |
|------|------|----------|----------|
| 高温预警 | 警告 | 温度连续3次 > 85°C | 温度连续3次 < 80°C |
| 严重过热 | 严重 | 温度任意1次 > 100°C | 温度连续3次 < 90°C |
| 过流 | 警告 | 电流连续3次 > 额定值 | 电流连续3次 < 额定90% |
| 堵转 | 严重 | 转速 < 目标10% 且电流 > 额定120% | 转速恢复至目标80% |
| 振动异常 | 警告 | 振动连续5次 > 7.1 mm/s | 振动连续5次 < 6.0 mm/s |
| 电压异常 | 警告 | 电压连续3次 < 200V 或 > 240V | 电压连续3次 205~235V |
| 心跳超时 | 严重 | 5秒无消息 | 重连并收到心跳 |
| 遥测停更 | 警告 | 在线但5秒无遥测 | 收到下一条遥测 |

## 二进制协议

| 字段 | 长度 | 说明 |
|------|------|------|
| Magic | 2 字节 | 0x4D54 |
| Version | 1 字节 | 1 |
| TotalLength | 4 字节 | 含头部+Payload+CRC32 |
| MessageType | 2 字节 | 消息类型 |
| Sequence | 4 字节 | 递增序列号 |
| RequestId | 8 字节 | 请求响应匹配 |
| Timestamp | 8 字节 | Unix 毫秒 |
| Payload | 变长 | 按类型编码 |
| CRC32 | 4 字节 | 帧完整性校验 |

## 运行测试

```bash
cmake --build build --target motor_monitor_tests
ctest --test-dir build --output-on-failure
```

## 作者

MotorMonitor - 工业电机远程监控系统 MVP