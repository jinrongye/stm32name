/**
  ******************************************************************************
  * @file    user/hc_sr04.c
  * @author
  * @version V1.1
  * @date    2026-06-17
  * @brief   HC-SR04 超声波测距模块驱动（GPIO轮询 + TIM3计时）
  *
  *          工作原理：
  *          1. MCU 向 TRIG 引脚发送 ≥15μs 的高电平脉冲
  *          2. HC-SR04 自动发射 8 个 40kHz 超声波脉冲
  *          3. ECHO 引脚拉高，高电平持续时间与距离成正比
  *          4. GPIO 轮询检测 ECHO 高电平，TIM3 自由计数计时
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
#include "misc.h"

/* 导出函数实现 --------------------------------------------------------------*/

/**
  * @brief  初始化 HC-SR04 模块
  *         - PB0（TRIG）: 推挽输出，默认低电平
  *         - PB1（ECHO）: 浮空输入
  *         - TIM3: 自由运行计数器，1MHz（1μs 分辨率）
  *         - 不使用 NVIC 中断，测距由 GPIO 轮询完成
  */
void HC_SR04_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    /* ---- 使能 GPIO 和 TIM3 时钟 ---- */
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

    /* ---- TIM3 时基配置: 1MHz（1μs/计数），自由运行 ---- */
    TIM_TimeBaseStructure.TIM_Period        = HC_SR04_TIM_PERIOD;   /* 65535 */
    TIM_TimeBaseStructure.TIM_Prescaler     = HC_SR04_TIM_PRESCALER; /* 71 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(HC_SR04_TIM, &TIM_TimeBaseStructure);

    /* 启动 TIM3（仅作自由计数器，不使能中断） */
    TIM_Cmd(HC_SR04_TIM, ENABLE);
}

/**
  * @brief  发送触发脉冲 + 轮询 ECHO 完成一次完整测距
  * @note   发送 ≥15μs 高电平到 TRIG，然后轮询 ECHO 引脚
  *         高电平持续时间即超声波往返时间
  */
void HC_SR04_StartMeasure(void)
{
    /* 先拉低 TRIG 确保干净的电平（已默认低，此处保险） */
    GPIO_ResetBits(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN);

    /* 短暂延时让引脚稳定 */
    {
        volatile uint32_t d = 200;
        while (d--) { __NOP(); }
    }

    /* 发送 ≥15μs 高电平触发脉冲 */
    GPIO_SetBits(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN);
    {
        volatile uint32_t d = 400;   /* 约 20~30μs，确保可靠触发 */
        while (d--) { __NOP(); }
    }
    GPIO_ResetBits(HC_SR04_TRIG_PORT, HC_SR04_TRIG_PIN);
}

/**
  * @brief  查询测量是否完成（轮询方式下始终返回 1）
  * @retval 始终返回 1（测距在 HC_SR04_GetDistance 中阻塞完成）
  */
uint8_t HC_SR04_IsDataReady(void)
{
    return 1;  /* 轮询方式，GetDistance 内部完成等待 */
}

/**
  * @brief  轮询 ECHO 引脚并计算距离（阻塞式）
  * @retval 距离值（2.0 ~ 400.0 cm）
  *         -1.0 = 超时 / 无回波
  * @note   最长阻塞约 40ms（对应 4m+ 量程）
  */
float HC_SR04_GetDistance(void)
{
    uint32_t timeout;
    uint32_t start_cnt, end_cnt;
    uint32_t pulse_width_us;
    float    distance;

    /* ---- 等待 ECHO 变为高电平（超时 ≈ 5ms） ---- */
    timeout = 50000;
    while (GPIO_ReadInputDataBit(HC_SR04_ECHO_PORT, HC_SR04_ECHO_PIN) == 0)
    {
        if (--timeout == 0)
        {
            return -1.0f;  /* 超时: HC-SR04 未响应，检查 5V 供电和接线 */
        }
    }

    /* ---- ECHO 上升沿: 记录 TIM3 当前计数值 ---- */
    TIM_SetCounter(HC_SR04_TIM, 0);
    start_cnt = TIM_GetCounter(HC_SR04_TIM);

    /* ---- 等待 ECHO 变为低电平（超时 ≈ 40ms，对应 ~7m） ---- */
    timeout = 400000;
    while (GPIO_ReadInputDataBit(HC_SR04_ECHO_PORT, HC_SR04_ECHO_PIN) == 1)
    {
        if (--timeout == 0)
        {
            return -1.0f;  /* 超时: ECHO 高电平持续时间异常 */
        }
    }

    /* ---- ECHO 下降沿: 记录 TIM3 当前计数值 ---- */
    end_cnt = TIM_GetCounter(HC_SR04_TIM);

    /* 计算高电平脉宽（μs），处理可能的计数器溢出 */
    if (end_cnt >= start_cnt)
    {
        pulse_width_us = end_cnt - start_cnt;
    }
    else
    {
        pulse_width_us = (HC_SR04_TIM_PERIOD - start_cnt) + end_cnt + 1;
    }

    /* 将脉宽换算为距离:
     * 声速 ≈ 340 m/s = 0.034 cm/μs
     * 距离 = (脉宽 × 0.034) / 2  （往返）
     *      ≈ 脉宽 / 58
     */
    distance = (float)pulse_width_us / 58.0f;

    /* 量程校验（HC-SR04 有效范围: 2cm ~ 400cm） */
    if (distance < HC_SR04_MIN_DISTANCE_CM || distance > HC_SR04_MAX_DISTANCE_CM)
    {
        return -1.0f;
    }

    return distance;
}

/**
  * @brief  TIM3 中断服务处理函数（保留，轮询方式下不使用中断）
  * @note   轮询方式无需此函数，保留空实现防止链接错误
  */
void HC_SR04_IRQHandler(void)
{
    /* 轮询方式不使用中断，空函数 */
}
