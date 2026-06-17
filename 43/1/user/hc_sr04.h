/**
  ******************************************************************************
  * @file    user/hc_sr04.h
  * @author
  * @version V1.0
  * @date    2026-06-17
  * @brief   HC-SR04 超声波测距模块驱动 — 头文件
  *          - TRIG: PB0（推挽输出）
  *          - ECHO: PB1（TIM3_CH4 输入捕获）
  *          - 分辨率: 1μs（TIM3 工作频率 1MHz）
  *          - 量程: 约 2cm ~ 400cm
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
#define HC_SR04_ECHO_PIN        GPIO_Pin_1           /* PB1: 回波信号输入（TIM3_CH4） */

/* 定时器配置 ----------------------------------------------------------------*/
#define HC_SR04_TIM             TIM3                 /* 使用的定时器 */
#define HC_SR04_TIM_CHANNEL     TIM_Channel_4        /* 输入捕获通道 */
#define HC_SR04_TIM_IRQ         TIM3_IRQn            /* 定时器中断号 */
#define HC_SR04_TIM_PRESCALER   71                   /* 72MHz/(71+1) = 1MHz → 1μs/计数 */
#define HC_SR04_TIM_PERIOD      65535                /* 16位定时器最大值，约65ms超时 */

/* 测量参数 -----------------------------------------------------------------*/
#define HC_SR04_TIMEOUT_MS      100UL                /* 测量超时时间（ms） */
#define HC_SR04_MIN_DISTANCE_CM 2.0f                 /* 最小有效距离（cm） */
#define HC_SR04_MAX_DISTANCE_CM 400.0f               /* 最大有效距离（cm） */

/* 导出函数声明 --------------------------------------------------------------*/
void    HC_SR04_Init(void);                         /* 初始化 GPIO + TIM3 输入捕获 */
void    HC_SR04_StartMeasure(void);                 /* 发送 20μs 触发脉冲，启动一次测量 */
uint8_t HC_SR04_IsDataReady(void);                  /* 查询测量是否完成，1=完成 */
float   HC_SR04_GetDistance(void);                  /* 获取距离（cm），-1 = 超时/无回波 */
void    HC_SR04_IRQHandler(void);                   /* TIM3 中断服务回调，在 stm32f10x_it.c 中调用 */

#ifdef __cplusplus
}
#endif

#endif /* __HC_SR04_H */
