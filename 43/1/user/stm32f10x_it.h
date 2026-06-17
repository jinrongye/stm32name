/**
  ******************************************************************************
  * @file    user/stm32f10x_it.h
  * @author
  * @version V3.0
  * @date    2026-06-17
  * @brief   中断服务函数头文件
  ******************************************************************************
  */

/* 防止重复包含 --------------------------------------------------------------*/
#ifndef __STM32F10x_IT_H
#define __STM32F10x_IT_H

#ifdef __cplusplus
 extern "C" {
#endif

/* 包含头文件 ----------------------------------------------------------------*/
#include "stm32f10x.h"

/* 中断服务函数声明 ----------------------------------------------------------*/

void NMI_Handler(void);                 /* 不可屏蔽中断 */
void HardFault_Handler(void);           /* 硬件错误 */
void MemManage_Handler(void);           /* 内存管理异常 */
void BusFault_Handler(void);            /* 总线错误 */
void UsageFault_Handler(void);          /* 用法错误 */
void SVC_Handler(void);                 /* 服务调用 */
void DebugMon_Handler(void);            /* 调试监视 */
void PendSV_Handler(void);              /* 可挂起系统调用 */
void SysTick_Handler(void);             /* 系统定时器（1ms 时基） */
void TIM3_IRQHandler(void);             /* TIM3 中断（HC-SR04 输入捕获） */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F10x_IT_H */
