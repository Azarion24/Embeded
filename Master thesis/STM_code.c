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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// Definicje dla LIS3DH
#define LIS3DH_WHO_AM_I         0x0F
#define LIS3DH_WHO_AM_I_RESP    0x33
#define LIS3DH_OUT_X_L          0x28
#define CTRL_REG1               0x20
#define CTRL_REG4               0x23

#define MOTION_THRESHOLD        1.5f   // Próg różnicy dla wykrycia ruchu [g]
#define INACTIVITY_TIMEOUT_MS   60000  // 1 minuta braku ruchu [ms]
#define SAMPLE_INTERVAL_MS      100    // Odstęp między pomiarami [ms]
#define SEND_INTERVAL_MS        15000    // Interwał wysyłania komunikatu [ms]

// Definicje dla GPS
#define GPS_BUFFER_SIZE         256

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

//Akcelerometr
typedef struct {
    float x, y, z;
} AccelerationData;

//GPS
typedef struct {
    float decimalLat;
    float decimalLong;
    char formattedTime[20];
    float speed;        // w węzłach
    float altitude;
    bool valid;// Flaga ważności danych
} GPSData;

// Stan systemu
AccelerationData prevAccel = {0};
bool isMoving = false;
bool gsmInitialized = false;
uint32_t lastMotionTime = 0;
uint32_t lastSampleTime = 0;
uint32_t lastSendTime = 0;

// GPS
uint8_t rxBuffer[GPS_BUFFER_SIZE] = {0};
uint8_t rxIndex = 0;
uint8_t rxData;
GPSData gpsData = {0};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

// Funkcje dla akcelerometru
void LIS3DH_Init(void);
uint8_t LIS3DH_ReadRegister(uint8_t reg);
void LIS3DH_ReadXYZ(int16_t *x, int16_t *y, int16_t *z);
float ConvertToG(int16_t raw);
bool CheckForMotion(AccelerationData current, AccelerationData previous);
void ProcessAccelerometer(void);

// Funkcje dla GPS
float nmeaToDecimal(float coordinate);
void formatDateTime(float utcTime, int dateInt, char *buffer, size_t bufferSize);
void gpsParse(char *strParse);
int gpsValidate(char *nmea);

// GSM/Firebase
bool initGSM(void);
void closeGSM(void);
void sendToFirebase(const char* jsonData);
void sendCurrentLocation(void);
void sendCurrentStatus(bool moving);
void sendTripBegin(void);
void sendTripEnd(void);
void sendLastParking(void);

void safeDelay(uint32_t ms);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void safeDelay(uint32_t ms) {
    uint32_t tickstart = HAL_GetTick();
    while ((HAL_GetTick() - tickstart) < ms) {
        // Można tutaj dodać inne zadania w tle
        HAL_Delay(1);
    }
}

void LIS3DH_Init(void) {
    // Konfiguracja CTRL_REG1 - 10Hz, włącz wszystkie osie
    uint8_t ctrl_reg1[] = {CTRL_REG1, 0x27};
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, ctrl_reg1, sizeof(ctrl_reg1), HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

    // Konfiguracja CTRL_REG4 - ±2g
    uint8_t ctrl_reg4[] = {CTRL_REG4, 0x00};
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, ctrl_reg4, sizeof(ctrl_reg4), HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
}

uint8_t LIS3DH_ReadRegister(uint8_t reg){
    uint8_t txData = reg | 0x80;
    uint8_t rxData = 0;
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &txData, 1, 1000);
    HAL_SPI_Receive(&hspi1, &rxData, 1, 1000);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
    return rxData;
}

void LIS3DH_ReadXYZ(int16_t *x, int16_t *y, int16_t *z) {

    uint8_t tx_buffer[1] = {0x80 | 0x40 | LIS3DH_OUT_X_L}; // READ + AUTO_INCREMENT
    uint8_t rx_buffer[6] = {0};

    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, tx_buffer, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, rx_buffer, 6, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

    *x = (int16_t)((rx_buffer[1] << 8) | rx_buffer[0]);
    *y = (int16_t)((rx_buffer[3] << 8) | rx_buffer[2]);
    *z = (int16_t)((rx_buffer[5] << 8) | rx_buffer[4]);

}

