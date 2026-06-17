/**
  ******************************************************************************
  * @file    user/main.c
  * @author
  * @version V3.0
  * @date    2026-06-17
  * @brief   夜间智能避障系统 — 主程序
  *
  *          功能：
  *          - 光敏传感器（ADC1 CH0, PA0）：检测环境明暗
  *          - HC-SR04 超声波（PB0/PB1, TIM3）：夜间测距
  *          - SSD1306 OLED（I2C, PB6/PB7）：显示状态与距离
  *          - PC13 LED：夜间亮 / 日间灭
  *
  *          逻辑：
  *          - 明亮（电压 >= 1.5V）→ 日间模式：超声波关闭、LED 灭
  *          - 黑暗（电压 <  1.5V）→ 夜间模式：超声波测距、显示距离、
  *            距离 < 30cm 时显示危险警告
  *
  *          硬件平台：
  *          - 主控: STM32F103C8T6（中容量）
  *          - 库:   STM32F10x 标准外设库 V3.5.0
  *          - IDE:  Keil MDK-ARM
  ******************************************************************************
  */

/* 包含头文件 ----------------------------------------------------------------*/
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_adc.h"
#include "misc.h"
#include "oled.h"
#include "hc_sr04.h"

/* 宏定义 -------------------------------------------------------------------*/
#define LED_PORT                GPIOC
#define LED_PIN                 GPIO_Pin_13         /* PC13: 板载 LED，低电平亮 */

#define VOLTAGE_THRESHOLD       1.5f                 /* 光敏电压阈值: < 1.5V = 黑暗 */
#define DANGER_DISTANCE_CM      30.0f                /* 障碍物警告距离: 30cm */
#define MEASURE_TIMEOUT_MS      100UL                /* 超声波回波超时等待 */
#define MAIN_LOOP_DELAY_MS      100UL                /* 主循环刷新间隔 */

/* 全局变量 — 与 ISR 共享 ----------------------------------------------------*/
volatile uint32_t g_ms_tick = 0;                     /* 1ms 系统时基计数器 */

/* 私有函数声明 --------------------------------------------------------------*/
static void Delay_ms(uint32_t ms);
static void SysTick_Init(void);
static void LED_Init(void);
static void ADC1_Init(void);
static uint16_t ADC1_Read(void);
static void Display_NightMode(float distance);
static void Display_DayMode(void);

/**
  * @brief  主函数入口
  */
int main(void)
{
    uint16_t adc_value;
    float    voltage;
    float    distance;
    uint8_t  is_night = 0;                           /* 前一时刻的模式标志，用于边沿切换 */
    uint32_t timeout;

    /* ===== 系统初始化 ===== */
    SysTick_Init();                                  /* 1ms 时基，用于延时 */
    OLED_Init();                                     /* SSD1306 OLED（I2C PB6/PB7） */
    LED_Init();                                      /* PC13 板载 LED */
    ADC1_Init();                                     /* ADC1 CH0（PA0）光敏传感器 */
    HC_SR04_Init();                                  /* 超声波: PB0(TRIG), PB1(ECHO), TIM3 */

    /* 显示启动画面 */
    OLED_Clear();
    //OLED_ShowString(16, 20, "Night Avoid", 8);
    //OLED_ShowString(16, 36, "System Ready", 8); 
    OLED_Refresh();
    Delay_ms(1000);

    /* ===== 主循环 ===== */
    while (1)
    {
        /* 步骤1: 读取光敏传感器（ADC1 CH0 → PA0） */
        adc_value = ADC1_Read();
        voltage   = (float)adc_value * 3.3f / 4095.0f;

        /* 步骤2: 根据电压判断日间/夜间模式 */
        if (voltage >= VOLTAGE_THRESHOLD)
        {
            /* ================================================================
             * 夜间模式 — 检测到黑暗环境
             * - 点亮 LED 指示灯
             * - 启动超声波距离测量
             * - 显示距离及障碍物警告
             * ================================================================ */

            /* 进入夜间: LED 亮（低电平有效） */
            if (!is_night)
            {
                GPIO_ResetBits(LED_PORT, LED_PIN);
                is_night = 1;
            }

            /* 触发 HC-SR04 测量（发送 20μs TRIG 脉冲） */
            HC_SR04_StartMeasure();

            /* 等待测量完成，带超时退出（防止 ECHO 无回波时死等） */
            timeout = g_ms_tick;
            while (!HC_SR04_IsDataReady())
            {
                if ((g_ms_tick - timeout) >= MEASURE_TIMEOUT_MS)
                {
                    break;  /* 超时: 无回波信号 */
                }
            }

            /* 读取测距结果 */
            distance = HC_SR04_GetDistance();

            /* 刷新 OLED 显示 */
            Display_NightMode(distance);
        }
        else
        {
            /* ================================================================
             * 日间模式 — 环境光线充足
             * - 熄灭 LED 指示灯
             * - 超声波模块待机（省电，不发送触发脉冲）
             * - 显示日间状态
             * ================================================================ */

            /* 进入日间: LED 灭 */
            if (is_night)
            {
                GPIO_SetBits(LED_PORT, LED_PIN);
                is_night = 0;
            }

            /* 刷新 OLED 显示 */
            Display_DayMode();
        }

        /* 步骤3: 延时，保证显示刷新稳定 */
        Delay_ms(MAIN_LOOP_DELAY_MS);
    }
}

