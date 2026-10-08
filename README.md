# RA8D1 + BMI088 振动/温度融合端侧 AI

> 显示硬件说明：当前 H0233S001 是 222×480 的非触摸屏；工程使用 256 像素
> 帧缓冲跨度满足 GLCDC 对齐。触摸能力、预留管脚和替换条件见
> [docs/DISPLAY_TOUCH.md](docs/DISPLAY_TOUCH.md)。

面向 **CPKCOR-RA8D1B + CPKEXP-EKRA8X1** 的非视觉端侧 AI 项目。BMI088 以 800 Hz 采集三轴加速度和三轴角速度，并读取片内温度；RA8D1 在本地完成 512 点窗口、26 维特征提取、五分类或一类异常检测，以及 256×480 MIPI 屏状态机显示。运行时不需要摄像头、网络或云服务。

## 已实现

- BMI088 SPI1 双片选驱动、800 Hz 定时采样和温度融合。
- 无训练权重时使用现场正常工况校准的一类异常检测；导入权重后切换五分类 MLP。
- ST7796U / H0233S001 V1 MIPI DSI 屏，RGB565 双帧缓冲位于板载 SDRAM，完整帧在垂直同步处切换，避免边扫描边绘制造成的撕裂。
- 事件驱动、无延时的 UI 状态机：总览、波形、AI 诊断和事件历史四页。
- DWT 周期计数得到端侧推理耗时；显示置信度、Top-3 异常特征和模型元数据。
- 3 个连续异常窗口确认告警、5 个正常窗口恢复，降低告警抖动。
- 最近 8 次健康状态变化的环形事件记录。
- SCI3 上位机机器协议：状态/AI 结果、64 点实时波形和 Top-3 异常解释可统一接入桌面仪表盘。

## 必须修改的 BMI088 接线

启用显示屏后需要启用扩展板 SDRAM，**D9/PA07 会成为 SDRAM_DQ21，不能再作为陀螺仪片选**。请把原先接在 D9 的陀螺仪 CS 线移动到 **D3/P907**。

| BMI088 模块 | 扩展板 | RA8D1 | 用途 |
|---|---|---|---|
| VCC / VDD / VDDIO | 3V3 | — | 只使用 3.3 V 逻辑 |
| GND | GND | — | 共地 |
| SCK / SCL | Arduino D13 | P412 | SPI1 RSPCK |
| SDO / MISO | Arduino D12 | P410 | SPI1 MISO |
| SDI / MOSI / SDA | Arduino D11 | P411 | SPI1 MOSI |
| CSB1_A / ACC_CS | Arduino D8 | P504 | 加速度计片选，低有效 |
| CSB2_G / GYRO_CS | **Arduino D3** | **P907** | 陀螺仪片选，低有效 |
| PS（若引出） | GND | — | 选择 SPI；部分模块已板载处理 |

串口继续使用 SCI3、115200-8-N-1：USB 转串口 RXD 接 D1/P409，TXD 接 D0/P408，GND 共地，VCC 不接。若使用板载 J-Link OB 虚拟串口，直接使用设备管理器中的 `COM16`（本机实测）；不需要额外 USB-UART。核心板蓝色 LED P600 用于健康状态，低电平点亮。SCI3 时钟必须选择 PLL1R/2，本工程已固定为该配置，避免生成 100% 波特率误差。

## 屏幕与交互

屏幕参数来自同一扩展板的厂商示例：H0233S001 V1、ST7796U、256×480、RGB565、MIPI DSI。工程只移植必需的 SDRAM、GLCDC、MIPI PHY/DSI 和面板初始化，没有引入完整 Arm-2D 跑分工程。当前板端渲染器内置的是紧凑型 **ASCII 5×7 点阵字库**，因此板载页面使用高对比度英文缩写；中文界面由 PC 上位机提供。若要让板载屏显示中文，需另行加入经过授权的中文点阵/矢量字库及 UTF-8 解码，不能只把字符串改成中文。

当前 UI 包含总览、64 点波形、Top-3 异常贡献和最近事件四页；状态机还会显示“需要基线”“校准失败”和“BMI088 故障”等覆盖提示。总览的大号温度、异常分数、类别和健康状态适合实机演示；AI 页显示真实基线窗口数，事件页显示 3 窗确认/5 窗恢复的迟滞进度。

| 操作 | 功能 |
|---|---|
| S1 短按 | 总览 → 波形 → AI 诊断 → 事件历史 |
| S1 按住至少 0.8 秒 | 建立 16 个正常窗口的异常检测与温度基线 |
| `u` | 切换屏幕页面 |
| `i` | 串口输出模型、推理耗时和 Top-3 异常特征 |

校准时保持被测设备处于稳定的正常工况。采集窗口约 0.64 秒；UI 状态机本身不使用阻塞延时，采样与校准仍按确定时序执行。

## 串口命令

| 命令 | 功能 |
|---|---|
| `0`…`4` | 采集 normal / imbalance / loose / rub / bearing 标注窗口 |
| `b` | 建立 16 个正常窗口的一类异常基线 |
| `m` | 开关连续端侧监测 |
| `p` | 输出一个 26 维融合特征向量 |
| `t` | 立即读取 BMI088 温度 |
| `u` | 切换 UI 页面 |
| `i` | 输出可观测性与解释信息 |
| `q` | 只读输出上位机状态、波形和 Top-3 快照，不触发新采样或推理 |
| `h` | 帮助 |

