#include "stm32l4xx.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_dac.h"
#include "stm32l4xx_ll_adc.h"
#include "stm32l4xx_ll_tim.h"
#include "stm32l4xx_ll_dma.h"
#include "arm_math.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>

//Konfiguracja
#define UART_BUF_SIZE    64
#define SAMPLE_RATE      8000  //f probkowania Hz
#define BUFFER_SIZE      128   //Rozmiar ping-pong
#define SIN_TABLE_SIZE   256   //Rozmiar tabeli sin dla generatora
#define DEFAULT_FREQ     1000  //Domyslna f generatora Hz

//Bufory i zmienne globalne
uint8_t uart_buf[UART_BUF_SIZE];
uint8_t uart_buf_idx = 0;

//Bufory ping-pong dla ADC/DAC
uint16_t adc_buffer_a[BUFFER_SIZE];
uint16_t adc_buffer_b[BUFFER_SIZE];
uint16_t dac_buffer_a[BUFFER_SIZE];
uint16_t dac_buffer_b[BUFFER_SIZE];

//Flagi i wskazniki buforow
volatile bool buffer_a_ready = false;
volatile bool buffer_b_ready = false;
volatile bool use_buffer_a = true;

//Generator sygnalu
uint16_t sin_table[SIN_TABLE_SIZE];
volatile uint32_t phase_accumulator = 0;
volatile uint32_t phase_increment = 0;
volatile bool generator_enabled = false;
volatile uint32_t generator_freq = DEFAULT_FREQ;

//Zmienne przetwarzania DSP
volatile float gain = 1.0f;
volatile bool processing_enabled = false;
volatile bool filter_enabled = false;

//Filtr FIR (dolnoprzepustowy)
#define FILTER_TAPS 32
float32_t fir_coeffs[FILTER_TAPS] = {
    0.0031f, 0.0045f, 0.0069f, 0.0103f, 0.0148f, 0.0203f, 0.0267f, 0.0340f,
    0.0419f, 0.0503f, 0.0589f, 0.0674f, 0.0754f, 0.0827f, 0.0889f, 0.0938f,
    0.0971f, 0.0987f, 0.0987f, 0.0971f, 0.0938f, 0.0889f, 0.0827f, 0.0754f,
    0.0674f, 0.0589f, 0.0503f, 0.0419f, 0.0340f, 0.0267f, 0.0203f, 0.0148f
};
float32_t fir_state[FILTER_TAPS + BUFFER_SIZE - 1];
arm_fir_instance_f32 fir_instance;

// Prototypy funkcji
void UART_Init(void);
void UART_SendString(const char *str);
void processCommand(char *cmd);
void System_Init(void);
void DAC_Init(void);
void ADC_Init(void);
void DMA_Init(void);
void TIM_Init(void);
void Generator_Init(void);
void DSP_Init(void);
void GenerateSinTable(void);
void SetGeneratorFrequency(uint32_t freq);
void ProcessBuffer(uint16_t* input_buf, uint16_t* output_buf);
void StartSignalProcessing(void);
void StopSignalProcessing(void);

//DMA
void DMA1_Channel1_IRQHandler(void);
void DMA1_Channel2_IRQHandler(void);
void TIM6_DAC_IRQHandler(void);

//Inicjalizacjia zeagrow
void System_Init(void){
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    FLASH->ACR |= FLASH_ACR_LATENCY_4WS; //Najwyzsze obroty

    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    while(LL_PWR_IsActiveFlag_VOS() != 0);

    LL_RCC_HSI_Enable();
    while(!LL_RCC_HSI_IsReady());

    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 10, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_Enable();
    LL_RCC_PLL_EnableDomain_SYS();
    while(!LL_RCC_PLL_IsReady());

    //Zmiana na PLL
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);

    SystemCoreClockUpdate();
}

