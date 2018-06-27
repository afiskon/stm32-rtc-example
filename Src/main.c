#include "main.h"
#include "stm32f4xx_hal.h"

/* USER CODE BEGIN Includes */
/* vim: set ai et ts=4 sw=4: */
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "st7735.h"
#include "fonts.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;
RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_RTC_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

#define NCOLORS 7
static const char colors[NCOLORS+1] = "wyrgbmc";

void UART_Printf(const char* fmt, ...) {
    char buff[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    HAL_UART_Transmit(&huart2, (uint8_t*)buff, strlen(buff),
                      HAL_MAX_DELAY);
    va_end(args);
}

void Fix_Date(RTC_DateTypeDef* date, bool set_max) {
    if(date->Date == 0) {
        date->Date = 1;
        return; // no need to re-check
    }

    if(date->Date <= 28) {
        return; // always OK
    }

    // since Year is 0..99 there is no need to
    // check (Year % 100) and (Year % 400) cases.
    bool is_leap = (date->Year % 4) == 0;
    uint8_t days_in_month = 31;
    // There are 30 days in April, June, September and November
    if((date->Month == 4) || (date->Month == 6) || (date->Month == 9) || (date->Month == 11)) {
        days_in_month = 30;
    } else if(date->Month == 2) { // February
        days_in_month = is_leap ? 29 : 28;
    }

    if(date->Date > days_in_month) {
        date->Date = set_max ? days_in_month : 1;
    }
}

int RTC_Set(
        uint8_t year, uint8_t month, uint8_t day,
        uint8_t hour, uint8_t min, uint8_t sec,
        uint8_t dow) {
    HAL_StatusTypeDef res;
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;

    memset(&time, 0, sizeof(time));
    memset(&date, 0, sizeof(date));

    date.WeekDay = dow;
    date.Year = year;
    date.Month = month;
    date.Date = day;

    res = HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
    if(res != HAL_OK) {
        UART_Printf("HAL_RTC_SetDate failed: %d\r\n", res);
        return -1;
    }

    time.Hours = hour;
    time.Minutes = min;
    time.Seconds = sec;

    res = HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
    if(res != HAL_OK) {
        UART_Printf("HAL_RTC_SetTime failed: %d\r\n", res);
        return -2;
    }

    return 0;
}

void init() {
    ST7735_Init();
    ST7735_FillScreen(ST7735_BLACK);

    HAL_PWR_EnableBkUpAccess();

    UART_Printf("Ready!\r\n");
}

uint16_t font_color(uint8_t clr, bool selected) {
    if(selected) {
        clr = (clr + 1) % NCOLORS;
    }

    switch(colors[clr]) {
        case 'w':
            return ST7735_WHITE;
        case 'y':
            return ST7735_YELLOW;
        case 'r':
            return ST7735_RED;
        case 'g':
            return ST7735_GREEN;
        case 'b':
            return ST7735_BLUE;
        case 'm':
            return ST7735_MAGENTA;
        case 'c':
            return ST7735_CYAN;
        default: // should never happen, return gray
            return ST7735_COLOR565(15, 31, 15);
    }
}

uint8_t cursor_offset = 0;

void loop() {
    HAL_Delay(100);

    bool btn_dec = false;
    bool btn_inc = false;

    if(HAL_GPIO_ReadPin(Btn1_GPIO_Port, Btn1_Pin) == GPIO_PIN_RESET) {
        cursor_offset = (cursor_offset == 0) ? 7 : cursor_offset - 1;
    }

    if(HAL_GPIO_ReadPin(Btn2_GPIO_Port, Btn2_Pin) == GPIO_PIN_RESET) {
        cursor_offset = (cursor_offset + 1) % 9;
    }

    if(HAL_GPIO_ReadPin(Btn3_GPIO_Port, Btn3_Pin) == GPIO_PIN_RESET) {
        btn_dec = true;
    }

    if(HAL_GPIO_ReadPin(Btn4_GPIO_Port, Btn4_Pin) == GPIO_PIN_RESET) {
        btn_inc = true;
    }

    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    HAL_StatusTypeDef res;

    res = HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    if(res != HAL_OK) {
        UART_Printf("HAL_RTC_GetTime failed: %d\r\n", res);
        return;
    }

    res = HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
    if(res != HAL_OK) {
        UART_Printf("HAL_RTC_GetDate failed: %d\r\n", res);
        return;
    }

    uint8_t chosen_color = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);
    chosen_color = chosen_color % NCOLORS;
    uint16_t default_color = font_color(chosen_color, false);

    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%02d", time.Hours);
    ST7735_WriteString(11*0, 18*0, tmp, Font_11x18, font_color(chosen_color, (cursor_offset == 1)), ST7735_BLACK);
    ST7735_WriteString(11*2, 18*0, ":", Font_11x18, default_color, ST7735_BLACK);
    snprintf(tmp, sizeof(tmp), "%02d", time.Minutes);
    ST7735_WriteString(11*3, 18*0, tmp, Font_11x18, font_color(chosen_color, (cursor_offset == 2)), ST7735_BLACK);
    ST7735_WriteString(11*5, 18*0, ":", Font_11x18, default_color, ST7735_BLACK);
    snprintf(tmp, sizeof(tmp), "%02d", time.Seconds);
    ST7735_WriteString(11*6, 18*0, tmp, Font_11x18, font_color(chosen_color, (cursor_offset == 3)), ST7735_BLACK);

    snprintf(tmp, sizeof(tmp), "%02d", date.Date);
    ST7735_WriteString(11*0, 18*1, tmp, Font_11x18, font_color(chosen_color, (cursor_offset == 4)), ST7735_BLACK);
    ST7735_WriteString(11*2, 18*1, ".", Font_11x18, default_color, ST7735_BLACK);
    snprintf(tmp, sizeof(tmp), "%02d", date.Month);
    ST7735_WriteString(11*3, 18*1, tmp, Font_11x18, font_color(chosen_color, (cursor_offset == 5)), ST7735_BLACK);
    ST7735_WriteString(11*5, 18*1, ".", Font_11x18, default_color, ST7735_BLACK);
    snprintf(tmp, sizeof(tmp), "%02d", date.Year);
    ST7735_WriteString(11*6, 18*1, tmp, Font_11x18, font_color(chosen_color, (cursor_offset == 6)), ST7735_BLACK);

    ST7735_WriteString(11*0, 18*2, "DOW:  ", Font_11x18, default_color, ST7735_BLACK);
    snprintf(tmp, sizeof(tmp), "%02d", date.WeekDay);
    ST7735_WriteString(11*6, 18*2, tmp, Font_11x18, font_color(chosen_color, (cursor_offset == 7)), ST7735_BLACK);

    ST7735_WriteString(11*0, 18*3, "Color: ", Font_11x18, default_color, ST7735_BLACK);
    snprintf(tmp, sizeof(tmp), "%c/%c", colors[chosen_color], colors[(chosen_color + 1) % NCOLORS]);
    ST7735_WriteString(11*7, 18*3, tmp, Font_11x18, font_color(chosen_color, (cursor_offset == 8)), ST7735_BLACK);

    if(cursor_offset == 1) { // edit hour
        if(btn_dec) {
            if(time.Hours == 0) {
                time.Hours = 23;
            } else {
                time.Hours--;
            }
        } else if(btn_inc) {
            time.Hours = (time.Hours + 1) % 24;
        }
    }

    if(cursor_offset == 2) { // edit minute
        if(btn_dec) {
            if(time.Minutes == 0) {
                time.Minutes = 59;
            } else {
                time.Minutes--;
            }
        } else if(btn_inc) {
            time.Minutes = (time.Minutes + 1) % 60;
        }
    }

    if(cursor_offset == 3) { // edit second
        if(btn_dec) {
            if(time.Seconds == 0) {
                time.Seconds = 59;
            } else {
                time.Seconds--;
            }
        } else if(btn_inc) {
            time.Seconds = (time.Seconds + 1) % 60;
        }
    }

    if(cursor_offset == 4) { // edit day
        if(btn_dec) {
            if(date.Date == 1) {
                date.Date = 31;
                Fix_Date(&date, true);
            } else {
                date.Date--;
            }
        } else if(btn_inc) {
            date.Date++;
            Fix_Date(&date, false);
        }
    }

    if(cursor_offset == 5) { // edit month
        if(btn_dec) {
            if(date.Month == 1) {
                date.Month = 12;
            } else {
                date.Month--;
            }
        } else if(btn_inc) {
            if(date.Month == 12) {
                date.Month = 1;
            } else {
                date.Month++;
            }
        }

        Fix_Date(&date, true);
    }

    if(cursor_offset == 6) { // edit year
        if(btn_dec) {
            if(date.Year == 0) {
                date.Year = 99;
            } else {
                date.Year--;
            }
        } else if(btn_inc) {
            date.Year = (date.Year + 1) % 100;
        }
        Fix_Date(&date, true);
    }

    if(cursor_offset == 7) { // edit day of week
        if(btn_dec) {
            if(date.WeekDay == 1) {
                date.WeekDay = 7;
            } else {
                date.WeekDay--;
            }
        } else if(btn_inc) {
            if(date.WeekDay == 7) {
                date.WeekDay = 1;
            } else {
                date.WeekDay++;
            }
        }
    }

    if(cursor_offset == 8) { // edir color
        if(btn_dec) {
            if(chosen_color == 0) {
                chosen_color = NCOLORS-1;
            } else {
                chosen_color--;
            }
        } else if(btn_inc) {
            chosen_color = (chosen_color + 1) % NCOLORS;
        }
    }

    // Don't re-use RTC_*TypeDef structures filled by HAL_RTC_GetTime/GetDate
    // It causes bugs like 25th hour
    if((cursor_offset >= 1) && (cursor_offset <= 7) && (btn_dec || btn_inc)) {
        RTC_Set(
           date.Year,  date.Month, date.Date,      // Y, M, D
           time.Hours, time.Minutes, time.Seconds, // H, M, S
           date.WeekDay // day of week, 1-7 (mon-sun)
        );
    }

    if((cursor_offset == 8) && (btn_dec || btn_inc)) {
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, (uint32_t)chosen_color);
    }
}

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_RTC_Init();

  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  init();
  while (1)
  {
    loop();
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

    /**Configure the main internal regulator output voltage 
    */
  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* RTC init function */
static void MX_RTC_Init(void)
{

    /**Initialize RTC Only 
    */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initialize RTC and set the Time and Date 
    */
  if(HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0x32F2){
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    HAL_RTCEx_BKUPWrite(&hrtc,RTC_BKP_DR0,0x32F2);
  }

}

/* SPI1 init function */
static void MX_SPI1_Init(void)
{

  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Btn2_Pin Btn3_Pin Btn4_Pin */
  GPIO_InitStruct.Pin = Btn2_Pin|Btn3_Pin|Btn4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : Btn1_Pin */
  GPIO_InitStruct.Pin = Btn1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Btn1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void _Error_Handler(char * file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler_Debug */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
