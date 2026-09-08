# 工业电机远程监控客户端

基于 Qt 6 的工业电机远程监控客户端，支持多设备独立 TCP 连接、实时遥测、规则化故障诊断、告警管理和历史数据追溯。

## 技术栈

- **C++17** + **Qt 6.8** (Widgets / Network / SQL)
- **SQLite** 持久化存储 (WAL 模式)
- **CMake** 模块化构建系统
- **GoogleTest** 单元测试与网络集成测试 (70+ 测试用例)
- **双协议栈**: 自定义二进制协议 + Modbus TCP
- **Docker** 容器化部署
- **GitHub Actions** CI/CD 自动化

## 快速开始

### 环境要求

- Windows 10/11 / Ubuntu 22.04+
- Qt 6.8+ (MinGW / GCC)
- CMake 3.21+
- Git

### 构建运行

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --target all --parallel
./build/src/client/motor_monitor_client
```

Windows 使用 Visual Studio 或 Qt 的 MinGW 工具链执行同样的 CMake 命令；运行时请确保 Qt、MinGW 和 CMake 的 `bin` 目录已加入 `PATH`。

### 运行测试

```bash
ctest --test-dir build --output-on-failure
```

也可以单独运行关键测试：

```bash
# SQLite WAL 持久化测试
./build/tests/unit/persistence_test

# 100 台模拟设备并发连接测试
./build/tests/integration/network_integration_test \
  --gtest_filter=NetworkIntegrationTest.SupportsOneHundredSimultaneousDevices
```

### Docker 部署

```bash
docker-compose up --build
```

## 架构

```
UI 线程 ──信号/槽──► 通信线程 ──信号/槽──► 存储线程
(Widgets)           (QTcpSocket)          (SQLite)
```

## 功能模块

| 模块 | 说明 |
|------|------|
| 设备管理 | 100 台设备列表、搜索筛选、批量生成、配置持久化 |
| 实时监控 | 温度/转速/电流/电压/振动，QPainter 自绘曲线 |
| 告警引擎 | 8 条规则，完整生命周期（激活/确认/恢复/关闭） |
| 远程控制 | 启动/停止/急停/调速，按钮状态联动 |
| 诊断展示 | 实际值 vs 阈值，告警依据 + 最近控制结果 |
| 历史查询 | 4 类查询，分页 + CSV 导出 |
| 运行日志 | 分级/分类/设备筛选，10 MiB 滚动保存 |
| 性能观测 | 消息速率/写入速率/错误数/重连次数 |
| 模拟器 | 电机物理模型，故障注入，协议层故障 |
| 协议适配 | 策略模式，支持自定义协议 / Modbus TCP |

## 已验证能力

- SQLite 已开启 WAL（Write-Ahead Logging）模式，并配置 5 秒写入等待超时。
- 网络集成测试已验证 100 个客户端同时连接本地模拟器，全部完成上线并收到遥测数据。
- GitHub Actions 已配置 Linux 环境下的 Qt、CMake 构建和 GoogleTest 执行。

## 设计模式

| 模式 | 应用 |
|------|------|
| 策略模式 | IProtocolAdapter → Custom / ModbusTCP |
| 工厂模式 | createAdapter(type) |
| 观察者模式 | Qt 信号/槽跨线程 |
| 仓储模式 | Repository → SqliteRepository |
| 依赖注入 | Worker 注入 MainWindow |

## 项目结构

```
├── src/
│   ├── client/          # 客户端
│   │   ├── core/         # JSON 配置
│   │   ├── network/      # 通信层 (Worker + Connection)
│   │   ├── ui/           # 界面层 (MainWindow + 面板)
│   │   └── main.cpp
│   ├── common/          # 公共库
│   │   ├── protocol/     # 协议编解码 + 适配器
│   │   ├── domain/       # 领域类型
│   │   ├── alarm_engine/ # 告警引擎
│   │   └── persistence/  # SQLite 仓储
│   └── simulator/       # 电机模拟器
├── tests/               # 单元测试 + 集成测试
├── cmake/               # CMake 配置
├── .github/workflows/   # CI/CD
├── Dockerfile
├── docker-compose.yml
└── CMakeLists.txt
```

## 配置文件

首次运行自动生成 `config.json`，支持自定义：

```json
{
    "autoStartSimulator": true,
    "batchGenerateCount": 100,
    "defaultHost": "127.0.0.1",
    "defaultPort": 9000,
    "devicePresets": [],
    "logLevel": "Info"
}
```
