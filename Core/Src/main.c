/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "canloop.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;
CAN_HandleTypeDef hcan2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
uint8_t                 board_variant = UNKNOWN_BOARD;
uint16_t                led_pin       = GPIO_PIN_NONE;
uint8_t                 usart         = 1;
uint32_t                usart3_gpio   = 0;
UART_HandleTypeDef*     huart         = &huart1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_CAN2_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//-------------------------------------------------------------------------
// BlackBoard:
// Optionally install an LED on PB4 (Labelled V8 on back of board)
// UART3 on PB10(TX) and PB11(RX)
// CAN1 requires clearing these (Because GD32F105)
//  CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_SLEEP);
//  CLEAR_BIT(hcan->Instance->MCR, CAN_MCR_DBF);
// Power consumption: 54mA in sleep mode
//                    64mA when active
//-------------------------------------------------------------------------
// Blue board:
// UART3 on PC10(TX) and PC11(RX)
// Power consumption: 69mA in sleep mode (with FTDI attached)
//                    89mA when active   (with FTDI attached)
//-------------------------------------------------------------------------
// GreenBoard:
// UART1 on PA9(TX) and PA10(RX)
// Power consumption: 18mA in sleep mode
//                    38mA when active
//-------------------------------------------------------------------------
// SmallGreen:
// LED on PB9
// UART1 on PA9(TX) and PA10(RX)
// Power consumption: 18mA in sleep mode (Yes, same as large green)
//                    38mA when active
//-------------------------------------------------------------------------
uint32_t board_variants[4][3] = {
  {0x42319f17, 0x03323033, 0x3032334c}, // BOARD_VARIANT_BLACK - Verified CAN and USART3
  {0x05d8ff39, 0x38365548, 0x43213544}, // BOARD_VARIANT_BLUE - Verified CAN and USART3
  {0x05d8ff36, 0x37325553, 0x43184563}, // BOARD_VARIANT_GREEN - Verified CAN and USART1
  {0x05d5ff30, 0x38374250, 0x43024338}  // BOARD_VARIANT_SMALL_GREEN - Verified
};

#define BOARD_VARIANT_BLACK         0
#define BOARD_VARIANT_BLUE          1
#define BOARD_VARIANT_GREEN         2
#define BOARD_VARIANT_SMALL_GREEN   3
#define UNKNOWN_BOARD               0xff

char board_variant_names[4][16] = {
  "Black",
  "Blue",
  "Green",
  "Small Green"
};

