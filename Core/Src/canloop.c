#include "main.h"
#include <string.h>
#include <stdio.h>

extern CAN_HandleTypeDef  hcan1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;

typedef struct {
  uint32_t id;
  uint8_t  len;
  uint8_t  data[8];
} CAN_FRAME;

typedef struct {
  CAN_FRAME recvFrame;
  CAN_FRAME responseFrame;
} CAN_RESPONSE_TABLE;

#define USART_TIMEOUT   1000
#define BNS_A1_INTERVAL 100
#define COUNTER_INIT    4     // Slows down the LED blink rate to approx 2Hz

#define EZS_A1_ID       0x000 // Triggers the send of BNS_A1
#define BNS_A1_ID       0x009
#define SAM_F_A2_ID     0x028 // Triggers the send of SG_APPL_BNS
#define NM_EZS_ID       0x400 // Triggers the send of NM_BNS
#define NM_BNS_ID       0x41f
#define SG_APPL_BNS_ID  0x7ef

static int nm_bns_state       = 0;
static int sg_appl_bns_toggle = 0x83;
static char msgBuf[128];

static const CAN_FRAME BNS_A1 = { BNS_A1_ID, 3, { 0x00, 0x00, 0x20 } };

static const CAN_FRAME NM_BNS_RESPONSE[5] = {
  { NM_BNS_ID, 8, { 0xfe, 0x1f, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff } }, // State 0
  { NM_BNS_ID, 8, { 0xfd, 0x01, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff } }, // State 1
  { NM_BNS_ID, 8, { 0xfe, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff } }, // State 2
  { NM_BNS_ID, 8, { 0xfd, 0x00, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff } }, // State 3
};

static CAN_FRAME SG_APPL_BNS = { SG_APPL_BNS_ID, 8, { 0xbb, 0xb0, 0x03, 0x00, 0xb0, 0xb0, 0x00, 0x09 } };

static void sendSerialMsg(char* msg) {
  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), USART_TIMEOUT);
  HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), USART_TIMEOUT);
}

static void printError(char* iface, char* desc, int errno) {
  snprintf(msgBuf, sizeof(msgBuf), "%s: %d\r\n", desc, errno);
  sendSerialMsg(msgBuf);
}

// SLCAN/LAWICEL formatted frame. Can be used with SavvyCAN
static void printReceivedFrame(CAN_FRAME* frame) {
  int len = snprintf(msgBuf, sizeof(msgBuf), "t%03x%u", (unsigned int)frame->id, (unsigned int)frame->len);
  for (int i = 0; i < frame->len; i++) {
  	len += snprintf(msgBuf + len, sizeof(msgBuf) - len, "%02x", frame->data[i]);
  }
  strcat(msgBuf, "\r");
  sendSerialMsg(msgBuf);
}

static void sendCANFrame(const CAN_FRAME* frame) {
  int                 rc;
  uint32_t            TxMailbox;
  CAN_TxHeaderTypeDef TxHeader;

  TxHeader.DLC    = frame->len;
  TxHeader.StdId  = frame->id;
  TxHeader.RTR    = CAN_RTR_DATA;
  TxHeader.IDE    = CAN_ID_STD;

  if ((rc = HAL_CAN_GetTxMailboxesFreeLevel(&hcan1)) > 0) {
      if ((rc = HAL_CAN_AddTxMessage(&hcan1, &TxHeader, frame->data, &TxMailbox)) != HAL_OK) {
          HAL_CAN_ResetError(&hcan1);
          printError("", "Tx Error", rc);
      }
  }
}

static int handleCANFrame(const CAN_FRAME* frame) {
  switch (frame->id) {
    case NM_EZS_ID: { // EZS Network Management Request
      if (frame->data[0] == 0xfe && frame->data[1] == 0x00 && nm_bns_state == 0) {
        sendCANFrame(&NM_BNS_RESPONSE[nm_bns_state]); nm_bns_state++; // Set state-1
        sendCANFrame(&NM_BNS_RESPONSE[nm_bns_state]); nm_bns_state++; // Set State-2
        break;
      } else if (frame->data[0] == 0xfe && frame->data[1] == 0x01 && nm_bns_state == 2) {
        sendCANFrame(&NM_BNS_RESPONSE[nm_bns_state]); nm_bns_state++; // Set State-3
      } else if (frame->data[0] == 0xfd && frame->data[1] == 0x01 && nm_bns_state == 3) {
        sendCANFrame(&NM_BNS_RESPONSE[nm_bns_state]);
      } else if (frame->data[0] == 0xfe && frame->data[1] == 0x00 && nm_bns_state == 3) {
        sendCANFrame(&NM_BNS_RESPONSE[0]);
      } else {
        sendCANFrame(&NM_BNS_RESPONSE[nm_bns_state]);
      }
      break;
    }
    case EZS_A1_ID: { // Triggers the send of BNS_A1
      sendCANFrame(&BNS_A1);
      break;
    }
    case SAM_F_A2_ID: { // Triggers the send of SG_APPL_BNS
      SG_APPL_BNS.data[2] = sg_appl_bns_toggle;
      sg_appl_bns_toggle ^= 0x80; // Toggle between 0x03 and 0x83 every other frame
      sendCANFrame(&SG_APPL_BNS);
      break;
    }
    default:
      return 0;
  }
  return 1;
}

void canloop() {
  int                   rc;
  int                   counter = COUNTER_INIT;
  int                   ledState = GPIO_PIN_SET;
  CAN_FRAME             frame;
  CAN_RxHeaderTypeDef   canHeader;
  uint32_t              lastFrameTime = HAL_GetTick(); // Track time of the last received frame

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, ledState); // Initially LED is on

  while (1) {
    if ((rc = HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0)) > 0) {
      if ((rc = HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &canHeader, frame.data)) == HAL_OK) {
        frame.id = canHeader.StdId;
        frame.len = canHeader.DLC;
        printReceivedFrame(&frame);
        lastFrameTime = HAL_GetTick(); // Reset the timeout timer
        if (handleCANFrame(&frame) != 0) {
          // If we are processing frames we recognize, blink the LED
          if (--counter <= 0) {
            ledState = ledState == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, ledState);
            counter = COUNTER_INIT;
          }
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
