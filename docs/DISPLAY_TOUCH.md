# CPKEXP-EKRA8X1 显示屏与触摸能力

本工程对应的板载模组是 **H0233S001 V1 + ST7796U**。H0233S001 的物理分辨率是
222×480，当前扩展板通过 MIPI-DSI 驱动显示。工程中的 256×480 是 GLCDC/SDRAM
帧缓冲传输宽度，用来满足行跨度和对齐要求，不代表玻璃面板有 256 个可见像素。

## 结论

当前 H0233S001 **不支持触摸**。ST7796U 是 LCD 显示控制器，不是触摸控制器。
扩展板 50 Pin FPC 虽然预留了 TP-SCL、TP-SDA、TP-INT、TP-RST，但在 H0233S001
上这四个信号均为 NC。因此，FSP 引脚配置里出现 `DISP_INT` 或 PCB 上出现 TP
丝印，并不能说明当前安装的屏幕可以触摸。

官方手册给出的带触摸替代型号为 **HS0233S001T001**。由于触摸信号与 MIPI
第二数据通道复用，手册要求它配合不带 MIPI 的核心板并使用 RGB/SPI 显示方式；
更换模组后还需要按手册重新设置 J5 跳线，并增加软件 I2C、触摸中断、复位、
坐标校准和 UI 命中测试，不能只增加一个触摸驱动文件。

## 当前工程策略

- 板端页面由 S1 短按和串口命令切换，不显示虚假的触摸按钮。
- 背光初始化后保持常亮，页面刷新前再次确认背光使能。
- 不读取悬空的 TP 管脚，避免随机中断或误触。
- 如果以后换成触摸模组，应先单独做 I2C 扫描和原始坐标测试，再接入状态机。

资料来源：

- [Renesas CPKEXP-EKRA8X1 显示和摄像头接口说明](https://github.com/renesas/cpk_examples/blob/master/cpkexp_ekra8x1/docs/02_displaycam.adoc)
- [Renesas CPK 示例代码仓库](https://github.com/renesas/cpk_examples)

