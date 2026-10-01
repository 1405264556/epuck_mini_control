# 多机器人协调控制接口与演进方案

本文档面向后续编队控制、分布式优化、任务分配和安全避障算法开发。当前上位机使用 20 Hz 统一控制周期，每台在线机器人拥有独立串口线程，算法层不直接操作串口，只读取世界状态并输出轮速命令。

## 当前控制链路

```text
每机器人串口线程 -> RobotManager 状态快照 -> MultiRobotCoordinator (20 Hz)
                                                |
                                                +-> 内置编队
                                                +-> 内置群集
                                                +-> C++ 策略插件
                                                +-> 外部进程适配器（协议已定义）
                                                |
                                  轮速限幅、在线过滤、统一下发
```

0.5.0 的批量轮速由每端口调度器按共同单调时钟截止时间下发，旧指令合并，零速优先。编码器反馈驱动位姿，超过 1 秒未更新会暂停自动控制，参与设备断线则停止。手动控制会退出当前自动算法。

原生策略的 step() 当前仍在主线程同步执行，必须短于 40 ms。耗时优化应由插件自行在独立工作线程计算，step() 只返回最新完成解；外部 Python/MATLAB 协调进程适配器尚未实现。主机共同截止时间也不保证固件端硬件级同步。详见 [通信与验收](communication_and_validation.md)。

## 策略选择建议

| 策略 | 适用任务 | e-puck 部署建议 |
| --- | --- | --- |
| 领航者-跟随者 | 队列移动、跟随实验 | 第一阶段实现，参数少，容易实机调试 |
| 虚拟结构/刚性编队 | 横列、圆形、V 形等队形保持 | 适合上位机集中式计算，需要可靠位姿 |
| 图一致性 | 朝向、速度、位置偏差趋同 | 用邻接表表达通信拓扑，逐步改为分布式 |
| Reynolds/Boids | 分离、对齐、聚合和目标跟随 | 适合群集行为演示，必须叠加安全层 |
| ORCA/RVO | 多机器人局部碰撞避免 | 计算快，适合二维差速机器人局部避障 |
| CBF/QP 安全过滤 | 对任意控制器进行最小修改并满足安全约束 | 适合作为编队、群集或学习控制器之后的统一安全层 |
| CBBA/拍卖分配 | 多机器人多任务分配 | 先做上位机集中仿真，再加入邻居消息和异步冲突消解 |
| 分布式 MPC | 带动力学、输入和耦合约束的协同轨迹优化 | 计算量大，建议 Python/MATLAB 原型后转 C++ 求解器 |

参考资料：

- [Olfati-Saber 与 Murray：切换拓扑和时延下的一致性问题](https://authors.library.caltech.edu/records/t2gnt-vd720)
- [ORCA：Optimal Reciprocal Collision Avoidance](https://gamma-web.iacs.umd.edu/ORCA/)
- [差速机器人约束下的 ORCA](https://gamma-web.iacs.umd.edu/ORCA-DD/ORCA-DD.pdf)
- [MIT ACL：Consensus-Based Bundle Algorithm](https://acl.mit.edu/projects/consensus-based-bundle-algorithm)
- [在线多机器人轨迹生成与分布式 MPC](https://arxiv.org/abs/1909.05150)
- [多机器人异构系统 Safety Barrier Certificates](https://arxiv.org/abs/1609.00651)
- [通信时延下的分布式安全控制](https://arxiv.org/abs/2402.09382)

## C++ 插件接口

接口定义位于 `src/control/IMultiRobotStrategy.h`，插件动态库放在可执行文件同级的 `algorithms/` 目录。

每个控制周期输入 `MultiRobotWorldState`：

- `timestamp`：上位机毫秒时间戳。
- `deltaTime`：控制周期，当前默认 0.05 秒。
- `agents`：参与算法的在线机器人 ID、位置、航向和传感器快照。
- `neighbors`：邻接拓扑，用于一致性、分布式优化和局部通信算法。
- `parameters`：界面或配置文件传入的目标点、间距和算法参数。

插件返回 `MultiRobotStrategyOutput`：

- `wheelSpeeds`：各机器人归一化左右轮速度，范围 `[-1, 1]`。
- `targetPositions`：可选的目标位置，用于画布显示和调试。
- `diagnostics`：可选诊断数据，如代价函数、迭代次数和收敛残差。

插件类需要继承 `QObject` 和 `IMultiRobotStrategy`，并声明：

```cpp
Q_PLUGIN_METADATA(IID IMultiRobotStrategy_iid)
Q_INTERFACES(IMultiRobotStrategy)
```

控制周期预算为 50 ms。插件单次 `step()` 超过 40 ms 时，上位机会发出告警；需要迭代求解的算法应使用异步外部进程适配器。

## Python/MATLAB 外部进程协议

外部算法采用一行一个 JSON 对象的 NDJSON 长驻进程协议。上位机发送：

```json
{"type":"step","sequence":17,"timestamp":1760000000000,"dt":0.05,"robots":[{"id":"COM:3","pose":[0.0,0.0,0.0]},{"id":"COM:5","pose":[20.0,0.0,0.0]}],"neighbors":{"COM:3":["COM:5"],"COM:5":["COM:3"]},"parameters":{"goal":[100.0,50.0],"spacing_cm":20.0}}
```

算法返回同一 `sequence` 的命令：

```json
{"type":"command","sequence":17,"commands":[{"id":"COM:3","left":0.35,"right":0.31},{"id":"COM:5","left":0.28,"right":0.36}],"diagnostics":{"residual":0.012}}
```

适配器应遵守以下规则：

- 只允许输出输入中存在且在线的机器人 ID。
- 左右轮速度必须为有限数值，范围 `[-1, 1]`。
- 同一时刻只保留一个待处理周期；算法未及时返回时跳过新周期，不累积控制延迟。
- 超过 150 ms 没有新命令时发送零速，超过 1 秒时停止进程并报告错误。
- MATLAB 建议使用 Engine API 或长驻 `-batch` 服务封装，禁止每 50 ms 重新启动 MATLAB。

示例骨架位于 `examples/algorithms/`。

## 推荐实施顺序

1. 完成多机器人位姿来源。当前画布位置包含里程估计，正式编队建议接入顶视相机、AprilTag、UWB 或融合定位。
2. 先验证同步控制、独立急停、断线保护和命令超时，确保单机异常不会拖住其他机器人。
3. 部署集中式领航者-跟随者或虚拟结构，并使用 ORCA/CBF 作为安全过滤层。
4. 引入邻接拓扑、消息时间戳和丢包/时延仿真，验证图一致性算法。
5. 增加任务模型后接入 CBBA；增加轨迹约束后接入 DMPC。
6. 所有新算法先在仿真和低速模式验证，再逐步提高速度和机器人数量。