//Inicjalizacja UART
void UART_Init(void) {
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

    LL_GPIO_InitTypeDef gpio_tx;
    LL_GPIO_StructInit(&gpio_tx);
    gpio_tx.Pin = LL_GPIO_PIN_2;
    gpio_tx.Mode = LL_GPIO_MODE_ALTERNATE;
    gpio_tx.Alternate = LL_GPIO_AF_7;
    LL_GPIO_Init(GPIOA, &gpio_tx);

    LL_GPIO_InitTypeDef gpio_rx;
    LL_GPIO_StructInit(&gpio_rx);
    gpio_rx.Pin = LL_GPIO_PIN_15;
    gpio_rx.Mode = LL_GPIO_MODE_ALTERNATE;
    gpio_rx.Alternate = LL_GPIO_AF_3;
    LL_GPIO_Init(GPIOA, &gpio_rx);

    LL_USART_Disable(USART2);

    LL_USART_InitTypeDef usart;
    LL_USART_StructInit(&usart);
    usart.BaudRate = 115200;
    usart.TransferDirection = LL_USART_DIRECTION_TX_RX;
    usart.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    usart.OverSampling = LL_USART_OVERSAMPLING_16;
    LL_USART_Init(USART2, &usart);

    LL_RCC_ClocksTypeDef clocks;
    LL_RCC_GetSystemClocksFreq(&clocks);
    USART2->BRR = (clocks.PCLK1_Frequency + (115200/2U))/115200;
    LL_USART_Enable(USART2);
}

//Send string
void UART_SendString(const char *s) {
    while(*s) {
        while(!LL_USART_IsActiveFlag_TXE(USART2));
        LL_USART_TransmitData8(USART2, *s++);
    }
    while(!LL_USART_IsActiveFlag_TC(USART2));
}

//Inicijalizacja DAC
void DAC_Init(void) {
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA); //Generator PA4
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1); //Wyjscie z przetwarzania PA5

    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_5, LL_GPIO_MODE_ANALOG);

    //DAC1 Channel 1 
    LL_DAC_SetTriggerSource(DAC1, LL_DAC_CHANNEL_1, LL_DAC_TRIG_TIM6_TRGO);
    LL_DAC_SetOutputBuffer(DAC1, LL_DAC_CHANNEL_1, LL_DAC_OUTPUT_BUFFER_ENABLE);
    LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_1);
    LL_DAC_EnableTrigger(DAC1, LL_DAC_CHANNEL_1);

    //DAC1 Channel 2
    LL_DAC_SetTriggerSource(DAC1, LL_DAC_CHANNEL2, LL_DAC_TRIG_TIM7_TRGO);
    LL_DAC_SetOutputBuffer(DAC1, LL_DAC_CHANNEL_2, LL_DAC_OUTPUT_BUFFER_ENABLE);
    LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_2);
    LL_DAC_EnableTrigger(DAC1, LL_DAC_CHANNEL_2);

}

//Inicjalizacja ADC
void ADC_Init(void){
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC);
    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_SYSCLK);

    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_0,LL_GPIO_MODE_ANALOG); //ADC

    //ADC Calibration
    LL_ADC_DisableDeepPowerDown(ADC1);
    LL_ADC_EnableInternalRegulator(ADC1);
    LL_mDelay(1);

    LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);
    while(LL_ADC_IsCalibrationOnGoing(ADC1));

    //Konfiguracja
    LL_ADC_SetResolution(ADC1,LL_ADC_RESOLUTION_12B);
    LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);
    LL_ADC_SetLowPowerMode(ADC1, LL_ADC_LP_MODE_NONE);

    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_TIM7_TRGO);
    LL_ADC_REG_SetTriggerEdge(ADC1, LL_ADC_REG_TRIG_EXT_RISING);
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);
    LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
    
    //Konfiguracja kanalow
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_5);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_5, LL_ADC_SAMPLINGTIME_12CYCLES_5);

    //Wlaczenie
    LL_ADC_Enable(ADC1);
    while(!LL_ADC_IsActiveFlag_ADRDY(ADC1));
}

