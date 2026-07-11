/**
 * #############################################################################
 * @file     LED_Blinker_Lib.c
 * @brief    New Vario
 * @author   Horst Rupp
 * @brief
 * #############################################################################
 */
//
// Includes
//
///

#include <LED_Blinker_Lib.h>

/*
  typedef struct
  {
    uint16_t  ThePin;
    uint16_t  On_Off;
    uint16_t  On_Interval;
    uint16_t  Off_Interval;
    uint32_t  Last_Change;
  } LED_Blinker_Def_t;
  */

  LED_Blinker_Def_t   F4_LED[4];

//  ****************************************************************************
//
void Initialize_Flash_Sequence ( uint8_t LED_Index, uint16_t On_Intval, uint16_t Off_Intval )
{
  ASSERT ( ( LED_Index >= 0 ) && ( LED_Index <= 3 ) );
  F4_LED[LED_Index].ThePin        = GPIO_PIN_12 << LED_Index;
  F4_LED[LED_Index].On_Off        = c_On;
  F4_LED[LED_Index].On_Interval   = On_Intval;
  F4_LED[LED_Index].Off_Interval  = Off_Intval;
  F4_LED[LED_Index].Last_Change   = 0;
}

//  ****************************************************************************
//
void Run_Flash_Sequence ( uint8_t LED_Index )
{
  ASSERT ( ( LED_Index >= 0 ) && ( LED_Index <= 3 ) );

  uint8_t ix = HAL_GPIO_ReadPin ( LED_Port, F4_LED[LED_Index].ThePin );
  if ( ix )
  {
    if ( g_SystemTime_ms - F4_LED[LED_Index].Last_Change > F4_LED[LED_Index].On_Interval )
    {
      HAL_GPIO_WritePin ( LED_Port, F4_LED[LED_Index].ThePin, GPIO_PIN_RESET );
      F4_LED[LED_Index].Last_Change = g_SystemTime_ms;
    }
  }
  else
  {
    if ( g_SystemTime_ms - F4_LED[LED_Index].Last_Change > F4_LED[LED_Index].Off_Interval )
    {
      HAL_GPIO_WritePin ( LED_Port, F4_LED[LED_Index].ThePin, GPIO_PIN_SET );
      F4_LED[LED_Index].Last_Change = g_SystemTime_ms;
    }
  }
}

//  ****************************************************************************
//
void Stop_Flash_Sequence ( uint8_t LED_Index )
{
  ASSERT ( ( LED_Index >= 0 ) && ( LED_Index <= 3 ) );
  F4_LED[LED_Index].On_Off = c_Off;
  HAL_GPIO_WritePin ( LED_Port, F4_LED[LED_Index].ThePin, GPIO_PIN_RESET );
}

//
// ============================================================================
//
void Blink_a_Pattern ( uint8_t pattern, uint16_t speed_in_ms, uint8_t do_die  )
{
  //uint8_t l_pattern = pattern & 0xf;    // only 4 bits relevant
  uint8_t ix;

  while ( 1 )
  {
    ix = HAL_GPIO_ReadPin ( B1_GPIO_Port, B1_Pin);
    if ( ! ix )
    {
      HAL_GPIO_WritePin ( LED_Port, LD3_Pin, GPIO_PIN_RESET );
      HAL_GPIO_WritePin ( LED_Port, LD4_Pin, GPIO_PIN_RESET );
      HAL_GPIO_WritePin ( LED_Port, LD5_Pin, GPIO_PIN_RESET );
      HAL_GPIO_WritePin ( LED_Port, LD6_Pin, GPIO_PIN_RESET );

      vTaskDelay ( speed_in_ms );

      for ( uint8_t i = 0; i < 4; i++ )
      {
        ix = 1 << i;
        if ( ( pattern & ix ) != 0 )  // if set
        {
          switch (i)
          {
          case 0 :
            HAL_GPIO_WritePin ( LED_Port, LD3_Pin, GPIO_PIN_SET );
            break;
          case 1 :
            HAL_GPIO_WritePin ( LED_Port, LD4_Pin, GPIO_PIN_SET );
            break;
          case 2 :
            HAL_GPIO_WritePin ( LED_Port, LD5_Pin, GPIO_PIN_SET );
            break;
          case 3 :
            HAL_GPIO_WritePin ( LED_Port, LD6_Pin, GPIO_PIN_SET );
            break;
          }
        }
      }

      vTaskDelay ( speed_in_ms );
    }
    else
      break;
  }

  HAL_GPIO_WritePin ( LED_Port, LD3_Pin, GPIO_PIN_RESET );
  HAL_GPIO_WritePin ( LED_Port, LD4_Pin, GPIO_PIN_RESET );
  HAL_GPIO_WritePin ( LED_Port, LD5_Pin, GPIO_PIN_RESET );
  HAL_GPIO_WritePin ( LED_Port, LD6_Pin, GPIO_PIN_RESET );

  if ( do_die )
    Bootloader_JumpToColdStart ( );
}


//
// ****************************************************************************
//  EOF
// ****************************************************************************
//

