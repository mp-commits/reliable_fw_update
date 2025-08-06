/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "stdbool.h"
#include "string.h"
#include "ed25519.h"
#include "fragmentstore/fragmentstore.h"
#include "installer.h"
#include "w25qxx_init.h"
#include "delay.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef void (*pFunction)(void);
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_METADATA_ADDRESS  0x08010000U
#define FIRST_FLASH_ADDRESS   (APP_METADATA_ADDRESS + sizeof(Metadata_t))
#define LAST_FLASH_ADDRESS    (0x82000000U)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi3;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

static const uint8_t PUBLIC_KEY[32] = {
    0x8f, 0x0d, 0xeb, 0x67, 0xb9, 0xfa, 0x75, 0xc5,
    0x68, 0x25, 0x00, 0x19, 0x86, 0x16, 0x91, 0xaf,
    0x6e, 0x00, 0x0b, 0x74, 0x64, 0x9f, 0xb3, 0xc0,
    0x5d, 0x8f, 0x65, 0x13, 0x9f, 0x0d, 0x65, 0x67
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI3_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch)
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART1 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  (void)GPIO_Pin;
  printf("Bootloader button interrupt\r\n");
}

static uint32_t Crc32(const uint8_t* data, size_t size)
{
  uint32_t r = ~0; const uint8_t *end = data + size;
 
  while(data < end)
  {
    r ^= *data++;
 
    for(int i = 0; i < 8; i++)
    {
      uint32_t t = ~((r&1) - 1); r = (r>>1) ^ (0xEDB88320 & t);
    }
  }
 
  return ~r;
}

static bool InRange(uint32_t val, uint32_t low, uint32_t high)
{
  return (val >= low) && (val <= high);
}

static bool IsMetadataValid(const Metadata_t* metadata)
{
  const uint32_t fwSignCrc = Crc32(metadata->firmwareSignature, sizeof(metadata->firmwareSignature));
  const uint32_t metaSignCrc = Crc32(metadata->metadataSignature, sizeof(metadata->metadataSignature));

  /* Add \0 terminator to metadata magic string */
  char metaStr[sizeof(metadata->magic) + 1U];
  memcpy(metaStr, metadata->magic, sizeof(metadata->magic));
  metaStr[sizeof(metadata->magic)] = '\0';

  printf("Metadata magic:           %s\r\n", metaStr);
  printf("Firmware type:            %lu\r\n", metadata->type);
  printf("Firmware version:         0x%lX\r\n", metadata->version);
  printf("Firmware rollback number: %lu\r\n", metadata->rollbackNumber);
  printf("Firmware ID:              0x%lX\r\n", metadata->firmwareId);
  printf("Firmware start address:   0x%lX\r\n", metadata->startAddress);
  printf("Firmware size:            0x%lX\r\n", metadata->firmwareSize);
  printf("Firmware name:            %s\r\n", metadata->name);
  printf("Firmware signature CRC32: 0x%lX\r\n", fwSignCrc);
  printf("Metadata signature CRC32: 0x%lX\r\n", metaSignCrc);

  if (0 != strcmp(metaStr, "_M_E_T_A_D_A_T_A"))
  {
    return false;
  }

  const uint32_t start = metadata->startAddress;
  const uint32_t end = metadata->startAddress + metadata->firmwareSize;

  if (!InRange(start, FIRST_FLASH_ADDRESS, LAST_FLASH_ADDRESS))
  {
    return false;
  }

  if (!InRange(end, FIRST_FLASH_ADDRESS, LAST_FLASH_ADDRESS))
  {
    return false;
  }

  const uint8_t* sign = metadata->metadataSignature;
  const uint8_t*  msg = (const uint8_t*)metadata;
  size_t          len = sizeof(Metadata_t) - sizeof(metadata->metadataSignature);
  
  if (ed25519_verify(sign, msg, len, PUBLIC_KEY))
  {
    return true;
  }
  
  return false;
}

static bool IsApplicationValid(const Metadata_t* metadata)
{
    const uint8_t* sig = metadata->firmwareSignature;
    const uint8_t* msg = (const uint8_t*)(metadata->startAddress);
    const size_t   len = metadata->firmwareSize;

    if (ed25519_verify(sig, msg, len, PUBLIC_KEY))
    {
      uint32_t sp = *(__IO uint32_t*)metadata->startAddress;
      uint32_t pc = *(__IO uint32_t*)(metadata->startAddress + 4);
    
      const bool stackPointerValid = sp == 0x20030000U;
      const bool programCounterValid = InRange(pc, FIRST_FLASH_ADDRESS, LAST_FLASH_ADDRESS);
    
      return stackPointerValid && programCounterValid;
    }

    printf("Firmware signature verification failed!\r\n");
    return false;
}

static void JumpTo(uint32_t address)
{
  printf("Jumping to application\r\n");
  
  uint32_t app_stack = *(__IO uint32_t*)address;
  uint32_t app_reset_handler = *(__IO uint32_t*)(address + 4);

  __disable_irq();
  for (int i = 0; i < 8; i++) {
      NVIC->ICER[i] = 0xFFFFFFFF;
      NVIC->ICPR[i] = 0xFFFFFFFF;
  }

  HAL_DeInit();
  HAL_RCC_DeInit();
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL  = 0;

  SCB->VTOR = address;

  __set_MSP(app_stack);

  pFunction app_entry = (pFunction)app_reset_handler;
  app_entry();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  MX_SPI3_Init();
  MX_USART3_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  printf("Bootloader initialized\r\n");

  w25qxx_handle_t* hnd = W25Q128_Init(&hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin);
  if (hnd == NULL)
  {
    printf("W25Q128_Init failed!\r\n");
  }
  else
  {
    printf("W25Q128_Init OK!\r\n");
  }

  InstallerKeys_t keys = {
    .metadataPubKey = PUBLIC_KEY,
    .firmwarePubKey = PUBLIC_KEY,
    .fragmentPubKey = NULL,
  };

  INSTALLER_InitAreas(hnd, &keys);

  const Metadata_t* metadata = (const Metadata_t*)(APP_METADATA_ADDRESS);

  if (IsMetadataValid(metadata))
  {
    if (IsApplicationValid(metadata))
    {
      JumpTo(metadata->startAddress);
    }
    else
    {
      printf("No valid application\r\n");
    }
  }
  else
  {
    printf("No valid metadata\r\n");
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin); // red LED
    HAL_Delay(500);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 168-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */
  HAL_TIM_Base_Start(&htim1);
  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD1_Pin|LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI3_CS_GPIO_Port, SPI3_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : USER_Btn_Pin */
  GPIO_InitStruct.Pin = USER_Btn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD1_Pin|LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_OverCurrent_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI3_CS_Pin */
  GPIO_InitStruct.Pin = SPI3_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(SPI3_CS_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void DELAY_us(uint16_t us)
{
  __HAL_TIM_SET_COUNTER(&htim1,0);
	while (__HAL_TIM_GET_COUNTER(&htim1) < us);
}

void DELAY_ms(uint32_t ms)
{
  for (uint32_t i = 0; i < ms; i++)
  {
    DELAY_us(1000U);
  }
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
  __disable_irq();
  while (1)
  {
  }
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
