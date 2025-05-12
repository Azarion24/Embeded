#include <stm32l4xx_ll_bus.h>
#include <stm32l4xx_ll_gpio.h>
#include <stm32l4xx_ll_usart.h>
#include <stm32l4xx_ll_tim.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "stm32l4xx.h"

volatile uint32_t uwTick = 0;
#define UART_BUF_SIZE 32
uint8_t uart_buf[UART_BUF_SIZE];
uint8_t uart_buf_idx = 0;
char response[150]; // Bufor na odpowiedz

uint8_t led_brightness = 50;  // Poczatkowe wypelnienie PWM 50%
uint8_t blink_frequency = 0; // Domyslnie dioda nie miga (0 Hz)
bool is_blinking = false;     // Flaga, czy dioda miga

void SysTick_Handler(void){
    uwTick++;
}

void LL_InitTick(uint32_t HCLKFrequency, uint32_t Ticks){
    SysTick_Config(HCLKFrequency / 1000); // Konfiguracja SysTick do generowania przerwań co 1 ms
}

uint32_t LL_GetTick(void){
    return uwTick;
}

void UART_Init(void){
    // 1) Wlaczenie zegarow dla GPIOA i USART2
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

    // 2) Konfiguracja PA2 jako TX (AF7) i PA15 jako RX (AF3)
    LL_GPIO_InitTypeDef gpio_tx;
    LL_GPIO_StructInit(&gpio_tx);
    gpio_tx.Pin       = LL_GPIO_PIN_2;
    gpio_tx.Mode      = LL_GPIO_MODE_ALTERNATE;
    gpio_tx.Alternate = LL_GPIO_AF_7;
    LL_GPIO_Init(GPIOA, &gpio_tx);

    LL_GPIO_InitTypeDef gpio_rx;
    LL_GPIO_StructInit(&gpio_rx);
    gpio_rx.Pin       = LL_GPIO_PIN_15;
    gpio_rx.Mode      = LL_GPIO_MODE_ALTERNATE;
    gpio_rx.Alternate = LL_GPIO_AF_3;
    LL_GPIO_Init(GPIOA, &gpio_rx);

    // 3) Inicjalizacja USART2
    LL_USART_InitTypeDef usart;
    LL_USART_StructInit(&usart);
    usart.BaudRate            = 115200;
    usart.TransferDirection   = LL_USART_DIRECTION_TX_RX;
    LL_USART_Init(USART2, &usart);
    LL_USART_Enable(USART2);
}

void PWM_Init(void){
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);

    LL_GPIO_InitTypeDef gpio;
    LL_GPIO_StructInit(&gpio);
    gpio.Pin       = LL_GPIO_PIN_3;
    gpio.Mode      = LL_GPIO_MODE_ALTERNATE;
    gpio.Alternate = LL_GPIO_AF_1;
    LL_GPIO_Init(GPIOB, &gpio);

    LL_TIM_InitTypeDef tim;
    LL_TIM_StructInit(&tim);
    tim.Prescaler     = 79;   // 80 MHz / 80 = 1 MHz
    tim.CounterMode   = LL_TIM_COUNTERMODE_UP;
    tim.Autoreload    = 999;  // 1 kHz PWM
    LL_TIM_Init(TIM2, &tim);

    LL_TIM_OC_InitTypeDef oc;
    LL_TIM_OC_StructInit(&oc);
    oc.OCMode       = LL_TIM_OCMODE_PWM1;
    oc.OCState      = LL_TIM_OCSTATE_ENABLE;
    oc.CompareValue = 500;    // 50% wypelnienie
    LL_TIM_OC_Init(TIM2, LL_TIM_CHANNEL_CH2, &oc);

    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH2);
    LL_TIM_EnableCounter(TIM2);
}

void UART_SendString(const char *s){
    while(*s){
        while(!LL_USART_IsActiveFlag_TXE(USART2));
        LL_USART_TransmitData8(USART2, *s++);
    }
}