//Inicjalizacja DMA
void DMA_Init(void){
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

    //DMA1 Kanał 1 - ADC 1
    LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_1,
        LL_DMA_DIRECTION_PERIPH_TO_MEMORY |
        LL_DMA_PRIORITY_HIGH |
        LL_DMA_MODE_CIRCULAR |
        LL_DMA_PERIPH_NOINCREMENT |
        LL_DMA_MEMORY_INCREMENT |
        LL_DMA_P_SIZE_HALFWORD |
        LL_DMA_M_SIZE_HALFWORD);

    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
        (uint32_t)&ADC1->DR,
        (uint32_t)adc_buffer_a,
        LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, BUFFER_SIZE * 2);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMA_REQUEST_0);

    //Przerwania
    LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    NVIC_SetPriority(DMA1_Channel1_IRQn,0);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    // DMA1 Channel 2 - DAC Channel 2
    LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_2,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH |
        LL_DMA_PRIORITY_HIGH |
        LL_DMA_MODE_CIRCULAR |
        LL_DMA_PERIPH_NOINCREMENT |
        LL_DMA_MEMORY_INCREMENT |
        LL_DMA_P_SIZE_HALFWORD |
        LL_DMA_M_SIZE_HALFWORD);

    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_2,
        (uint32_t)dac_buffer_a,
        (uint32_t)&DAC1->DHR12R2,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, BUFFER_SIZE * 2);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMA_REQUEST_3);

    LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_2);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);
    NVIC_SetPriority(DMA1_Channel2_IRQn, 1);
    NVIC_EnableIRQ(DMA1_Channel2_IRQn);    

}

// Inicjalizacja timerow
void TIM_Init(void) {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM6);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM7);
    
    // TIM6 - Generator trigger (rozne czestotliwosci)
    LL_TIM_SetPrescaler(TIM6, 0);
    LL_TIM_SetAutoReload(TIM6, (SystemCoreClock / (DEFAULT_FREQ * SIN_TABLE_SIZE)) - 1);
    LL_TIM_SetTriggerOutput(TIM6, LL_TIM_TRGO_UPDATE);
    LL_TIM_EnableIT_UPDATE(TIM6);
    NVIC_SetPriority(TIM6_DAC_IRQn, 2);
    NVIC_EnableIRQ(TIM6_DAC_IRQn);
    
    // TIM7 - ADC/DAC sampling trigger
    LL_TIM_SetPrescaler(TIM7, 0);
    LL_TIM_SetAutoReload(TIM7, (SystemCoreClock / SAMPLE_RATE) - 1);
    LL_TIM_SetTriggerOutput(TIM7, LL_TIM_TRGO_UPDATE);
}

//Inicjalizacja generatora sygnalu
void Generator_Init(void){
    GenerateSinTable();
    SetGeneratorFrequency(DEFAULT_FREQ);
}

//Inicjalizacja DSP
void DSP_Init(void){
    arm_fir_init_f32(&fir_instance, FIR_TAPS, fir_coeffs, fir_state, BUFFER_SIZE);
}

//Ustawienie f generatora
void SetGeneratorFrequency(uint32_t freq){
    generator_freq = freq;
    uint32_t timer_reload = (SystemCoreClock/(freq*SIN_TABLE_SIZE)) - 1;
    LL_TIM_SetAutoReload(TIM6, timer_reload);

    char msg[64];
    sprintf(msg, "Czestotliwosc generatora ustawiona na %lu Hz\r\n",freq);
    UART_SendString(msg);
}