uint8_t GetBoardVariant(void) {
  uint32_t unique_id[3];
  unique_id[0] = *(uint32_t*)(0x1FFFF7E8); // UID 31:0
  unique_id[1] = *(uint32_t*)(0x1FFFF7EC); // UID 63:32
  unique_id[2] = *(uint32_t*)(0x1FFFF7F0); // UID 95:64
  sendDebugMsg("UniqueID: %08x %08x %08x \r\n", unique_id[0], unique_id[1], unique_id[2]);

  for (uint8_t i = 0; i < sizeof(board_variants) / sizeof(board_variants[0]); i++) {
    if (unique_id[0] == board_variants[i][0] &&
        unique_id[1] == board_variants[i][1] &&
        unique_id[2] == board_variants[i][2]) {
      sendDebugMsg( "Board variant: %s\r\n", board_variant_names[i]);
      return i; // Return the matching variant index
    }
  }
  sendDebugMsg( "Unable to determine the board variant. Defaulting to %s settings.\r\n", board_variant_names[BOARD_VARIANT_SMALL_GREEN]);
  return BOARD_VARIANT_SMALL_GREEN;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  board_variant = GetBoardVariant();
  switch (board_variant) {
    case BOARD_VARIANT_BLACK:
      usart         = 3;
      huart         = &huart3;
      usart3_gpio   = GPIOB_BASE;
      led_pin       = GPIO_PIN_4; // Must be installed by end user
      break;
    case BOARD_VARIANT_BLUE:
      usart         = 3;
      huart         = &huart3;
      usart3_gpio   = GPIOC_BASE;
      led_pin       = GPIO_PIN_NONE;
      break;
    case BOARD_VARIANT_GREEN:
      usart         = 1;
      huart         = &huart1;
      usart3_gpio   = 0;
      led_pin       = GPIO_PIN_NONE;
      break;
    case BOARD_VARIANT_SMALL_GREEN:
      usart         = 1;
      huart         = &huart1;
      usart3_gpio   = 0;
      led_pin       = GPIO_PIN_9; // Prepopulated by manufacturer
      break;
    default: // Defaults to same as SMALL GREEN if unknown
      usart         = 1;
      huart         = &huart1;
      usart3_gpio   = 0;
      led_pin       = GPIO_PIN_9;
      break;
  }

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_CAN1_Init();
  MX_CAN2_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  canloop();

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV5;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.Prediv1Source = RCC_PREDIV1_SOURCE_PLL2;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL2.PLL2State = RCC_PLL2_ON;
  RCC_OscInitStruct.PLL2.PLL2MUL = RCC_PLL2_MUL8;
  RCC_OscInitStruct.PLL2.HSEPrediv2Value = RCC_HSE_PREDIV2_DIV5;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the Systick interrupt time
  */
  __HAL_RCC_PLLI2S_ENABLE();
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */
  // Konfig taken from: https://www.elektronik-keller.de/index.php/projekte/stm32/29-stm32cubemx-stm32-can
  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */
  CAN_FilterTypeDef sFilterConfig;
  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 18;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_16TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_7TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    sendDebugMsg("CAN1 Init failed\r\n");
    Error_Handler();
  } else {
    sendDebugMsg("CAN1 Init OK\r\n");
  }
  /* USER CODE BEGIN CAN1_Init 2 */
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK) {
    /* Filter configuration Error */
    Error_Handler();
  }

  if (HAL_CAN_Start(&hcan1) != HAL_OK) {
    /* Start Error */
    Error_Handler();
  }
  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief CAN2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN2_Init(void)
{

  /* USER CODE BEGIN CAN2_Init 0 */
#if ENABLE_CAN_2 == 1
  CAN_FilterTypeDef sFilterConfig;
  /* USER CODE END CAN2_Init 0 */

  /* USER CODE BEGIN CAN2_Init 1 */

  /* USER CODE END CAN2_Init 1 */
  hcan2.Instance = CAN2;
  hcan2.Init.Prescaler = 18;
  hcan2.Init.Mode = CAN_MODE_NORMAL;
  hcan2.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan2.Init.TimeSeg1 = CAN_BS1_16TQ;
  hcan2.Init.TimeSeg2 = CAN_BS2_7TQ;
  hcan2.Init.TimeTriggeredMode = DISABLE;
  hcan2.Init.AutoBusOff = DISABLE;
  hcan2.Init.AutoWakeUp = DISABLE;
  hcan2.Init.AutoRetransmission = DISABLE;
  hcan2.Init.ReceiveFifoLocked = DISABLE;
  hcan2.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN2_Init 2 */
  sFilterConfig.FilterBank = 15;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO1;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 15;

  if (HAL_CAN_ConfigFilter(&hcan2, &sFilterConfig) != HAL_OK) {
    /* Filter configuration Error */
    Error_Handler();
  }

  if (HAL_CAN_Start(&hcan2) != HAL_OK) {
    /* Start Error */
    Error_Handler();
  }
#endif
  /* USER CODE END CAN2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */
  if (usart != 1) {
    return;
  }
  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */
  if (usart != 3) {
    return;
  }
  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */
  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE(); // CAN2 & USART3 (black) & LED GPIO (small green)

  /*Configure GPIO pin Output Level */
  if (led_pin != GPIO_PIN_NONE) {
    HAL_GPIO_WritePin(GPIOB, led_pin, GPIO_PIN_RESET);

    /*Configure GPIO pins : PB4 PB9 */
    GPIO_InitStruct.Pin = led_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, led_pin, GPIO_PIN_SET); // Initially the LED is on
  }

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void EnterSleepMode(void) {
  sendDebugMsg("Entering low power mode\r\n");

  // Turn off the LED and de-initialize the GPIO pin
  if (led_pin != GPIO_PIN_NONE) {
    HAL_GPIO_WritePin(GPIOB, led_pin, GPIO_PIN_RESET);
    HAL_GPIO_DeInit(GPIOB, led_pin);
    __HAL_RCC_GPIOB_CLK_DISABLE();
  }

  // Power off and de-initialize the UART
  if (usart == 1) {
    HAL_UART_DeInit(&huart1);
    __HAL_RCC_USART1_CLK_DISABLE();
  } else if (usart == 3) {
    HAL_UART_DeInit(&huart3);
    __HAL_RCC_USART3_CLK_DISABLE();
  }
}

void ExitSleepMode(void) {
  // Re-enable the GPIOB clock and re-initialize the GPIO pin
  __HAL_RCC_GPIOB_CLK_ENABLE();
  MX_GPIO_Init();

  // Re-enable USART1 clock and re-initialize the UART
  if (usart == 1) {
    __HAL_RCC_USART1_CLK_ENABLE();
    MX_USART1_UART_Init();
  } else if (usart == 3) {
    __HAL_RCC_USART3_CLK_ENABLE();
    MX_USART3_UART_Init();
  }

  sendDebugMsg("Exited low power mode\r\n");
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