/* ===== 系统函数 ============================================================*/

/**
  * @brief  初始化 SysTick 定时器，产生 1ms 中断
  * @note   HCLK = 72MHz，SysTick 重装载值 = 72000
  */
static void SysTick_Init(void)
{
    if (SysTick_Config(SystemCoreClock / 1000))
    {
        /* 配置失败 — 不应发生 */
        while (1);
    }
}

/**
  * @brief  利用 SysTick 计数器实现毫秒级延时
  * @param  ms: 延时毫秒数
  */
static void Delay_ms(uint32_t ms)
{
    uint32_t start = g_ms_tick;
    while ((g_ms_tick - start) < ms);
}

/* ===== LED 控制 ============================================================*/

/**
  * @brief  初始化 PC13 板载 LED（推挽输出，默认熄灭）
  * @note   PC13 = 高电平 → LED 灭；PC13 = 低电平 → LED 亮（低电平驱动）
  */
static void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = LED_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &GPIO_InitStructure);

    /* 默认: LED 熄灭 */
    GPIO_SetBits(LED_PORT, LED_PIN);
}

/* ===== ADC / 光敏传感器 ====================================================*/

/**
  * @brief  初始化 ADC1 通道0（PA0）用于光敏传感器采样
  *         单次转换模式，软件触发
  * @note   光敏模块 DO 引脚 → PA0，输出模拟电压
  *         遮盖（黑暗）: 电压 ≈ 0 ~ 1.5V
  *         光照（明亮）: 电压 ≈ 1.5 ~ 3.3V
  *         实际阈值可调节模块上的电位器进行校准
  */
static void ADC1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    /* 使能 GPIOA 和 ADC1 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    /* PA0: 模拟输入 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ADC1: 独立模式，单通道，软件触发 */
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 配置规则通道: CH0，采样时间 55.5 周期 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);

    /* 使能并校准 ADC1 */
    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

/**
  * @brief  读取 ADC1 单次转换结果
  * @retval 12位 ADC 转换值: 0 ~ 4095
  */
static uint16_t ADC1_Read(void)
{
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    return (uint16_t)ADC_GetConversionValue(ADC1);
}

/* ===== OLED 显示函数 =======================================================*/

/**
  * @brief  夜间模式 OLED 显示
  *         显示测距状态、距离数值，距离过近时显示危险警告
  * @param  distance: 测量距离（cm），-1 = 无信号 / 超出量程
  */
static void Display_NightMode(float distance)
{
    uint8_t dist_int, dist_frac;

    OLED_Clear();

    /* ---- 第1行: 模式标题（大字，居中）y=0~15 ---- */
    OLED_ShowString(24, 0, "NIGHT MODE", 8);



    /* ---- 第2行: 距离数值 y=24~39 ---- */
    OLED_ShowString(0, 24, "Dist:", 8);

    if (distance < 0)
    {
        /* 未收到回波 — 前方无障碍物或传感器异常 */
        OLED_ShowString(48, 24, "No Sig", 8);
    }
    else
    {
        /* 显示距离: XX.X cm（浮点数拆分为整数和小数部分） */
        dist_int  = (uint8_t)distance;
        dist_frac = (uint8_t)(distance * 10.0f + 0.5f) % 10;
        OLED_Printf(48, 24, 16, "%d.%d cm", (int)dist_int, (int)dist_frac);
    }

    /* ---- 第3行: 警告 / 状态 y=48~63 ---- */
    if (distance > 0 && distance < DANGER_DISTANCE_CM)
    {
        /* 障碍物距离过近 */
        OLED_ShowString(24, 48, "  DANGER!  ", 16);
    }
    else if (distance < 0)
    {
        OLED_ShowString(16, 48, "Check Sensor", 16);
    }
    else
    {
        OLED_ShowString(48, 48, "Safe", 16);
    }

    OLED_Refresh();
}

/**
  * @brief  日间模式 OLED 显示
  *         超声波模块处于待机状态以节省功耗
  */
static void Display_DayMode(void)
{
    OLED_Clear();

    /* ---- 第1行: 模式标题（大字，居中）y=4~19 ---- */
    OLED_ShowString(32, 4, "DAY MODE", 16);

    /* ---- 第2行: 状态提示 y=36~51 ---- */
    OLED_ShowString(8, 36, "Ultrasonic OFF", 16);

    OLED_Refresh();
}

/******************* (C) COPYRIGHT 2026 *****END OF FILE****/
