/**
  ******************************************************************************
  * @file    user/stm32f10x_it.c
  * @author
  * @version V3.0
  * @date    2026-06-17
  * @brief   中断服务函数
  *          - SysTick_Handler: 1ms 系统时基
  *          - TIM3_IRQHandler: HC-SR04 超声波输入捕获
  *          - 其余为 Cortex-M3 异常处理（默认死循环）
  ******************************************************************************
  */

/* 包含头文件 ----------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "hc_sr04.h"

/* 外部变量 -----------------------------------------------------------------*/
extern volatile uint32_t g_ms_tick;  /* 1ms 时基计数器，定义在 main.c */

/* Cortex-M3 处理器异常处理 --------------------------------------------------*/

/**
  * @brief  NMI 不可屏蔽中断
  */
void NMI_Handler(void)
{
}

/**
  * @brief  硬件错误异常 — 死循环
  */
void HardFault_Handler(void)
{
    while (1)
    {
    }
}

/**
  * @brief  内存管理异常 — 死循环
  */
void MemManage_Handler(void)
{
    while (1)
    {
    }
}

/**
  * @brief  总线错误异常 — 死循环
  */
void BusFault_Handler(void)
{
    while (1)
    {
    }
}

/**
  * @brief  用法错误异常 — 死循环
  */
void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

/**
  * @brief  SVCall 服务调用异常
  */
void SVC_Handler(void)
{
}

/**
  * @brief  调试监视异常
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  PendSV 可挂起系统调用
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  SysTick 定时器中断 — 每 1ms 触发一次
  *         递增全局时基计数器 g_ms_tick，供 Delay_ms() 使用
  */
void SysTick_Handler(void)
{
    g_ms_tick++;
}

/* STM32F10x 外设中断处理 ----------------------------------------------------*/

/**
  * @brief  TIM3 全局中断
  *         HC-SR04 超声波模块 ECHO 引脚输入捕获
  *         具体逻辑由 hc_sr04.c 中的 HC_SR04_IRQHandler() 处理
  */
void TIM3_IRQHandler(void)
{
    HC_SR04_IRQHandler();
}

/******************* (C) COPYRIGHT 2026 *****END OF FILE****/
