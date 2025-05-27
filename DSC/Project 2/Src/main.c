#include "stm32l4xx.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_pwr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

//Bufor UART
#define UART_BUF_SIZE 64
uint8_t uart_buf[UART_BUF_SIZE];
uint8_t uart_buf_idx = 0;

// Prototypy
void UART_Init(void);
void MCO_Init(void);
void LSE_Init(void);
void UART_SendString(const char *str);
void processCommand(char *cmd);
void SetMCO(const char *source, uint32_t prescaler);
void SetFlashLatency(uint32_t frequency);
void CalibrateMSI(void);

const char *mco_source = "off";
uint32_t mco_prescaler = 1;
bool lse_ready = false;

// Inicjalizacja MCO na PA8
void MCO_Init(void) {
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_8, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_8, LL_GPIO_AF_0);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_8, LL_GPIO_SPEED_FREQ_HIGH); //maksymalna szybkosc przelaczania
}

// Inicjalizacja LSE
void LSE_Init(void) {
    LL_PWR_EnableBkUpAccess(); //Odblokowanie
    LL_RCC_LSE_DisableBypass(); //Wylacza bypass
    LL_RCC_LSE_Enable();

    // Timeout 5 sekund
    uint32_t timeout = 500;
    SystemCoreClockUpdate();
    LL_Init1msTick(SystemCoreClock);
    while (!LL_RCC_LSE_IsReady() && timeout--){
    	LL_mDelay(10);
    }

    UART_Init();
    if(LL_RCC_LSE_IsReady()) {
        lse_ready = true;
        UART_SendString("LSE gotowe (32.768 kHz)\r\n");
    } else {
        UART_SendString("Blad inicjalizacji LSE!\r\n");
    }
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

    LL_USART_Disable(USART2);

    // 3) Inicjalizacja USART2
    LL_USART_InitTypeDef usart;
    LL_USART_StructInit(&usart);
    usart.BaudRate            = 115200;
    usart.TransferDirection   = LL_USART_DIRECTION_TX_RX;
    usart.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    usart.OverSampling = LL_USART_OVERSAMPLING_16;
    LL_USART_Init(USART2, &usart);

    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    USART2->BRR = (clocks.PCLK1_Frequency + (115200/2U))/115200;
    LL_USART_Enable(USART2);
}

void UART_SendString(const char *s){
    while(*s){
        while(!LL_USART_IsActiveFlag_TXE(USART2));
        LL_USART_TransmitData8(USART2, *s++);
    }
    //Poczekaj na zakonczenie transmisji ostatniego znaku
    while(!LL_USART_IsActiveFlag_TC(USART2));
}

void SetFlashLatency(uint32_t frequency){
    //CPU dziala za syzbko dla pamieci wiec CPU musi czekac za pomoca wait states
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    if(frequency > 16000000) FLASH->ACR |= FLASH_ACR_LATENCY_4WS;
    else if(frequency > 8000000) FLASH->ACR |= FLASH_ACR_LATENCY_2WS;
    else FLASH->ACR |= FLASH_ACR_LATENCY_0WS;
}

void SetMCO(const char *source, uint32_t prescaler){
    if (strcmp(source, "off") == 0) {
        LL_RCC_ConfigMCO(LL_RCC_MCO1SOURCE_NOCLOCK, LL_RCC_MCO1_DIV_1);
        mco_source = "off";
    }else{
        uint32_t mco_src = 0;
        if(strcmp(source, "sys") == 0) mco_src = LL_RCC_MCO1SOURCE_SYSCLK;
        else if(strcmp(source, "hsi") == 0) mco_src = LL_RCC_MCO1SOURCE_HSI;
        else if(strcmp(source, "msi") == 0) mco_src = LL_RCC_MCO1SOURCE_MSI;
        else if(strcmp(source, "pll") == 0) mco_src = LL_RCC_MCO1SOURCE_PLLCLK;
        else if(strcmp(source, "lse") == 0) mco_src = LL_RCC_MCO1SOURCE_LSE;
        else {
            UART_SendString("Nieznane zrodlo MCO\r\n");
            return;
        }

        LL_RCC_ConfigMCO(mco_src, prescaler);
        mco_source = source;
        mco_prescaler = (1 << (prescaler >> RCC_CFGR_MCOPRE_Pos));
    }
}

