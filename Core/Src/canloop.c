#include "main.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

extern CAN_HandleTypeDef  hcan1;

typedef struct {
  uint32_t id;
  uint8_t  len;
  uint8_t  data[8];
} CAN_FRAME;

typedef struct {
  CAN_FRAME recvFrame;
  CAN_FRAME responseFrame;
} CAN_RESPONSE_TABLE;

#define DEBUG_MODE            1
#define ENABLE_SLCAN          1

#define SLEEP_TIMEOUT         3000
#define USART_TIMEOUT         1000
#define BNS_A1_INTERVAL       100
#define NM_BNS_INTERVAL       175
#define SG_APPL_BNS_INTERVAL  100
#define LED_TOGGLE_TIMEOUT    1000

#define EZS_A1_ID           0x000 // Triggers the send of BNS_A1
#define BNS_A1_ID           0x009
#define SAM_F_A2_ID         0x028 // Triggers the send of SG_APPL_BNS
#define NM_EZS_ID           0x400 // Triggers the send of NM_BNS
#define NM_BNS_ID           0x41f
#define SG_APPL_BNS_ID      0x7ef

#define KEYLESS_GO_ACTIVE   0x80 // Keyless Go terminal control active
#define STARTER_ACTIVE      0x40 // Terminal 50
#define KEY_INSERTED        0x10 // Key is inserted
#define KEY_ACCESSORY_ON    0x01 // Terminal 15c on
#define KEY_RUN_ON          0x02 // Terminal 15R on
#define MAIN_IGN_ON         0x04 // Terminal 15 on
#define COURTESY_POWER      0x08 // Terminal 15x

// BNS trouble codes
// Data byte 0
#define BN_Warn1            0x01 // OFFSET 7 LEN 1 - Vehicle electrical system warning: Emergency operation (Prio1 and Prio2 consumers off, starter battery supports)
#define BN_Warn2            0x02 // OFFSET 6 LEN 1 - On-board power supply warning: emergency operation (Prio1 or Prio2 consumers off)
#define BN_Warn3            0x04 // OFFSET 5 LEN 1 - Vehicle electrical system warning: Relay K2 defective
#define BN_Warn4            0x08 // OFFSET 4 LEN 1 - Vehicle electrical system warning: Starter battery fuse defective
#define BN_Warn5            0x10 // OFFSET 3 LEN 1 - Vehicle electrical system warning: "Key is inserted" not recognized
#define BN_NTLF_P2          0x20 // OFFSET 2 LEN 1 - On-board network emergency operation: Prio2 consumer off
#define BN_NTLF_P1          0x40 // OFFSET 1 LEN 1 - On-board network emergency operation: Prio1 consumer off
#define BN_NTLF             0x80 // OFFSET 0 LEN 1 - On-board network emergency operation: Prio1 and Prio2 consumers off, second battery supports

// Data byte 1
#define BN_SOHS_VB  0x10 // OFFSET 11 LEN 1 - Vehicle electrical system warning: State of Health supply battery

// Data byte 2
#define BNS_AKT     0x20 // OFFSET 18 LEN 1 - On-board power supply control unit active

#define BATTERY_LOW_THRESHOLD 108 // 10.8V

#define TRUE   1
#define FALSE  0
#define AWAKE  0
#define ASLEEP 1

static char msgBuf[128];

static CAN_FRAME BNS_A1      = { BNS_A1_ID, 3, { 0x00, 0x00, 0x20 } };
static CAN_FRAME NM_BNS      = { NM_BNS_ID, 8, { 0xfe, 0x1f, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff } };
static CAN_FRAME NM_BNS_NEXT = { NM_BNS_ID, 8, { 0xfd, 0x01, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff } }; // Delay updates by one frame
static CAN_FRAME SG_APPL_BNS = { SG_APPL_BNS_ID, 8, { 0xbb, 0xb0, 0x03, 0x00, 0xb0, 0xb0, 0x00, 0x09 } };

static int      ledState            = GPIO_PIN_SET;
static int      sleepState          = AWAKE; // 0: Awake, 1: Asleep
static uint8_t  ignitionState       = 0;
static uint8_t  batteryVoltage      = 0; // Value / 10 == battery voltage
static uint32_t sleep_timeout       = 0;
static uint32_t bns_a1_timeout      = 0;
static uint32_t nm_bns_timeout      = 0;
static uint32_t sg_appl_bns_timeout = 0;
static uint32_t led_toggle_timeout  = 0;

static void sendSerialMsg (char* msg) {
  HAL_UART_Transmit(huart, (uint8_t*)msg, strlen(msg), USART_TIMEOUT);
}

