/*
 * can.h
 *
 *  Created on: 18.11.2022
 *      Author: Maximilian Betz
 */

#ifndef CAN_H_
#define CAN_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include "FreeRTOS.h"

#define CAN_RX_QUEUE_LEN 100
#define CAN_RX_TIMEOUT_MS 100

extern CAN_HandleTypeDef hcan1;
//extern CAN_HandleTypeDef hcan2;

//! basic CAN packet type
typedef struct
{
  uint16_t id;  //!< identifier
  uint16_t dlc; //!< data length code
  union
  {
    uint8_t  data_b[8];   //!< data seen as 8 times uint8_t
    int8_t   data_sb[8];  //!< data seen as 8 times int8_t
    uint16_t data_h[4];   //!< data seen as 4 times uint16_t
    int16_t  data_sh[4];  //!< data seen as 4 times int16_t
    uint32_t data_w[2];   //!< data seen as 2 times uint32_t
    int32_t  data_sw[2];  //!< data seen as 2 times int32_t
    float    data_f[2];   //!< data seen as 2 times 32-bit floats
    uint64_t data_l;      //!< data seen as 64-bit integer
  };
} CAN_Packet_t;

HAL_StatusTypeDef CAN_Init(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef CAN_Send(CAN_HandleTypeDef *hcan, CAN_Packet_t *p );
HAL_StatusTypeDef CAN_Receive(CAN_HandleTypeDef *hcan, CAN_Packet_t *p );


#ifdef __cplusplus
}
#endif

#endif