float ConvertToG(int16_t raw){
    return (float)raw * 0.001f; // ±2g, 1 mg/digit
}

bool CheckForMotion(AccelerationData current, AccelerationData previous) {
    float diff_x = fabsf(current.x - previous.x);
    float diff_y = fabsf(current.y - previous.y);
    float diff_z = fabsf(current.z - previous.z);

    return (diff_x > MOTION_THRESHOLD) || (diff_y > MOTION_THRESHOLD) || (diff_z > MOTION_THRESHOLD);
}

void ProcessAccelerometer(void) {
    uint32_t currentTime = HAL_GetTick();

    if ((currentTime - lastSampleTime) >= SAMPLE_INTERVAL_MS) {
        AccelerationData currentAccel;
        int16_t raw[3];

        LIS3DH_ReadXYZ(&raw[0], &raw[1], &raw[2]);
        currentAccel.x = ConvertToG(raw[0]);
        currentAccel.y = ConvertToG(raw[1]);
        currentAccel.z = ConvertToG(raw[2]);

        if (CheckForMotion(currentAccel, prevAccel)) {
            lastMotionTime = currentTime;
            if (!isMoving) {
                isMoving = true;
                if (!gsmInitialized) {
                    gsmInitialized = initGSM();
                }
                sendCurrentStatus(true);
                safeDelay(1000);  // 3 sekundy przerwy
                sendTripBegin();
            }
        } else if (isMoving && ((currentTime - lastMotionTime) > INACTIVITY_TIMEOUT_MS)) {
            isMoving = false;
            sendCurrentStatus(false);
            safeDelay(1000);
            sendTripEnd();
            safeDelay(1000);
            sendLastParking();

            if (gsmInitialized) {
            	safeDelay(2000); // Poczekaj przed zamknięciem
                closeGSM();
                gsmInitialized = false;
            }
        }

        prevAccel = currentAccel;
        lastSampleTime = currentTime;
    }
}

// Implementacja funkcji dla GPS
float nmeaToDecimal(float coordinate){
    int degree = (int)(coordinate/100);
    float minutes = coordinate - degree * 100;
    return degree + minutes / 60;
}

void formatDateTime(float utcTime, int dateInt, char *buffer, size_t bufferSize) {
    int hours = (int)(utcTime / 10000);
    int minutes = (int)((utcTime - (hours * 10000)) / 100);
    int seconds = (int)(utcTime - (hours * 10000) - (minutes * 100));
    int day = dateInt / 10000;
    int month = (dateInt / 100) % 100;
    int year = 2000 + (dateInt % 100);

    snprintf(buffer, bufferSize, "%02d:%02d:%02d %02d.%02d.%04d",
             (hours + 2) % 24, minutes, seconds, day, month, year); // +2h dla czasu polskiego
}

void gpsParse(char *strParse) {
    float nmeaLat, nmeaLong, utcTime, altitude, speed, course;
    char ns, ew, status, unit;
    int dateInt = 0;

    if (strncmp(strParse, "$GPGGA", 6) == 0) {
        if (sscanf(strParse, "$GPGGA,%f,%f,%c,%f,%c,%*d,%*d,%*f,%f,%c",
                   &utcTime, &nmeaLat, &ns, &nmeaLong, &ew, &altitude, &unit) >= 6) {
            gpsData.decimalLat = nmeaToDecimal(nmeaLat);
            gpsData.decimalLong = nmeaToDecimal(nmeaLong);
            if (unit == 'M') {
                gpsData.altitude = altitude;
            }
            gpsData.valid = true;
        }
    } else if (strncmp(strParse, "$GPRMC", 6) == 0) {
        if (sscanf(strParse, "$GPRMC,%f,%c,%f,%c,%f,%c,%f,%f,%d",
                   &utcTime, &status, &nmeaLat, &ns, &nmeaLong, &ew, &speed, &course, &dateInt) >= 9) {
            if (status == 'A') { // Sprawdź czy GPS ma fix
                gpsData.decimalLat = nmeaToDecimal(nmeaLat);
                gpsData.decimalLong = nmeaToDecimal(nmeaLong);
                formatDateTime(utcTime, dateInt, gpsData.formattedTime, sizeof(gpsData.formattedTime));
                gpsData.speed = speed * 1.852f; // konwersja z węzłów na km/h
                gpsData.valid = true;
            }
        }
    }
}

