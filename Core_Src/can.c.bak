#include "can.h"
#include "main.h"
#include "queue.h"
#include "Flash_F4xx_Lib.h"

extern  QueueHandle_t CAN1_rxQueue_Id;
extern  QueueHandle_t CAN2_rxQueue_Id;


void CAN_Error_Handler(CAN_HandleTypeDef *hcan)
{
    uint32_t error = HAL_CAN_GetError(hcan);

    // Check for Bus-Off (BOF) error
    if (error & HAL_CAN_ERROR_BOF)
    {
        // Stop CAN safely
        HAL_CAN_Stop(hcan);

        // Clear error codes (this also clears the BOF flag internally)
        //__HAL_CAN_CLEAR_FLAG(hcan, CAN_FLAG_LEC);

        // Reset transmission state
        // No need to deinit/init fully
        HAL_CAN_Start(hcan);

        // Optionally restart notifications
        HAL_CAN_ActivateNotification(hcan,
            CAN_IT_RX_FIFO0_MSG_PENDING |
            CAN_IT_TX_MAILBOX_EMPTY |
            CAN_IT_BUSOFF |
            CAN_IT_ERROR);

        // Log or handle error counters if needed
    }
}


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

  if (HAL_CAN_ConfigFilter(hcan, &sFilterConfig)!= HAL_OK)
    {
      /* Same reasoning as MX_CAN1_Init()'s HAL_CAN_Init() retry (Horst,
       * 2026-07-14): starting up with no CAN counterpart on the bus yet is
       * a normal, expected condition, not a fault -- do NOT halt here.
       * Retry via a full self-reset instead, giving a counterpart a
       * chance to power up / join the bus between attempts. */
      if ( ! UNDER_DEBUGGER() )
      {
        HAL_Delay ( 300 );
        Bootloader_JumpToColdStart();
      }
    }

  if (HAL_CAN_ActivateNotification(hcan, CAN_IT_TX_MAILBOX_EMPTY|CAN_IT_RX_FIFO0_MSG_PENDING|CAN_IT_ERROR|CAN_IT_BUSOFF)!= HAL_OK)
    {
      /* Same reasoning as MX_CAN1_Init()'s HAL_CAN_Init() retry (Horst,
       * 2026-07-14): starting up with no CAN counterpart on the bus yet is
       * a normal, expected condition, not a fault -- do NOT halt here.
       * Retry via a full self-reset instead, giving a counterpart a
       * chance to power up / join the bus between attempts. */
      if ( ! UNDER_DEBUGGER() )
      {
        HAL_Delay ( 300 );
        Bootloader_JumpToColdStart();
      }
    }

  if (HAL_CAN_Start(hcan) != HAL_OK)
    {
      /* Same reasoning as MX_CAN1_Init()'s HAL_CAN_Init() retry (Horst,
       * 2026-07-14): starting up with no CAN counterpart on the bus yet is
       * a normal, expected condition, not a fault -- do NOT halt here.
       * Retry via a full self-reset instead, giving a counterpart a
       * chance to power up / join the bus between attempts. */
      if ( ! UNDER_DEBUGGER() )
      {
        HAL_Delay ( 300 );
        Bootloader_JumpToColdStart();
      }
    }

  return HAL_OK;
}

HAL_StatusTypeDef CAN_Send(CAN_HandleTypeDef *hcan, CAN_Packet_t *p)
{
  CAN_TxHeaderTypeDef canHeader;
  canHeader.StdId = (uint32_t)p->id;
  canHeader.ExtId = 0;
  canHeader.IDE = CAN_ID_STD;
  canHeader.RTR = CAN_RTR_DATA;
  canHeader.DLC = p->dlc;

  //return HAL_CAN_AddTxMessage(hcan, &canHeader, p->data_b, CAN_TX_MAILBOX1 );
  uint32_t txMailbox = 0;
  HAL_StatusTypeDef st = HAL_CAN_AddTxMessage(hcan, &canHeader, p->data_b, &txMailbox);
  /* txMailbox now contains the mailbox used */
  return st;
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

  HAL_CAN_GetRxMessage(hcan, 0, &rxfifo0_header, rxfifo0_rxData);

  CAN_Packet_t p;
  p.id = (uint16_t)rxfifo0_header.StdId;
  p.dlc = rxfifo0_header.DLC;

  for (int i = 0; i < p.dlc; i++)
  {
    p.data_b[i] = rxfifo0_rxData[i];
  }

  if(hcan->Instance == CAN1)
  {
    if( xQueueSendFromISR(CAN1_rxQueue_Id, &p, &xHigherPriorityTaskWokenByPost) != pdTRUE)
    {
      /* Route to the graceful recovery path instead of the fatal
       * Error_Handler() -- CAN_Error_Handler() only acts if the underlying
       * cause is an actual Bus-Off condition; a plain queue-full is left
       * alone (packet dropped) rather than crashing the system. */
      CAN_Error_Handler(hcan);
    }
  }

  if(hcan->Instance == CAN2)
  {
    if( xQueueSendFromISR(CAN2_rxQueue_Id, &p, &xHigherPriorityTaskWokenByPost) != pdTRUE)
    {
      CAN_Error_Handler(hcan);
    }
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
  //  Route to the graceful recovery path instead of the fatal Error_Handler().
  //  A Bus-Off (or other bus error) is expected and harmless whenever no CAN
  //  partner is present/powered -- the system must keep running, not break.
  CAN_Error_Handler(hcan);
}
