/**
  ******************************************************************************
  * @file    user/hc_sr04.h
  * @author
  * @version V2.0
  * @date    2026-06-17
  * @brief   HC-SR04 超声波测距模块驱动 — 头文件（纯GPIO+SysTick）
  *          - TRIG: PB0（推挽输出）
  *          - ECHO: PB1（浮空输入，GPIO轮询）
  *          - 计时: 纯软件循环 + SysTick 超时保护
  *          - 量程: 约 2cm ~ 400cm
  *          - 用法: distance = HC_SR04_Measure();
  ******************************************************************************
  */

#ifndef __HC_SR04_H
#define __HC_SR04_H

#ifdef __cplusplus
 extern "C" {
#endif

/* 包含头文件 ----------------------------------------------------------------*/
#include "stm32f10x.h"

/* 引脚定义 -----------------------------------------------------------------*/
#define HC_SR04_TRIG_PORT       GPIOB                /* TRIG 所在端口 */
#define HC_SR04_TRIG_PIN        GPIO_Pin_0           /* PB0: 触发信号输出 */
#define HC_SR04_ECHO_PORT       GPIOB                /* ECHO 所在端口 */
#define HC_SR04_ECHO_PIN        GPIO_Pin_1           /* PB1: 回波信号输入 */

/* 测量参数 -----------------------------------------------------------------*/
#define HC_SR04_MIN_DISTANCE_CM 2.0f                 /* 最小有效距离（cm） */
#define HC_SR04_MAX_DISTANCE_CM 450.0f               /* 最大有效距离（cm） */

/* 导出函数声明 --------------------------------------------------------------*/
void  HC_SR04_Init(void);                            /* 初始化 GPIO（PB0输出 PB1输入） */
float HC_SR04_Measure(void);                         /* 一次完整测距，返回 cm，-1=超时 */
void  HC_SR04_IRQHandler(void);                      /* 保留空函数，兼容 stm32f10x_it.c */

#ifdef __cplusplus
}
#endif

#endif /* __HC_SR04_H */