int gpsValidate(char *nmea) {
    uint8_t calculatedCheck = 0;
    char *p = nmea + 1; // Pomijamy znak '$'

    while (*p && *p != '*' && (p - nmea) < 75) {
        calculatedCheck ^= *p++;
    }

    if (*p != '*' || *(p + 1) == 0 || *(p + 2) == 0) {
        return 0;
    }

    char check[3] = {*(p + 1), *(p + 2), '\0'};
    char calcStr[3];
    snprintf(calcStr, sizeof(calcStr), "%02X", calculatedCheck);

    return strcmp(check, calcStr) == 0;
}

bool initGSM(void) {
    const char* cmd[] = {
        "AT+SAPBR=0,1\r\n",
        "AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n",
        "AT+SAPBR=3,1,\"APN\",\"internet\"\r\n",
        "AT+SAPBR=1,1\r\n",
        "AT+HTTPTERM\r\n",
        "AT+HTTPINIT\r\n",
        "AT+HTTPPARA=\"CID\",1\r\n",
    };

    for (int i = 0; i < sizeof(cmd) / sizeof(cmd[0]); i++) {
        HAL_UART_Transmit(&huart3, (uint8_t*)cmd[i], strlen(cmd[i]), 200);
        if (i == 3) {
        	safeDelay(3000);  // 5s na nawiązanie połączenia GPRS
        } else {
        	safeDelay(2000);
        }
    }
    return true;
}

void closeGSM(void) {
    const char* cmd[] = {
        "AT+HTTPTERM\r\n",
        "AT+SAPBR=0,1\r\n"
    };

    for (int i = 0; i < sizeof(cmd) / sizeof(cmd[0]); i++) {
        HAL_UART_Transmit(&huart3, (uint8_t*)cmd[i], strlen(cmd[i]), 200);
        safeDelay(200);
    }
}

void sendToFirebase(const char* jsonData) {

    const char *cmd[] = {
        "AT+HTTPPARA=\"URL\",\"Your URL\"\r\n",
        "AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n"
    };

    for (int i = 0; i < 2; i++) {
        HAL_UART_Transmit(&huart3, (uint8_t*)cmd[i], strlen(cmd[i]), 1000);
        safeDelay(1000);
    }

    char dataCmd[50];
    int dataLength = strlen(jsonData);
    snprintf(dataCmd, sizeof(dataCmd), "AT+HTTPDATA=%d,10000\r\n", dataLength);
    HAL_UART_Transmit(&huart3, (uint8_t*)dataCmd, strlen(dataCmd), 1000);
    safeDelay(500);
    HAL_UART_Transmit(&huart3, (uint8_t*)jsonData, dataLength, 1000);
    safeDelay(500);
    HAL_UART_Transmit(&huart3, (uint8_t*)"AT+HTTPACTION=1\r\n", 17, 1000);
    safeDelay(4000);
}

void sendCurrentLocation(void) {
    if (!gsmInitialized || !gpsData.valid) return;

    char jsonData[256];
    snprintf(jsonData, sizeof(jsonData),
             "{\"type\":\"currentLocation\",\"lat\":%.6f,\"lng\":%.6f,\"time\":\"%s\",\"speed\":%.2f,\"altitude\":%.2f}",
             gpsData.decimalLat, gpsData.decimalLong, gpsData.formattedTime, gpsData.speed, gpsData.altitude);
    sendToFirebase(jsonData);
}