//Przetwarzani bufroa
void ProcessBuffer(uint16_t* input_buf, uint16_t* output_buf){
    if(!processing_enabled){
        memcpy(output_buf, input_buf, BUFFER_SIZE * sizeof(uint16_t));
        return;
    }

    //konwersja do float
    float32_t float_input[BUFFER_SIZE];
    float32_t float_output[BUFFER_SIZE];

    for(int i = 0; i < BUFFER_SIZE; i++){
        float_input[i] = (float32_t)input_buf[i]/4095.0f; //Normalizacja
    }

    //Wzmocnienie
    arm_scale_f32(float_input, gain, float_output, BUFFER_SIZE);

    //Filtracja FIR(jesli wlaczona)
    if(filter_enabled){
        arm_fir_f32(&fir_instance, float_output, float_output, BUFFER_SIZE);
    }

    //Konwersja z powrotem
    for(int i = 0; i < BUFFER_SIZE; i++){
        float val = float_output[i] * 4095.0f;
        if(val > 4095.0f) val = 4095.0f;
        if(val < 0.0f) val = 0.0f;
        output_buf[i] = (uint16_t)val;
    }
}

//Start przetwarzania sygnalu
void StartSignalProcessing(void){
    LL_DMA_SetMemoryAddress(DMA1 LL_DMA_CHANNEL_1, (uint32_t)adc_buffer_a);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)adc_buffer_a);

    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
    
    LL_TIM_EnableCounter(TIM7);
    LL_ADC_REG_StartConversion(ADC1);
    
    UART_SendString("Signal processing started\r\n");
}

// Stop przetwarzania sygnalu
void StopSignalProcessing(void){
    LL_TIM_DisableCounter(TIM7);
    LL_ADC_REG_StopConversion(ADC1);
    
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    
    UART_SendString("Signal processing stopped\r\n");
}

// Obsluga przerwan DMA
void DMA1_Channel1_IRQHandler(void) {
    if(LL_DMA_IsActiveFlag_HT1(DMA1)) {
        LL_DMA_ClearFlag_HT1(DMA1);
        buffer_a_ready = true;
        use_buffer_a = false;
    }
    
    if(LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        buffer_b_ready = true;
        use_buffer_a = true;
    }
}

void DMA1_Channel2_IRQHandler(void) {
    if(LL_DMA_IsActiveFlag_HT2(DMA1)) {
        LL_DMA_ClearFlag_HT2(DMA1);
    }
    
    if(LL_DMA_IsActiveFlag_TC2(DMA1)) {
        LL_DMA_ClearFlag_TC2(DMA1);
    }
}

// Obsluga przerwania timera generatora
void TIM6_DAC_IRQHandler(void) {
    if(LL_TIM_IsActiveFlag_UPDATE(TIM6)){
        LL_TIM_ClearFlag_UPDATE(TIM6);
        
        if(generator_enabled) {
            uint32_t index = phase_accumulator >> 24; // 8 MSB
            LL_DAC_ConvertData12RightAligned(DAC1, LL_DAC_CHANNEL_1, sin_table[index]);
            phase_accumulator += phase_increment;
        }
    }
}

