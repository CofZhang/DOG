#include "led.h"

extern TIM_HandleTypeDef htim17;

void LED1_On(void)  { HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET); }
void LED1_Off(void) { HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET); }

void LED2_On(void)  { HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET); }
void LED2_Off(void) { HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET); }

void LED3_On(void)  { HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET); }
void LED3_Off(void) { HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET); }

void LED4_On(void)  { HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET); }
void LED4_Off(void) { HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET); }

void LED5_On(void)  { HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, GPIO_PIN_SET); }
void LED5_Off(void) { HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, GPIO_PIN_RESET); }

void LED6_On(void)  { HAL_GPIO_WritePin(LED6_GPIO_Port, LED6_Pin, GPIO_PIN_SET); }
void LED6_Off(void) { HAL_GPIO_WritePin(LED6_GPIO_Port, LED6_Pin, GPIO_PIN_RESET); }

void LED7_On(void)  { HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, GPIO_PIN_SET); }
void LED7_Off(void) { HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, GPIO_PIN_RESET); }

void LED8_On(void)  { HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, GPIO_PIN_SET); }
void LED8_Off(void) { HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, GPIO_PIN_RESET); }

//void BEEP_On(void)  { HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET); }
//void BEEP_Off(void) { HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET); }

void Beep_Init(void)
{
	HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1);
}

// 蜂鸣器发声：freq = 频率(Hz)
void Buzzer_Beep(uint16_t freq)
{
	if(freq == 0)
	{
		BEEP_Off();
		return;
	}
	
	uint32_t tmr_clk = 500000;   // 500kHz 计数时钟（120MHz APB2 / (239+1) = 500kHz）
	uint32_t period = tmr_clk / freq - 1;
	uint32_t duty = period / 2;   // 50%占空比，声音最大
	
	__HAL_TIM_SET_AUTORELOAD(&htim17, period);
	__HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, duty);
	
	HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
}

void BEEP_On(void)
{
	Buzzer_Beep(1000);
}

void BEEP_Off(void)
{
	HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1);
}
