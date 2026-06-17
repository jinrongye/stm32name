/**
  ******************************************************************************
  * @file    user/hc_sr04.c
  * @author
  * @version V2.0
  * @date    2026-06-17
  * @brief   HC-SR04 超声波测距模块驱动（TIM3计时 + SysTick超时）
  *
  *          工作原理：
  *          1. TRIG 发送 ≥10μs 高电平脉冲 → HC-SR04 发射 8个40kHz超声波
  *          2. ECHO 引脚拉高，高电平持续时间 ∝ 距离
  *          3. TIM3 自由计数器（1MHz）测量高电平脉宽
  *          4. SysTick（g_ms_tick）做超时保护
  *          5. 距离(cm) = 脉宽(μs) / 58
  *
  *          引脚映射：
  *          - TRIG: PB0（GPIO 推挽输出）
  *          - ECHO: PB1（GPIO 浮空输入）
  *          - TIM3 时钟: 72MHz，预分频 71 → 1MHz（1μs 分辨率）
  ******************************************************************************
  */

/* 包含头文件 ----------------------------------------------------------------*/
#include "hc_sr04.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"

/* 外部变量 — SysTick 时基 ------------------------------------------------*/
extern volatile uint32_t g_ms_tick;  /* 1ms 计数器，定义在 main.c */

/* 宏定义 -------------------------------------------------------------------*/
#define TIM3_PRESCALER  71     /* 72MHz/(71+1) = 1MHz → 1μs/计数 */
#define TIM3_PERIOD     65535  /* 16位定时器最大值 */

/* 导出函数实现 --------------------------------------------------------------*/

/**
  * @brief  初始化 HC-SR04 模块
  *         - PB0（TRIG）: 推挽输出，默认低电平
  *         - PB1（ECHO）: 浮空输入
  *         - TIM3: 自由运行计数器，1MHz，仅计时不用中断
  */
void HC_SR04_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    /* ---- 使能 GPIOB 和 TIM3 时钟 ---- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* ---- PB0: TRIG 推挽输出（默认低电平）---- */
    GPIO_InitStructure.GPIO_Pin   = HC_SR04_TRIG_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HC_SR04_TRIG_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN);

    /* ---- PB1: ECHO 浮空输入 ---- */
    GPIO_InitStructure.GPIO_Pin   = HC_SR04_ECHO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HC_SR04_ECHO_PORT, &GPIO_InitStructure);

    /* ---- TIM3: 1MHz 自由计数器，不使能中断 ---- */
    TIM_TimeBaseStructure.TIM_Period        = TIM3_PERIOD;
    TIM_TimeBaseStructure.TIM_Prescaler     = TIM3_PRESCALER;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_Cmd(TIM3, ENABLE);
}

/**
  * @brief  一次完整超声波测距（阻塞式，约 1~25ms）
  * @retval 距离值（2.0 ~ 450.0 cm）
  *         -1.0 = 超时 / 无回波 / 硬件异常
  *
  * @note   工作流程:
  *         1. TRIG 发送 15μs 高电平触发脉冲
  *         2. 等待 ECHO 变高（超时 20ms，SysTick）
  *         3. TIM3 清零，等待 ECHO 变低（超时 40ms，SysTick）
  *         4. 读取 TIM3 计数值 → 计算脉宽 → 换算距离
  */
float HC_SR04_Measure(void)
{
    uint32_t start_ms;
    uint32_t start_cnt, end_cnt;
    uint32_t pulse_width_us;
    float    distance;

    /* ===== 第1步: 发送触发脉冲 ===== */

    /* TRIG 拉低 2μs 确保干净起始电平 */
    GPIO_ResetBits(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN);
    {
        volatile uint32_t d = 20;
        while (d--) { __NOP(); }
    }

    /* TRIG 拉高 ≥10μs（此处约 15μs） */
    GPIO_SetBits(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN);
    {
        volatile uint32_t d = 150;
        while (d--) { __NOP(); }
    }

    /* TRIG 拉低，触发完成 */
    GPIO_ResetBits(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN);

    /* ===== 第2步: 等待 ECHO 变高（超时 20ms）===== */

    start_ms = g_ms_tick;
    while (GPIO_ReadInputDataBit(HC_SR04_ECHO_PORT, HC_SR04_ECHO_PIN) == 0)
    {
        if ((g_ms_tick - start_ms) >= 20)
        {
            return -1.0f;  /* 超时: 模块未响应，检查 5V 供电和接线 */
        }
    }

    /* ===== 第3步: ECHO 已变高，TIM3 从零开始计时 ===== */

    TIM_SetCounter(TIM3, 0);
    start_cnt = TIM_GetCounter(TIM3);

    /* ===== 第4步: 等待 ECHO 变低（超时 40ms，约 7m 量程）===== */

    start_ms = g_ms_tick;
    while (GPIO_ReadInputDataBit(HC_SR04_ECHO_PORT, HC_SR04_ECHO_PIN) == 1)
    {
        if ((g_ms_tick - start_ms) >= 40)
        {
            return -1.0f;  /* 超时: ECHO 高电平持续时间异常 */
        }
    }

    /* ===== 第5步: 记录 TIM3 计数值，计算脉宽 ===== */

    end_cnt = TIM_GetCounter(TIM3);

    /* 处理 TIM3 可能的溢出（16位计数器 65535μs ≈ 65ms，
     * ECHO 脉宽最长约 25ms，正常不会溢出，保留安全处理 */
    if (end_cnt >= start_cnt)
    {
        pulse_width_us = end_cnt - start_cnt;
    }
    else
    {
        pulse_width_us = (TIM3_PERIOD - start_cnt) + end_cnt + 1;
    }

    /* ===== 第6步: 换算距离 ===== */
    /*
     * 声速 ≈ 340 m/s = 0.034 cm/μs
     * 距离 = (脉宽 × 0.034) / 2   ← 往返双程
     *      = 脉宽 / 58.82
     *      ≈ 脉宽 / 58
     */
    distance = (float)pulse_width_us / 58.0f;

    /* 量程校验 */
    if (distance < HC_SR04_MIN_DISTANCE_CM || distance > HC_SR04_MAX_DISTANCE_CM)
    {
        return -1.0f;
    }

    return distance;
}

/**
  * @brief  空函数 — 保留以兼容 stm32f10x_it.c 中的 TIM3_IRQHandler 调用
  */
void HC_SR04_IRQHandler(void)
{
}