//Obsluga komend
void processCommand(char *cmd){
    if(strcmp(cmd,"help") == 0){
        UART_SendString(
            "\r\n==== Przetwarzanie sygnalu ====\r\n"
            "Generator:\r\n"
            " genon         - Wlacz generator\r\n"
            " genoff        - Wylacz generator\r\n"
            " freq <Hz>     - Ustaw czestotliwosc generatora\r\n"
            "Przetwarzanie:\r\n"
            " start         - Uruchom tor ADC/DAC\r\n"
            " stop          - Zatrzymaj tor ADC/DAC\r\n"
            " dsp on/off    - Wlacz/wylacz przetwarzanie DSP\r\n"
            " gain <val>    - Ustaw wzmocnienie (0.1-10.0)\r\n"
            " filter on/off - Wlacz/wylacz filtr FIR\r\n"
            " status        - Status systemu\r\n"
        );
    }
    else if(strcmp(cmd,"genon") == 0){
        generator_enabled = true;
        LL_TIM_EnableCounter(TIM6);
        UART_SendString("Generator wlaczony\r\n");
    }
    else if(strcmp(cmd,"genoff") == 0){
        generator_enabled = false;
        LL_TIM_DisableCounter(TIM6);
        LL_DAC_ConvertData12RightAligned(DAC1, LL_DAC_CHANNEL_1,2048);
        UART_SendString("Generator wylaczony\r\n");
    }
    else if(strstr(cmd,"freq ") == 0){
        uint32_t freq = atoi(cmd + 5);
        if(freq >= 10 && freq <= 10000){
            SetGeneratorFrequency(freq);
        }else{
            UART_SendString("Zarkes czestotliwosci musi byc z przedzialu: 10 - 10000 Hz\r\n");
        }
    }
    else if(strcmp(cmd,"start") == 0){
        StartSignalProcessing();
    }
    else if(strcmp(cmd,"stop") == 0){
        StopSignalProcessing();
    }
    else if(strcmp(cmd,"dsp on") == 0){
        processing_enabled = true;
        UART_SendString("DSP wlaczone\r\n");
    }
    else if(strcmp(cmd,"dsp off") == 0){
        processing_enabled = false;
        UART_SendString("DSP wylaczone\r\n");
    }
    else if(strstr(cmd,"gain ") == 0){
        float new_gain = atof(cmd + 5);
        if(new_gain >= 0.1f && new_gain <=10.0f){
            gain = new_gain;
            char msg[64];
            sprintf(msg,"Wzmocnienie ustawione na %.2f\r\n",gain);
            UART_SendString(msg);
        }else{
            UART_SendString("Zakres wzmocnienia musi byc z przedzialu: 0.1 - 10.0 \r\n");
        }
    }
    else if(strcmp(cmd,"filter on") == 0){
        filter_enabled = true;
        UART_SendString("Filtr FIR wlaczony\r\n");
    }
    else if(strcmp(cmd,"filter off") == 0){
        filter_enabled = false; 
        UART_SendString("Filtr FIR wylaczony\r\n");
    }
    else if(strcmp(cmd,"status") == 0){
        char msg[256];
        sprintf(msg,
            "\r\n--- Status ---\r\n"
            "Generator: %s, Freq: %lu Hz\r\n"
            "Processing: %s\r\n"
            "Gain: %.2f\r\n"
            "Filter: %s\r\n"
            "Sample rate: %d Hz\r\n",
            generator_enabled ? "ON" : "OFF", generator_freq,
            processing_enabled ? "ON" : "OFF",
            gain,
            filter_enabled ? "ON" : "OFF",
            SAMPLE_RATE);

        UART_SendString(msg);
    }else{
        UART_SendString("Nieznane polecenie! Wpisz 'help' zeby zobaczyc liste dostepnych polecen\r\n");
    }
}

int main(void){
    System_Init();
    UART_Init();
    DAC_Init();
    ADC_Init();
    DMA_Init();
    TIM_Init();
    Generator_Init();
    DSP_Init();

    UART_SendString(
        "\r\n==== Przetwarzanie sygnalu ====\r\n"
        "Wpisz 'pomoc' zeby wyswietlic dostepne komendy\r\n";
    );

    while(1){
        if(LL_USART_IsActiveFlag_RXNE(USART2)){
            char ch = LL_USART_ReceiveData8(USART2);
            if(ch == '\r' || ch == '\n'){
                uart_buf[uart_buf_idx] = '\0';
                processCommand((char*)uart_buf);
                uart_buf_idx = 0;
            }
            else if(uart_buf_idx < UART_BUF_SIZE - 1){
                uart_buf[uart_buf_idx++] = tolower(ch);
            }
            else if(uart_buf_idx > UART_BUF_SIZE - 1){
                uart_buf_idx = 0;
                UART_SendString("Bufor przepelniony\r\n");
            }
        }

        //Ping pong
        if(buffer_a_ready){
            buffer_a_ready = false;
            ProcessBuffer(adc_buffer_a, dac_buffer_a);
        }

        if(buffer_b_ready){
            buffer_b_ready = false;
            ProcessBuffer(adc_buffer_a, dac_buffer_b);
        }

        if(buffer_b_ready){
            buffer_b_ready = false;
            ProcessBuffer(adc_buffer_b, dac_buffer_b);
        }
    }
}