static void printError (char* iface, char* desc, int errno) {
  snprintf(msgBuf, sizeof(msgBuf), "%s: %d\r\n", desc, errno);
  sendSerialMsg(msgBuf);
}

/**
 * Sends a formatted debug message over serial.
 * This function behaves similarly to printf, formatting the given string and arguments
 * and then sending the resulting string using sendSerialMsg.
 *
 * @param format The format string.
 * @param ... The values to format according to the format string.
 */
void sendDebugMsg(const char *format, ...) {
#if DEBUG_MODE == 0
  return;
#else
  char buffer[256]; // Adjust the size according to your needs
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  sendSerialMsg(buffer);
#endif
}

// SLCAN/LAWICEL formatted frame. Can be used with SavvyCAN
static void printReceivedFrame (CAN_FRAME* frame) {
#if DEBUG_MODE == 0
#if ENABLE_SLCAN == 1
  int len = snprintf(msgBuf, sizeof(msgBuf), "t%03x%u", (unsigned int)frame->id, (unsigned int)frame->len);
  for (int i = 0; i < frame->len; i++) {
  	len += snprintf(msgBuf + len, sizeof(msgBuf) - len, "%02x", frame->data[i]);
  }
  strcat(msgBuf, "\r");
  sendSerialMsg(msgBuf);
#endif
#endif
}

