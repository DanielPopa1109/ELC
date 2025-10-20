/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ALL_RUNTIME_MEAS_Pin GPIO_PIN_13
#define ALL_RUNTIME_MEAS_GPIO_Port GPIOC
#define ANT30VFB_Pin GPIO_PIN_1
#define ANT30VFB_GPIO_Port GPIOA
#define AN2L1_Pin GPIO_PIN_2
#define AN2L1_GPIO_Port GPIOA
#define AN1L1_Pin GPIO_PIN_3
#define AN1L1_GPIO_Port GPIOA
#define ANL1VFB_Pin GPIO_PIN_4
#define ANL1VFB_GPIO_Port GPIOA
#define ENL1CLS_Pin GPIO_PIN_1
#define ENL1CLS_GPIO_Port GPIOB
#define ENL1_Pin GPIO_PIN_10
#define ENL1_GPIO_Port GPIOB
#define ADC_RUNTIME_MEAS_Pin GPIO_PIN_3
#define ADC_RUNTIME_MEAS_GPIO_Port GPIOB
#define SMON_RUNTIME_MEAS_Pin GPIO_PIN_5
#define SMON_RUNTIME_MEAS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
