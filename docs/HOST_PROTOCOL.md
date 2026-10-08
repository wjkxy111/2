# BMI088 Edge AI 上位机协议 v1

本协议通过 SCI3 `115200-8-N-1` 输出 CRLF 结尾的 ASCII 记录。所有新增记录均为 `TAG,KEY,VALUE,...`，上位机必须忽略未知尾部字段，以便固件继续扩展。

启动声明：

```text
PROTO,NAME,RA8D1_BMI088_EDGE_AI,VERSION,1,TRANSPORT,UART,BAUD,115200
```

## 幂等快照命令

主机发送单字节 `q`（不要附加 CR/LF），固件只读取最近一次已完成推理的缓存，不启动采样、不校准、不改变监测状态。一次响应依次为：

```text
BMI_TELEM,T_MS,1250,WINDOW,17,MODE,MONITOR,MONITOR,ON,SENSOR,READY,MODEL,ONE_CLASS_FALLBACK,HEALTH,WARN,SCORE_X100,975,CLASS,anomaly_only,CONF_PM,0,TEMP_MC,62400,RISE_MC,33800,TEMP_LEVEL,WARN,INFER_US,41,BASE_WINDOWS,16,EVENTS,3
BMI_WAVE,WINDOW,17,ENC,S8HEX,POINTS,64,DATA,<128 HEX chars>
BMI_EXPLAIN,WINDOW,17,RANK,1,FEATURE,acc_kurtosis,CONTRIB_X100,470
BMI_EXPLAIN,WINDOW,17,RANK,2,FEATURE,acc_crest,CONTRIB_X100,318
BMI_EXPLAIN,WINDOW,17,RANK,3,FEATURE,spec_250,CONTRIB_X100,188
```

- `MODE`：`IDLE`、`MONITOR` 或 `CALIBRATE`。
- `MODEL`：`TRAINED` 表示 26→12→5 MLP 权重已导入；`ONE_CLASS_FALLBACK` 表示使用现场 16 个正常窗口建立的一类统计基线。
- `SCORE_X100`：异常分数乘 100，实际范围 `0..2500`；一类模型报警阈值为 `900`（即 9.00）。
- `CONF_PM`：五分类置信度千分比 `0..1000`；一类回退模式下固定为 0。
- `TEMP_MC`、`RISE_MC`：毫摄氏度；温度告警边界为 60/70 °C 或温升 15/25 °C。
- `HEALTH`：融合迟滞后的 `OK`、`WARN`、`ALARM` 或 `SENSOR`。
- `BMI_WAVE`：64 个有符号 int8 二补码样本，每点两个十六进制字符，范围在当前固件中归一化到约 `-100..100`。
- `BMI_EXPLAIN`：一类模型中贡献最大的特征 z²（乘 100），不是神经网络 SHAP 值。

建议主机以 2 Hz 轮询。连续监测本身还会输出旧版 `RESULT`，主机可按 `WINDOW` 去重。

板载 UI 的双缓冲、页面切换和错误提示不会改变任何协议字段或命令。当前板载字库仅支持 ASCII；中文标签属于 PC 上位机显示层，不应写入本协议的机器字段。

## 向后兼容记录

固件保留已有 `RESULT`、`FEATURES`、`TEMP_NOW`、`EXPLAIN`、`CALIBRATION`、`BEGIN/TEMP/COLUMNS/DATA/END` 和 `ERROR` 记录，便于原有采集/训练工具继续使用。标注采集命令 `0..4` 会同步输出 512 行原始数据，不应高频点击。

## 控制命令

| 命令 | 功能 |
|---:|---|
| `q` | 读取缓存快照，不触发推理 |
| `b` | 采集 16 个正常窗口并校准一类模型/温度基线 |
| `m` | 开关连续监测 |
| `p` | 新采一窗并输出 26 维特征 |
| `t` | 读取当前 BMI088 温度 |
| `0..4` | 采集 normal / imbalance / loose / rub / bearing 标注窗口 |
| `u` | 切换板载 LCD 页面 |
| `i` | 输出模型、延迟和 Top-3 解释 |
| `h` | 输出帮助 |

## 工程边界

板端负责采样、特征、推理、融合与告警；PC 上位机只负责可视化、命令、录制和回放。断开电脑后端侧 AI 仍可运行。当前 `q` 的 UART 输出为同步阻塞发送，适合低频观测，不是高速原始传感器流接口。
