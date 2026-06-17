/**
  ******************************************************************************
  * @file    user/hc_sr04.c
  * @author
  * @version V1.0
  * @date    2026-06-17
  * @brief   HC-SR04 超声波测距模块驱动
  *
  *          工作原理：
  *          1. MCU 向 TRIG 引脚发送一个 20μs 的高电平脉冲
  *          2. HC-SR04 自动发射 8 个 40kHz 超声波脉冲
  *          3. ECHO 引脚拉高，高电平持续时间与距离成正比
  *          4. TIM3 CH4 输入捕获测量 ECHO 高电平脉宽
  *          5. 距离(cm) = 脉宽(μs) / 58
  *
  *          引脚映射：
  *          - TRIG: PB0（GPIO 推挽输出）
  *          - ECHO: PB1（TIM3 CH4 输入捕获）
  *          - TIM3 时钟: 72MHz，预分频 71 → 1MHz（1μs 分辨率）
  ******************************************************************************
  */

/* 包含头文件 ----------------------------------------------------------------*/
#include "hc_sr04.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "misc.h"

/* 私有变量 — 与中断服务函数共享 ---------------------------------------------*/
/**
  * @brief  输入捕获状态变量（与 ISR 共享）
  *         g_cap_rising:  记录 ECHO 上升沿时刻 TIM3 的计数值
  *         g_cap_falling: 记录 ECHO 下降沿时刻 TIM3 的计数值
  *         g_cap_done:    1 = 本次测量完成（下降沿已捕获或超时）
  *         g_cap_started: 1 = 已捕获上升沿，正在等待下降沿
  */
static volatile uint16_t g_cap_rising  = 0;
static volatile uint16_t g_cap_falling = 0;
static volatile uint8_t  g_cap_done    = 0;
static volatile uint8_t  g_cap_started = 0;

/* 私有函数声明 --------------------------------------------------------------*/
static void Delay_20us(void);

/* 导出函数实现 --------------------------------------------------------------*/

/**
  * @brief  初始化 HC-SR04 模块
  *         - PB0（TRIG）: 推挽输出，默认低电平
  *         - PB1（ECHO）: 浮空输入
  *         - TIM3 CH4: 输入捕获，先捕获上升沿
  *         - NVIC: 使能 TIM3 中断
  */
void HC_SR04_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    /* ---- 使能 GPIO 时钟 ---- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
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

    /* ---- TIM3 时基配置: 1MHz（1μs/计数），自动重装载 65535 ---- */
    TIM_TimeBaseStructure.TIM_Period        = HC_SR04_TIM_PERIOD;
    TIM_TimeBaseStructure.TIM_Prescaler     = HC_SR04_TIM_PRESCALER;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(HC_SR04_TIM, &TIM_TimeBaseStructure);

    /* ---- TIM3 CH4: 输入捕获，先捕获上升沿 ---- */
    TIM_ICInitStructure.TIM_Channel     = HC_SR04_TIM_CHANNEL;
    TIM_ICInitStructure.TIM_ICPolarity  = TIM_ICPolarity_Rising;     /* 先捕获上升沿 */
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;  /* 直连: PB1→TI4 */
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;            /* 每次边沿都捕获 */
    TIM_ICInitStructure.TIM_ICFilter    = 0x03;                      /* 8周期滤波，滤除毛刺 */
    TIM_ICInit(HC_SR04_TIM, &TIM_ICInitStructure);

    /* ---- 使能 TIM3 CC4 中断 + 更新中断（溢出超时检测）---- */
    TIM_ITConfig(HC_SR04_TIM, TIM_IT_CC4 | TIM_IT_Update, ENABLE);

    /* ---- NVIC 中断优先级配置 ---- */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  /* 2位抢占 + 2位子优先级 */
    NVIC_InitStructure.NVIC_IRQChannel                   = HC_SR04_TIM_IRQ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* ---- 启动 TIM3 计数器 ---- */
    TIM_Cmd(HC_SR04_TIM, ENABLE);
}

/**
  * @brief  发送 20μs 高电平触发脉冲到 HC-SR04 的 TRIG 引脚
  * @note   需在 HC_SR04_Init() 之后调用。
  *         1. 清零所有捕获状态标志
  *         2. TRIG 拉高 20μs
  *         3. TRIG 拉低
  *         后续 ECHO 的捕获由 TIM3 ISR 自动完成
  */
void HC_SR04_StartMeasure(void)
{
    /* 清零捕获状态 */
    g_cap_done    = 0;
    g_cap_started = 0;

    /* 清零 TIM3 计数器，保证测量从零开始 */
    TIM_SetCounter(HC_SR04_TIM, 0);

    /* 在 TRIG 引脚上产生 20μs 高电平脉冲 */
    GPIO_SetBits(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN);
    Delay_20us();
    GPIO_ResetBits(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN);
}

/**
  * @brief  查询本次测量是否已完成
  * @retval 1 = 测量完成（可调用 HC_SR04_GetDistance 读取结果）
  *         0 = 仍在等待中
  */