`RESULT` 行包含窗口号、异常分数、类别、置信度、温度、温升、温度级别、经过迟滞处理的健康状态、推理微秒数和模型版本。

上电时固件发送 `PROTO,NAME,RA8D1_BMI088_EDGE_AI,VERSION,1,TRANSPORT,UART,BAUD,115200`，上位机据此自动选择振动 AI 页面。机器协议的字段、波形 S8HEX 编码和轮询约束见 [`docs/HOST_PROTOCOL.md`](docs/HOST_PROTOCOL.md)。旧 `RESULT/DIAG/EXPLAIN` 输出及原有命令保持兼容。

## 随工程附带的上位机

`host_app/` 包含 RA8D1 Edge AI Studio 的源码、测试和独立 Windows 程序。
双击工程根目录的 `启动上位机.bat`，或直接运行
`host_app/dist/RA8D1_EDGE_AI_STUDIO/RA8D1_EDGE_AI_STUDIO.exe`。
连接开发板串口后，上位机会根据设备协议自动选择 BMI088 振动 AI 页面。

源码运行：在 `host_app` 目录执行 `python -m pip install -r requirements.txt`，
再执行 `run_host.bat`。执行 `run_host.bat --demo bmi` 可查看无需硬件的振动 AI 演示。
完整说明见 [host_app/README.md](host_app/README.md)。

## 编译和首次检查

1. 在 e² studio 导入本目录，工具链使用 LLVM 21.1.1，FSP 使用 6.4.0。
2. 打开 `configuration.xml`，目标器件应为 `R7FA8D1BHECBD`。只有需要修改引脚或 FSP 外设时才重新生成；本次 UI 修改不需要重生成，也不要手工改 `ra/`、`ra_cfg/`、`ra_gen/`。
3. 编译 `Debug` 并下载 `Debug/ra8d1_bmi088_vibration_ai.elf`。
4. 上电后串口应先看到：

   ```text
   PROTO,NAME,RA8D1_BMI088_EDGE_AI,VERSION,1,TRANSPORT,UART,BAUD,115200
   DISPLAY,ST7796U,READY,256x480,RGB565
   BMI088,INIT,0,ACC_ID,0x1E,GYRO_ID,0x0F
   MODEL,NOT_TRAINED,VERSION,vib-temp-1.1
   ```

若屏幕不亮，先检查屏排线方向、扩展板供电和串口中的 `DISPLAY` 状态。若 BMI088 初始化失败，重点检查 D8/P504 与新接线 D3/P907 两个 CS，以及 SPI 三根线；未接 BMI088 时应看到 `INIT,-3`、`ACC_ID,0x00`、`GYRO_ID,0x00` 和 `BMI088 SENSOR ERROR`，这是安全保护路径，不是下载失败。

首次运行建议先短按 S1 检查四页切换，再让设备保持稳定正常工况并长按 S1 0.8 秒。16 个窗口约需 10.2 秒；校准完成后串口发送 `m` 开启连续监测。五分类权重尚未训练时，界面显示 `ANOMALY ONLY`/`ONE-CLASS FALLBACK` 是正常现象，并不代表五分类已经可用。

## 训练五分类模型

一类异常检测可以直接现场校准。五分类模型必须使用你的实物在不同转速、负载、温度和安装方式下采集的数据训练：

```powershell
python -m pip install -r tools/requirements.txt
python tools/train_vibration_model.py logs/run1.log logs/run2.log
```

脚本输出混淆矩阵和分类报告，并更新 `src/vibration/vibration_model_data.h`。不要在人员靠近的高速设备上人为制造松动、摩擦或危险偏心。

## 代码结构

- `src/display/`：SDRAM、ST7796U 和 MIPI/GLCDC 显示适配。
- `src/ui/`：显式页面状态机与轻量 RGB565 渲染器。
- `src/edge_ai/`：告警迟滞、事件历史、模型元数据和异常解释。
- `src/bmi088/`：RA8D1 SPI 与双片选适配。
- `src/vendor/bmi08x/`：Bosch BMI08x SensorAPI 及许可证。
- `src/vibration/`：特征、异常模型、分类推理和模型权重。
- `src/temperature/`：温度滤波、基线与阈值。
- `docs/EDGE_AI_PORTFOLIO.md`：作品集表达、验证方案和后续量化路线。
- `docs/HOST_PROTOCOL.md`：统一上位机 v1 遥测、波形、解释与控制协议。

## Windows 统一上位机

`ra8d1_canfd_edge_ids/host_app` 已升级为统一的 **RA8D1 Edge AI Studio**。连接 SCI3 后，它会按 `PROTO` 自动识别本 BMI088 工程并切换到机器健康、波形、特征解释和事件页面；也能不接板子运行 BMI088 Demo。板端仍独立完成全部采样和推理。

```powershell
cd C:\Users\34542\Desktop\端侧AI与车辆嵌入式项目\ra8d1_canfd_edge_ids\host_app
python -m pip install -r requirements.txt
python main.py --demo bmi
```