// 0 - wylacozny
// 1 - wlaczony
// 2 - kradziez (wysyla ESP)

void sendCurrentStatus(bool moving) {
    if (!gsmInitialized || !gpsData.valid) return;
    int i = moving ? 1 : 0;
    char jsonData[128];


    snprintf(jsonData, sizeof(jsonData),
    		 "{\"type\":\"currentStatus\",\"isMoving\":%s,\"status\":%d,\"time\":\"%s\"}",
			 moving ? "true" : "false", i, gpsData.formattedTime);
    sendToFirebase(jsonData);
}

void sendTripBegin(void) {
    if (!gsmInitialized || !gpsData.valid) return;

    char jsonData[200];
    snprintf(jsonData, sizeof(jsonData),
             "{\"type\":\"tripBegin\",\"lat\":%.6f,\"lng\":%.6f,\"time\":\"%s\",\"altitude\":%.2f}",
             gpsData.decimalLat, gpsData.decimalLong, gpsData.formattedTime, gpsData.altitude);
    sendToFirebase(jsonData);
}

void sendTripEnd(void) {
    if (!gsmInitialized || !gpsData.valid) return;

    char jsonData[200];
    snprintf(jsonData, sizeof(jsonData),
             "{\"type\":\"tripEnd\",\"lat\":%.6f,\"lng\":%.6f,\"time\":\"%s\",\"altitude\":%.2f}",
             gpsData.decimalLat, gpsData.decimalLong, gpsData.formattedTime, gpsData.altitude);
    sendToFirebase(jsonData);
}

void sendLastParking(void) {
    if (!gsmInitialized || !gpsData.valid) return;

    char jsonData[200];
    snprintf(jsonData, sizeof(jsonData),
             "{\"type\":\"lastParking\",\"lat\":%.6f,\"lng\":%.6f,\"time\":\"%s\",\"altitude\":%.2f}",
             gpsData.decimalLat, gpsData.decimalLong, gpsData.formattedTime, gpsData.altitude);
    sendToFirebase(jsonData);
}

// Kompletny, odporny na \r\n callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // Ignoruj '\r', akceptuj wszystko inne aż do '\n'
        if (rxData != '\r' && rxData != '\n' &&
            rxIndex < sizeof(rxBuffer) - 1)
        {
            rxBuffer[rxIndex++] = rxData;
        }
        else if (rxData == '\n') {
            // Zamknij string
            rxBuffer[rxIndex] = '\0';
            // Sprawdź checksumę i jeśli OK, parsuj
            if (gpsValidate((char*)rxBuffer)) {
                gpsParse((char*)rxBuffer);
            }
            rxIndex = 0;
            memset(rxBuffer, 0, sizeof(rxBuffer));
        }
        HAL_UART_Receive_IT(&huart1, &rxData, 1);
    }
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
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  LIS3DH_Init();

  // Pierwszy odczyt akcelerometru
  int16_t raw[3];
  LIS3DH_ReadXYZ(&raw[0], &raw[1], &raw[2]);
  prevAccel.x = ConvertToG(raw[0]);
  prevAccel.y = ConvertToG(raw[1]);
  prevAccel.z = ConvertToG(raw[2]);

  lastSampleTime = HAL_GetTick();
  lastMotionTime = HAL_GetTick();

  // Uruchom odbieranie GPS
  HAL_UART_Receive_IT(&huart1, &rxData, 1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
	  uint32_t currentTime = HAL_GetTick();

	  // Sprawdzaj akcelerometr
	  ProcessAccelerometer();

	  // Wysyłaj aktualne położenie co 15 sekund jeśli jest ruch
	  if (gsmInitialized && isMoving && ((currentTime - lastSendTime) >= SEND_INTERVAL_MS)){
		  sendCurrentLocation();
	      lastSendTime = currentTime;
	  }
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
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

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 9600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : CS_Pin */
  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
