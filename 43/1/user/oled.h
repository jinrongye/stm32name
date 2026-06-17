/**
  ******************************************************************************
  * @file    user/oled.h
  * @author
  * @version V1.0
  * @date    2026-06-12
  * @brief   SSD1306 OLED driver header file (I2C)
  ******************************************************************************
  */

#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"

/* OLED I2C 地址（7位: 0x3C，左移1位得8位写地址 0x78）------------------*/
/* 如果 OLED 不亮，尝试另一个常见地址 0x7A                               */
#define OLED_I2C_ADDR       0x78    /* 大部分模块用 0x78，少数用 0x7A */

/* OLED dimensions ------------------------------------------------------------*/
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

/* OLED control byte ----------------------------------------------------------*/
#define OLED_CMD_BYTE       0x00  /* Next byte is command */
#define OLED_DATA_BYTE      0x40  /* Next byte is data */

/* Exported functions ---------------------------------------------------------*/
void OLED_Init(void);
void OLED_DisplayOn(void);
void OLED_DisplayOff(void);
void OLED_Clear(void);
void OLED_Refresh(void);
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t dot);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);
void OLED_ShowSignedNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size);
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t index, uint8_t size);
void OLED_Printf(uint8_t x, uint8_t y, uint8_t size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */
