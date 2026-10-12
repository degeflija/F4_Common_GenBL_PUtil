#include "can.h"
#include "main.h"
#include "queue.h"
#include "task.h"
#include "Flash_F4xx_Lib.h"
//

extern  QueueHandle_t CAN1_rxQueue_Id;
extern  QueueHandle_t CAN2_rxQueue_Id;

// *****************************************************************************
//
//  10.10.2026 : Stand VOR der CAN-Selbstueberwachung ( CAN_Supervise,
//  CAN_Reinit, CAN_Hard_Reset ) wiederhergestellt - auf Horsts Anweisung.
//
//  Einzige Ausnahme : die Laengenbegrenzung DLC <= 8 im Empfang bleibt.
//  AD57_FE sendet 0x311 mit DLC 10 ; ohne Begrenzung schreibt die
//  Kopierschleife zwei Bytes hinter p.data_b[] auf den Stack der
//  Interrupt-Routine.
//

// *****************************************************************************
//
void CAN_Error_Handler(CAN_HandleTypeDef *hcan)
{
  uint32_t error = HAL_CAN_GetError(hcan);

  // Check for Bus-Off (BOF) error
  if (error & HAL_CAN_ERROR_BOF)
  {
    HAL_CAN_Stop(hcan);
    HAL_CAN_Start(hcan);
    HAL_CAN_ActivateNotification(hcan,
        CAN_IT_RX_FIFO0_MSG_PENDING |
        CAN_IT_TX_MAILBOX_EMPTY |
        CAN_IT_BUSOFF |
        CAN_IT_ERROR);
  }
}

// *****************************************************************************
//
HAL_StatusTypeDef CAN_Init(CAN_HandleTypeDef *hcan)
{
  if(hcan->Instance == CAN1)
  {
    CAN1_rxQueue_Id = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(CAN_Packet_t));
    if (CAN1_rxQueue_Id == 0)
    {
      Error_Handler();
    }
  }

  /*Receive all messages on fifo 0, do not filter yet*/
  CAN_FilterTypeDef sFilterConfig;
  sFilterConfig.FilterIdHigh = 0xFFFF;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0xFFFF;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = 0;
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_16BIT;
  sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;
  sFilterConfig.SlaveStartFilterBank = 0;

  if  (
        ( HAL_CAN_ConfigFilter ( hcan, &sFilterConfig ) != HAL_OK )
      ||
        ( HAL_CAN_ActivateNotification ( hcan,
                                          CAN_IT_TX_MAILBOX_EMPTY |
                                          CAN_IT_RX_FIFO0_MSG_PENDING |
                                          CAN_IT_ERROR |
                                          CAN_IT_BUSOFF ) != HAL_OK )
      ||
        ( HAL_CAN_Start ( hcan ) != HAL_OK )
      )
  {
    /* Starting up with no CAN counterpart on the bus yet is a normal,
     * expected condition, not a fault -- do NOT halt here (Horst,
     * 2026-07-14). Retry via a full restart instead; a short delay gives
     * a counterpart a chance to power up / join the bus. */
    if ( ! UNDER_DEBUGGER() )
    {
      HAL_Delay ( 300 );
#ifdef BUILD_F4_AUDIO
      //
      //  12.10.2026 : Im Audio ECHTER Reset statt Software-Sprung.
      //  GenBL hat die Reset-Flags beim ersten Anlauf schon gelesen und
      //  geloescht. Nach einem Software-Sprung sieht GenBL deshalb
      //  RESET_CAUSE_UNKNOWN, springt nicht ins Audio und wartet endlos
      //  auf ein Neu-Flashen ( Audio stumm, GenBL-HeartBeat 0x200 laeuft ).
      //  NVIC_SystemReset() setzt RCC_FLAG_SFTRST -> GenBL springt ins Audio.
      //  GenBL selbst bleibt beim Software-Sprung ( Weg fuer 0x330 ).
      //
      NVIC_SystemReset ();
#else
      Bootloader_JumpToColdStart ();
#endif
    }
  }

  return HAL_OK;
}

// *****************************************************************************
//
HAL_StatusTypeDef CAN_Send(CAN_HandleTypeDef *hcan, CAN_Packet_t *p)
{
  CAN_TxHeaderTypeDef canHeader;
  canHeader.StdId = (uint32_t)p->id;
  canHeader.ExtId = 0;
  canHeader.IDE = CAN_ID_STD;
  canHeader.RTR = CAN_RTR_DATA;
  canHeader.DLC = p->dlc;
  canHeader.TransmitGlobalTime = DISABLE;

  uint32_t txMailbox = 0;
  return HAL_CAN_AddTxMessage(hcan, &canHeader, p->data_b, &txMailbox);
}

HAL_StatusTypeDef CAN_Receive(CAN_HandleTypeDef *hcan, CAN_Packet_t *p)
{
  if(hcan->Instance == CAN1)
    {
      if (xQueueReceive(CAN1_rxQueue_Id, p, CAN_RX_TIMEOUT_MS) == pdTRUE)
        {
          return HAL_OK;
        }
      else
        {
          return HAL_TIMEOUT;
        }
    }

  if(hcan->Instance == CAN2)
    {
      if (xQueueReceive(CAN2_rxQueue_Id, p, CAN_RX_TIMEOUT_MS) == pdTRUE)
        {
          return HAL_OK;
        }
      else
        {
          return HAL_TIMEOUT;
        }
    }

  return HAL_ERROR;
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{

}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{

}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{

}

void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan)
{

}

void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan)
{

}

void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan)
{

}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  BaseType_t xHigherPriorityTaskWokenByPost = pdFALSE;
  CAN_RxHeaderTypeDef rxfifo0_header;
  uint8_t rxfifo0_rxData[8];

  if ( HAL_CAN_GetRxMessage(hcan, 0, &rxfifo0_header, rxfifo0_rxData) != HAL_OK )
    return;

  CAN_Packet_t p;
  p.id = (uint16_t)rxfifo0_header.StdId;
  p.dlc = rxfifo0_header.DLC;
  if ( p.dlc > 8 )      //  siehe Kopf : AD57_FE sendet 0x311 mit DLC 10
    p.dlc = 8;

  for (int i = 0; i < p.dlc; i++)
  {
    p.data_b[i] = rxfifo0_rxData[i];
  }

  if(hcan->Instance == CAN1)
  {
    (void) xQueueSendFromISR(CAN1_rxQueue_Id, &p, &xHigherPriorityTaskWokenByPost);
  }

  if(hcan->Instance == CAN2)
  {
    (void) xQueueSendFromISR(CAN2_rxQueue_Id, &p, &xHigherPriorityTaskWokenByPost);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWokenByPost);
}

void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan)
{
  CAN_Error_Handler(hcan);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_Error_Handler(hcan);
}

void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan)
{
  CAN_Error_Handler(hcan);
}

void HAL_CAN_SleepCallback(CAN_HandleTypeDef *hcan)
{
  CAN_Error_Handler(hcan);
}

void HAL_CAN_WakeUpFromRxMsgCallback(CAN_HandleTypeDef *hcan)
{
  CAN_Error_Handler(hcan);
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
  //  A Bus-Off (or other bus error) is expected and harmless whenever no CAN
  //  partner is present/powered -- the system must keep running, not break.
  CAN_Error_Handler(hcan);
}