static void sendCANFrame (const CAN_FRAME* frame) {
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

static int handleCANFrame (const CAN_FRAME* recvFrame) {
  switch (recvFrame->id) {
    case EZS_A1_ID: { // EZS A1
      if ((ignitionState & KEYLESS_GO_ACTIVE) != (recvFrame->data[0] & KEYLESS_GO_ACTIVE)) {
        sendDebugMsg("KEYLESS_GO: %s\r\n", recvFrame->data[0] & KEYLESS_GO_ACTIVE ? "ACTIVE" : "IDLE");
      }
      if ((ignitionState & KEY_ACCESSORY_ON) != (recvFrame->data[0] & KEY_ACCESSORY_ON)) {
        sendDebugMsg("KEY_ACCESSORY: %s\r\n", recvFrame->data[0] & KEY_ACCESSORY_ON ? "ON" : "OFF");
      }
      if ((ignitionState & KEY_RUN_ON) != (recvFrame->data[0] & KEY_RUN_ON)) {
        sendDebugMsg("KEY_RUN: %s\r\n", recvFrame->data[0] & KEY_RUN_ON ? "ON" : "OFF");
      }
      if ((ignitionState & MAIN_IGN_ON) != (recvFrame->data[0] & MAIN_IGN_ON)) {
        sendDebugMsg("MAIN_IGN: %s\r\n", recvFrame->data[0] & MAIN_IGN_ON ? "ON" : "OFF");
      }
      if ((ignitionState & COURTESY_POWER) != (recvFrame->data[0] & COURTESY_POWER)) {
        sendDebugMsg("COURTESY_POWER: %s\r\n", recvFrame->data[0] & COURTESY_POWER ? "ON" : "OFF");
      }
      if ((ignitionState & KEY_INSERTED) != (recvFrame->data[0] & KEY_INSERTED)) {
        sendDebugMsg("KEY: %s\r\n", recvFrame->data[0] & KEY_INSERTED ? "INSERTED" : "ABSENT");
      }
      if (batteryVoltage != recvFrame->data[2]) {
        sendDebugMsg("Battery voltage: %d\r\n", recvFrame->data[2]);
      }
      ignitionState = recvFrame->data[0];
      batteryVoltage = recvFrame->data[2];
      // If less than 10.8, set BN_Warn2 and BN_SOHS_VB
      BNS_A1.data[1] = batteryVoltage < BATTERY_LOW_THRESHOLD ? BN_Warn2 | BN_SOHS_VB : 0x00;
      break;
    }
    case NM_EZS_ID: { // EZS Network Management Request
      // No clue what these mean. It's this way based on observation.
      NM_BNS_NEXT.data[0] = recvFrame->data[0];
      if (recvFrame->data[1] == 0x00) {
        NM_BNS_NEXT.data[1] = 0x1f;
      } else if (recvFrame->data[1] == 0x01) {
        NM_BNS_NEXT.data[1] = 0x00;
        if (recvFrame->data[0] == 0xfd) {
          NM_BNS_NEXT.data[2] = 0xbf;
        } else {
          NM_BNS_NEXT.data[2] = 0x3f;
        }
      }
      break;
    }
    default:
      return 0;
  }
  return 1;
}

static void send_NM_BNS (uint32_t currTime) {
  if (nm_bns_timeout == 0) {
    nm_bns_timeout = currTime + 60; // Delay 60 msec before sending the first frame
  } else if (currTime > nm_bns_timeout) {
    nm_bns_timeout = currTime + NM_BNS_INTERVAL;
    sendCANFrame(&NM_BNS);
    NM_BNS.data[0] = NM_BNS_NEXT.data[0];
    NM_BNS.data[1] = NM_BNS_NEXT.data[1];
  }
}

static void send_BNS_A1 (uint32_t currTime) {
  if (bns_a1_timeout == 0) {
    bns_a1_timeout = currTime + 140; // Delay 140 msec before sending the first frame
  } else if (currTime > bns_a1_timeout) {
    bns_a1_timeout = currTime + BNS_A1_INTERVAL;
    sendCANFrame(&BNS_A1);
  }
}

static void send_SG_APPL_BNS (uint32_t currTime) {
  if (sg_appl_bns_timeout == 0) {
    sg_appl_bns_timeout = currTime + 220; // Delay 220 msec before sending the first frame
  } else if (currTime > sg_appl_bns_timeout) {
    sg_appl_bns_timeout = currTime + SG_APPL_BNS_INTERVAL;
    sendCANFrame(&SG_APPL_BNS);
    SG_APPL_BNS.data[2] ^= 0x80; // Toggle between 0x03 and 0x83 every other frame
  }
}

static void sendPeriodicFrames (uint32_t currTime) {
  // Send BNS frames when the accessory power (Terminal 15c) or Keyless-Go are on
  // Or, send one initial burst of frames when coming out of sleep mode
  if ((ignitionState & (KEY_ACCESSORY_ON | KEYLESS_GO_ACTIVE)) || (sleep_timeout == 0)) {
    send_NM_BNS(currTime);
    send_BNS_A1(currTime);
    send_SG_APPL_BNS(currTime);
  }
}

static void toggleLED (uint32_t currTime) {
  if ((currTime > led_toggle_timeout) && (led_pin != GPIO_PIN_NONE)) {
    ledState = ledState == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(GPIOB, led_pin, ledState);
    led_toggle_timeout = currTime + LED_TOGGLE_TIMEOUT;
  }
}

// canSleep = 1: Enter sleep mode
//   If SLEEP_TIMEOUT milliseconds have elapsed since the last received frame
//   or since boot-up, reset state, turn off the LED and go to sleep.
// canSleep = 0: Exit sleep mode
//   Reset the sleep timeout.
static void powerManagement (uint32_t currTime, int canSleep) {
  if (canSleep == FALSE) {
    if (sleepState == ASLEEP) {
      sleepState = AWAKE;
      ExitSleepMode();
    }
    sleep_timeout = currTime + SLEEP_TIMEOUT;
  } else if (currTime > sleep_timeout) {
    if (sleepState == AWAKE) {
      sleepState = ASLEEP;
      sleep_timeout       = 0;
      bns_a1_timeout      = 0;
      nm_bns_timeout      = 0;
      sg_appl_bns_timeout = 0;
      led_toggle_timeout  = 0;

      NM_BNS.data[1] = 0x1f; NM_BNS.data[2] = 0x3f; // Reset NM_BNS state
      SG_APPL_BNS.data[2] = 0x83;                   // Reset SG_APPL_BNS state

      EnterSleepMode();
    }
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
  }
}

static int recvFrame (CAN_FRAME* frame) {
  int rc;
  CAN_RxHeaderTypeDef canHeader;

  if ((rc = HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0)) > 0) {
    if ((rc = HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &canHeader, frame->data)) == HAL_OK) {
      frame->id = canHeader.StdId;
      frame->len = canHeader.DLC;
      return 1;
    }
  }
  return 0;
}

void canloop () {
  uint32_t  currTime;
  CAN_FRAME frame;

  sleep_timeout = HAL_GetTick() + SLEEP_TIMEOUT;  // Sleep 3 seconds after boot-up if there's no CAN activity
  GetBoardVariant();
  sendDebugMsg("CAN loop started\r\n");

  while (1) {
    currTime = HAL_GetTick();
    if (recvFrame(&frame)) {
      sendPeriodicFrames(currTime);
      toggleLED(currTime); // Blink the LED
      handleCANFrame(&frame);
      printReceivedFrame(&frame); // Send frame to Serial for SavvyCAN, etc
      powerManagement(currTime, AWAKE);
    }

    powerManagement(currTime, ASLEEP);
  }
}