// Kalibracja MSI za pomoca LSE
void CalibrateMSI(void) {
    if(!lse_ready) {
        UART_SendString("LSE nieaktywne! Wlacz najpierw LSE\r\n");
        return;
    }

    // Wlacz kalibracje przez PLLMSI
    LL_RCC_MSI_EnablePLLMode();
    while(!LL_RCC_MSI_IsReady());

    UART_SendString("Kalibracja MSI w toku...\r\n");

    // Czekaj na stabilizacje (moze zajac do 100 ms)
    uint32_t timeout = 100000;
    while(!LL_RCC_MSI_IsReady() && timeout--);

    if(timeout > 0) {
        char msg[64];
        sprintf(msg, "MSI skalibrowane! Dokladnosc: +-0.25%%\r\n");
        UART_SendString(msg);
    } else {
        UART_SendString("Blad kalibracji!\r\n");
    }
}

void processCommand(char *cmd){
    // Pomoc
    if (strcmp(cmd, "help") == 0) {
        UART_SendString(
            "\r\n==== System Zegarowy STM32L432KC ====\r\n"
            "Dostepne komendy:\r\n"
            " msi[4|8|16|24|48] - Ustaw MSI z czestotliwoscia (MHz)\r\n"
            " hsi               - Wlacz HSI 16 MHz\r\n"
            " pll               - Uruchom PLL 80 MHz\r\n"
            " lseon             - Wlacz LSE 32.768 kHz\r\n"
            " calibrate         - Kalibruj MSI uzywajac LSE\r\n"
            " mco[off|sys|hsi|msi|pll][1|2|4] - Konfiguracja MCO\r\n"
            " status            - Aktualne ustawienia\r\n"
            " help              - Wyswietl te informacje\r\n"
            "Przyklady:\r\n"
            " msi48    - MSI 48 MHz\r\n"
            " mcosys2  - MCO = SYSCLK/2\r\n"
            " mcooff   - Wylacz MCO\r\n"
        );
    }

    // MSI
    else if (strstr(cmd, "msi") == cmd) {
        int freq = atoi(cmd + 3);
        LL_RCC_MSI_Enable();
        while (!LL_RCC_MSI_IsReady());

        uint32_t range;
        switch(freq){
            case 4:  range = LL_RCC_MSIRANGE_6; break;
            case 8:  range = LL_RCC_MSIRANGE_7; break;
            case 16: range = LL_RCC_MSIRANGE_8; break;
            case 24: range = LL_RCC_MSIRANGE_9; break;
            case 48: range = LL_RCC_MSIRANGE_11; break;
            default:
                UART_SendString("Niepoprawna czestotliwosc MSI (4/8/16/24/48 MHz)\r\n");
                return;
        }

        SetFlashLatency(freq * 1000000);

        LL_RCC_MSI_EnableRangeSelection();
        LL_RCC_MSI_SetRange(range);

        LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_MSI);
        while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_MSI);
        SystemCoreClockUpdate();
        UART_Init();

        char msg[64];
        sprintf(msg, "SYSCLK: %lu MHz (MSI)\r\n", SystemCoreClock/1000000);
        UART_SendString(msg);
    }

    // HSI
    else if (strcmp(cmd, "hsi") == 0) {
        LL_RCC_HSI_Enable();
        while (!LL_RCC_HSI_IsReady());

        LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
        LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
        LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

        SetFlashLatency(16000000);

        LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
        while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI);
        SystemCoreClockUpdate();
        UART_Init();
        UART_SendString("SYSCLK: 16 MHz (HSI)\r\n");
    }

    // PLL
    else if (strcmp(cmd, "pll") == 0) {
    	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    	SetFlashLatency(80000000);

    	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    	while(LL_PWR_IsActiveFlag_VOS() != 0);

    	UART_SendString("PLL: Enable HSI\r\n");
        LL_RCC_HSI_Enable();
        while (!LL_RCC_HSI_IsReady());

        UART_SendString("PLL: Switch SYSCLK to HSI\r\n");
        LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
        while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI);

        UART_Init();

        UART_SendString("PLL: Disable PLL\r\n");
        LL_RCC_PLL_Disable();
        while(LL_RCC_PLL_IsReady());

        UART_SendString("PLL: Set prescalers\r\n");
        LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
        LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
        LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

        LL_RCC_PLL_ConfigDomain_SYS(
        		LL_RCC_PLLSOURCE_HSI,
				LL_RCC_PLLM_DIV_1,
				10,
				LL_RCC_PLLR_DIV_2);

        UART_SendString("PLL: Enable PLL\r\n");
        LL_RCC_PLL_Enable();
        LL_RCC_PLL_EnableDomain_SYS();
        while (!LL_RCC_PLL_IsReady());

        UART_SendString("PLL: Switch SYSCLK to PLL\r\n");
        LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

        uint32_t timeout = 2000000;
        while((LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) && timeout--);

        if(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL){
        	UART_SendString("Błąd: nie udalo sie przelaczyc na PLL\r\n");
        	return;
        }

        SystemCoreClockUpdate();
        UART_Init();
        UART_SendString("SYSCLK: 80 MHz (PLL)\r\n");
    }

    // LSE
    else if (strcmp(cmd, "lseon") == 0) {
        LSE_Init();
    }

    // Kalibracja
    else if (strcmp(cmd, "calibrate") == 0) {
        CalibrateMSI();
    }

    // MCO
    else if (strstr(cmd, "mco") == cmd) {
        char source[8] = {0};
        uint32_t prescaler = LL_RCC_MCO1_DIV_1;

        // Parsowanie
        if(strstr(cmd, "mcooff")) {
            strcpy(source, "off");
        }
        else if(sscanf(cmd, "mco%3[^0-9]%lu", source, &prescaler) >= 1) {
            prescaler = (prescaler == 2) ? LL_RCC_MCO1_DIV_2 :
                       (prescaler == 4) ? LL_RCC_MCO1_DIV_4 :
                       LL_RCC_MCO1_DIV_1;
        }
        else {
            UART_SendString("Niepoprawna skladnia MCO! Przyklad: mcosys2\r\n");
            return;
        }

        SetMCO(source, prescaler);
        char msg[64];
        sprintf(msg, "MCO: %s /%lu\r\n", source, (1 << (prescaler >> RCC_CFGR_MCOPRE_Pos)));
        UART_SendString(msg);
    }

    // Status
    else if (strcmp(cmd, "status") == 0) {
        LL_RCC_ClocksTypeDef clocks;
        LL_RCC_GetSystemClocksFreq(&clocks);

        char msg[128];
        sprintf(msg,
            "\r\n--- Status ---\r\n"
            "SYSCLK: %lu MHz\r\n"
            "MCO: %s /%lu\r\n"
            "Flash latency: %lu WS\r\n"
            "LSE: %s\r\n",
            clocks.SYSCLK_Frequency/1000000,
            mco_source,
            mco_prescaler,
            (FLASH->ACR & FLASH_ACR_LATENCY) >> FLASH_ACR_LATENCY_Pos,
            lse_ready ? "aktywne" : "nieaktywne"
        );
        UART_SendString(msg);
    }

    else {
        UART_SendString("Nieznana komenda! Wpisz 'help' aby wyswietlic pomoc\r\n");
    }
}

int main(void){
    SystemCoreClockUpdate();
    LL_InitTick(SystemCoreClock, 1000);
    UART_Init();
    MCO_Init();

    UART_SendString(
        "\r\n==== System Zegarowy STM32L432KC ====\r\n"
        "Wpisz 'help' aby wyswietlic dostepne komendy\r\n"
    );

    while (1){
        if (LL_USART_IsActiveFlag_RXNE(USART2)) {
            char ch = LL_USART_ReceiveData8(USART2);
            if (ch == '\r' || ch == '\n') {
                uart_buf[uart_buf_idx] = '\0';
                processCommand((char*)uart_buf);
                uart_buf_idx = 0;
            }
            else if (uart_buf_idx < UART_BUF_SIZE - 1) {
                uart_buf[uart_buf_idx++] = tolower(ch);
            }
        }
    }
}