uint8_t HC_SR04_IsDataReady(void)
{
    return g_cap_done;
}

/**
  * @brief  获取测量距离（厘米）
  * @retval 距离值（2.0 ~ 400.0 cm）
  *         -1.0 = 测量未就绪 / 无回波 / 超出量程
  * @note   调用前需先确认 HC_SR04_IsDataReady() 返回 1，
  *         或在 HC_SR04_StartMeasure() 后等待足够长时间
  */
float HC_SR04_GetDistance(void)
{
    uint32_t pulse_width_us;
    float    distance;

    /* 检查测量是否完成 */
    if (!g_cap_done)
    {
        return -1.0f;  /* 尚未完成 */
    }

    /* 检查是否捕获到上升沿（未捕获 = 无回波） */
    if (!g_cap_started)
    {
        return -1.0f;  /* 无回波 — 前方无障碍物或超出量程 */
    }

    /* 计算高电平脉宽（μs） */
    if (g_cap_falling > g_cap_rising)
    {
        /* 正常情况: 未发生计数器溢出 */
        pulse_width_us = (uint32_t)(g_cap_falling - g_cap_rising);
    }
    else
    {
        /* 计数器溢出情况（极少见: 脉宽 > 65ms，距离 > 11m） */
        pulse_width_us = (uint32_t)(HC_SR04_TIM_PERIOD - g_cap_rising)
                       + (uint32_t)g_cap_falling + 1;
    }

    /* 将脉宽换算为距离:
     * 声速 ≈ 340 m/s = 0.034 cm/μs
     * 距离 = (脉宽 × 0.034) / 2  （往返）
     *      = 脉宽 / 58.82
     * 简化公式: distance_cm = pulse_width_us / 58
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
  * @brief  TIM3 中断服务处理函数 — 由 stm32f10x_it.c 中的 TIM3_IRQHandler() 调用
  *
  *         捕获 ECHO 引脚的边沿:
  *         - 上升沿: 记录计数值，切换为下降沿捕获
  *         - 下降沿: 记录计数值，置 g_cap_done = 1，切回上升沿捕获
  *         - 更新中断（溢出）: 超时处理，强制结束防止死等
  */
void HC_SR04_IRQHandler(void)
{
    /* ---- CC4 中断（输入捕获事件）---- */
    if (TIM_GetITStatus(HC_SR04_TIM, TIM_IT_CC4) != RESET)
    {
        TIM_ClearITPendingBit(HC_SR04_TIM, TIM_IT_CC4);

        if (!g_cap_started)
        {
            /* ----- 捕获到上升沿 ----- */
            g_cap_rising  = TIM_GetCapture4(HC_SR04_TIM);
            g_cap_started = 1;

            /* 切换为下降沿捕获 */
            TIM_ICInitTypeDef TIM_ICInitStructure;
            TIM_ICInitStructure.TIM_Channel     = HC_SR04_TIM_CHANNEL;
            TIM_ICInitStructure.TIM_ICPolarity  = TIM_ICPolarity_Falling;
            TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
            TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
            TIM_ICInitStructure.TIM_ICFilter    = 0x03;
            TIM_ICInit(HC_SR04_TIM, &TIM_ICInitStructure);
        }
        else
        {
            /* ----- 捕获到下降沿（测量完成）----- */
            g_cap_falling = TIM_GetCapture4(HC_SR04_TIM);
            g_cap_done    = 1;

            /* 切换回上升沿捕获，为下一次测量做准备 */
            TIM_ICInitTypeDef TIM_ICInitStructure;
            TIM_ICInitStructure.TIM_Channel     = HC_SR04_TIM_CHANNEL;
            TIM_ICInitStructure.TIM_ICPolarity  = TIM_ICPolarity_Rising;
            TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
            TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
            TIM_ICInitStructure.TIM_ICFilter    = 0x03;
            TIM_ICInit(HC_SR04_TIM, &TIM_ICInitStructure);
        }
    }

    /* ---- 更新中断（计数器溢出 / 超时）---- */
    if (TIM_GetITStatus(HC_SR04_TIM, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(HC_SR04_TIM, TIM_IT_Update);

        /* 如果已捕获上升沿但始终未捕获到下降沿，判定为超时 */
        if (g_cap_started && !g_cap_done)
        {
            g_cap_done = 1;  /* 强制标记完成（无有效下降沿） */
        }
    }
}

/* 私有函数 -----------------------------------------------------------------*/

/**
  * @brief  软件延时约 20μs（72MHz 下粗略校准）
  * @note   校准原理:
  *         每次循环约 4 个时钟周期（递减 + 比较 + NOP + 跳转）
  *         72MHz / 4 = 18M 次迭代/秒 → 1μs ≈ 18 次迭代
  *         20μs ≈ 360 次迭代；取 400 留有裕量
  */
static void Delay_20us(void)
{
    volatile uint32_t i = 400;
    while (i--)
    {
        __NOP();
    }
}
