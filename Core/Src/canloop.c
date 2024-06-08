#include "main.h"
#include <string.h>
#include <stdio.h>

extern CAN_HandleTypeDef hcan1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;

#define USART_TIMEOUT 1000
#define BNS_A1_INTERVAL 100

static uint8_t BNS_A1_DATA[3] = { 0x00, 0x00, 0x20 };
static uint8_t RxData[8];
static uint8_t TxData[8];
static uint32_t TxMailbox;
static CAN_RxHeaderTypeDef RxHeader;
static CAN_TxHeaderTypeDef TxHeader;

static char msgbuf[64];

static void sendSerialMsg(char* msg) {
  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), USART_TIMEOUT);
  HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), USART_TIMEOUT);
}

static void printError(char* iface, char* desc, int errno) {
	snprintf(msgbuf, sizeof(msgbuf), "%s: %d\r\n", desc, errno);
	sendSerialMsg(msgbuf);
}

// SLCAN/LAWICEL formatted frame. Can be used with SavvyCAN
static void printReceivedFrame() {
	int len = snprintf(msgbuf, sizeof(msgbuf), "t%03x%u", (unsigned int)RxHeader.StdId, (unsigned int)RxHeader.DLC);
	for (int i = 0; i < RxHeader.DLC; i++) {
		len += snprintf(msgbuf + len, sizeof(msgbuf) - len, "%02x", RxData[i]);
	}
	strcat(msgbuf, "\r");
	sendSerialMsg(msgbuf);
}

static void sendCANFrame(CAN_HandleTypeDef *can, uint32_t id, uint32_t len, uint8_t* data) {
  int rc = 0;
	TxHeader.DLC = len;
	TxHeader.StdId = id;
	TxHeader.RTR = CAN_RTR_DATA;
	TxHeader.IDE = CAN_ID_STD;
	memcpy(TxData, data, len);
	if ((rc = HAL_CAN_GetTxMailboxesFreeLevel(can)) > 0) {
		if ((rc = HAL_CAN_AddTxMessage(can, &TxHeader, TxData, &TxMailbox)) != HAL_OK) {
			HAL_CAN_ResetError(can);
			printError("", "Tx Error", rc);
		}
	}
}

#define COUNTER_INIT 4

void canloop(CAN_HandleTypeDef *can1) {
  int rc;
  int counter = COUNTER_INIT;
  int ledState = GPIO_PIN_SET;
  uint32_t lastFrameTime = HAL_GetTick(); // Track time of the last received frame
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, ledState); // Initially LED is on

  while (1) {
    if ((rc = HAL_CAN_GetRxFifoFillLevel(can1, CAN_RX_FIFO0)) > 0) {
      if ((rc = HAL_CAN_GetRxMessage(can1, CAN_RX_FIFO0, &RxHeader, RxData)) == HAL_OK) {
        printReceivedFrame();
        lastFrameTime = HAL_GetTick(); // Reset the timeout timer

        if (RxHeader.StdId == 0x0000) { // EZS_A1
          counter--;
          if (counter <= 0) {
            ledState = ledState == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, ledState);
            counter = COUNTER_INIT;
          }
          sendCANFrame(can1, 0x0009, sizeof(BNS_A1_DATA), BNS_A1_DATA);
        }
      }
    }

    // If 10 seconds have passed since the last received frame or since boot-up, go to sleep and turn off the LED
    if (HAL_GetTick() - lastFrameTime > 10000) {
      counter = COUNTER_INIT;
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET); // Turn off the LED
      HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    }
  }
}