void LED_SetBrightness(uint8_t percent){
    uint32_t arr = LL_TIM_GetAutoReload(TIM2);
    uint32_t ccr = (percent * (arr + 1)) / 100;
    LL_TIM_OC_SetCompareCH2(TIM2, ccr);
}

void LED_Blink(void){
    static uint32_t last_time = 0;
    static bool led_on = true;
    uint32_t current_time = LL_GetTick();

    // Jesli czestotliwosc wynosi 0, dioda nie miga
    if (blink_frequency == 0) return;

    // Sprawdzamy, czy minal czas dla migania (czas dla czestotliwosci w Hz)
    if (current_time - last_time >= (1000 / blink_frequency)) {
            if (led_on) {
                LED_SetBrightness(0);
                led_on = false;
            } else {
                LED_SetBrightness(led_brightness);
                led_on = true;
            }
            last_time = current_time;
        }
}

void process_uart_input(void){
    if(LL_USART_IsActiveFlag_RXNE(USART2) || LL_USART_IsActiveFlag_ORE(USART2)){
        if(LL_USART_IsActiveFlag_ORE(USART2)){
            LL_USART_ClearFlag_ORE(USART2);
        }
        uint8_t ch = LL_USART_ReceiveData8(USART2);
        if(ch == '\r' || ch == '\n'){
            if(uart_buf_idx > 0){
                uart_buf[uart_buf_idx] = '\0';  // Zakonczenie ciagu

                // Jesli uzytkownik podal liczbe bezposrednio, np. 123
                if(uart_buf[0] == '1'){ // Ustawienie PWM
                    int val = atoi((char*)uart_buf + 1); // Pomijamy pierwszy znak ('1')
                    val = val < 0 ? 0 : (val > 100 ? 100 : val);
                    led_brightness = val;
                    LED_SetBrightness(led_brightness); // Ustawienie jasnosci PWM
                    sprintf(response, "Jasnosc PWM ustawiona na: %d%%\r\n", val);
                    UART_SendString(response);
                }
                else if(uart_buf[0] == '2'){ // Ustawienie migania
                    int freq = atoi((char*)uart_buf + 1); // Pomijamy pierwszy znak ('2')
                    if(freq >= 1 && freq <= 100) { // Zakladajac zakres czestotliwosci od 1 Hz do 100 Hz
                        blink_frequency = freq;
                        is_blinking = true;
                        LED_SetBrightness(led_brightness);  // Zachowujemy aktualne wypelnienie PWM
                        sprintf(response, "Dioda zaczyna migac z czestotliwoscia: %d Hz i jasnoscia: %d%%\r\n", blink_frequency, led_brightness);
                    } else {
                        UART_SendString("Niepoprawna czestotliwosc. Zakres: 1-100 Hz\r\n");
                    }
                    UART_SendString(response);
                }
                uart_buf_idx = 0;
            }
        } else if(isdigit(ch) && uart_buf_idx < UART_BUF_SIZE-1){
            uart_buf[uart_buf_idx++] = ch;
        } else {
            UART_SendString("Wprowadzono bledne dane.\r\n");
            uart_buf_idx = 0;
        }
    }
}

int main(void){

	SystemCoreClockUpdate();
	LL_InitTick(SystemCoreClock, 1000);

	UART_Init();
	PWM_Init();

	UART_SendString("Witaj w programie na przemiot DSC\r\n");
	UART_SendString("[1] Ustaw jasnosc PWM (0-100%)\r\n");
	UART_SendString("[2] Miganie diody z czestotliwoscia (1-100 Hz)\r\n");

	// Poczatkowe ustawienie jasnosci PWM na 50% (dioda nie miga)
	LED_SetBrightness(50);

	while(1){
	    if(is_blinking) {
	        LED_Blink();  // Miganie dioda
	    }
	    process_uart_input();  // Przetwarzanie danych wejsciowych z UART
	}
}
