# 夜间智能避障系统

基于 STM32F103C8T6 的嵌入式期末大作业——当环境变暗时自动开启超声波测距，在 OLED 上显示障碍物距离，实现夜间避障功能。

## 功能

- **日间模式**：光敏传感器检测到光线充足 → 超声波待机省电
- **夜间模式**：光敏传感器检测到黑暗 → 自动开启 HC-SR04 超声波测距 → OLED 显示距离
- **危险警告**：障碍物距离 < 30cm 时显示 `>> DANGER! <<` 警告

## 硬件模块

| 模块 | 型号 | 用途 |
|------|------|------|
| 主控 | STM32F103C8T6 | 系统核心 |
| 显示屏 | SSD1306 OLED 128×64 (I2C) | 显示模式与距离 |
| 光敏传感器 | 光敏电阻 + LM393 | 检测环境明暗 |
| 超声波 | HC-SR04 | 夜间测距 |

## 引脚连接

| STM32引脚 | 连接至 | 功能 |
|:---------:|--------|------|
| PA0 | 光敏模块 DO | ADC1_CH0 读取光敏电压 |
| PB0 | HC-SR04 TRIG | 触发脉冲输出 |
| PB1 | HC-SR04 ECHO | TIM3_CH4 输入捕获测距 |
| PB6 | OLED SCL | 软件 I2C 时钟 |
| PB7 | OLED SDA | 软件 I2C 数据 |
| PC13 | 板载 LED | 夜间指示灯 |

> 详细接线图见 [硬件连接手册.pdf](43/1/user/硬件连接手册.pdf)

## 开发环境

- **IDE**：Keil MDK-ARM
- **库**：STM32F10x Standard Peripheral Library V3.5.0
- **启动文件**：startup_stm32f10x_md.s（中容量）
- **下载器**：ST-Link / J-Link

## 项目结构

```
43/1/
├── user/
│   ├── main.c              # 主程序（明暗判断 + 测距 + OLED显示）
│   ├── hc_sr04.h           # HC-SR04 超声波驱动头文件
│   ├── hc_sr04.c           # HC-SR04 超声波驱动（TIM3 输入捕获）
│   ├── oled.h              # SSD1306 OLED 驱动头文件
│   ├── oled.c              # SSD1306 OLED 驱动（软件 I2C）
│   ├── stm32f10x_it.h      # 中断服务头文件
│   ├── stm32f10x_it.c      # 中断服务（SysTick + TIM3）
│   └── stm32f10x_conf.h    # 标准库配置文件
├── Library/                # STM32F10x 标准外设库
│   └── stm32f10x_*.c/h
├── start/                  # 启动文件 + 系统文件
│   ├── startup_stm32f10x_md.s
│   ├── system_stm32f10x.c/h
│   └── core_cm3.c/h
└── project.uvprojx         # Keil 工程文件
```

## 系统工作流程

```
启动 → 初始化各模块
  ↓
主循环（200ms/次）
  ├── 读取光敏 ADC（PA0）→ 换算电压
  ├── 电压 < 1.5V（黑暗）
  │   ├── LED 亮
  │   ├── 触发 HC-SR04 超声波测距
  │   ├── 等待 ECHO 回波（超时 100ms）
  │   ├── 计算距离 = 脉宽/58 cm
  │   └── OLED: 夜间模式 + 距离 + 警告
  └── 电压 ≥ 1.5V（明亮）
      ├── LED 灭
      ├── 超声波待机
      └── OLED: 日间模式
```
