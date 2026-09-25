/**
  * ############################################################################
  * @file     task_SystemTimer.c
  * @brief    New Vario
  * @author   Horst Rupp
  * @brief    This task reads bit patterns from GPIO and rotary encoders.
  *           This task loops at 5 msecs.
  *           This task runs in both environments
  *           - the AD57 VARIO and
  *           - the AD57 Bootloader
  *
  * ############################################################################
  */
//
// Includes
//
#include  "task_SystemTimer.h"

#define   c_LED_System_Timer_On_IntVal    1500
#define   c_LED_System_Timer_Off_IntVal   50

//
// *****************************************************************************
//
//  25.09.2026 : Boot-Taster ( Beschreibung siehe task_SystemTimer.h )
//
void Boot_Button_Init ( void )
{
  GPIO_InitTypeDef l_init = { 0 };

  __HAL_RCC_GPIOE_CLK_ENABLE ( );

  l_init.Pin    = Boot_Button_Ext_Pin;
  l_init.Mode   = GPIO_MODE_INPUT;
  l_init.Pull   = GPIO_PULLUP;          //  offen = 1, gedrueckt ( gegen GND ) = 0
  l_init.Speed  = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init ( Boot_Button_Ext_Port, &l_init );
}

//
//  true, wenn B1 ( PA0, gedrueckt = 1 ) ODER der externe Taster
//  ( PE7, gedrueckt = 0 ) gedrueckt ist. Ohne Entprellung.
//
uint8_t Boot_Button_Pressed ( void )
{
  return  (
            ( HAL_GPIO_ReadPin ( B1_GPIO_Port, B1_Pin ) == GPIO_PIN_SET )
          ||
            ( HAL_GPIO_ReadPin ( Boot_Button_Ext_Port, Boot_Button_Ext_Pin ) == GPIO_PIN_RESET )
          );
}

//
// *****************************************************************************
//
void SystemTimer ( void * nix)
{
  uint8_t   l_boot_armed    = false;   //  erst nach einmal Loslassen
  uint8_t   l_boot_count    = 0;

  Boot_Button_Init ( );                //  25.09.2026

  #ifdef LED_System_Timer
  Initialize_Flash_Sequence ( LED_System_Timer,
                              c_LED_System_Timer_On_IntVal,
                              c_LED_System_Timer_Off_IntVal
                            );
  #endif

  //
  // ---------------------------------------------------------------------------
  //
  // the works
  //
  while( 1 )
  {
    vTaskDelay( 5 );                  // loop

    g_Time_in_Ticks   = xTaskGetTickCount( );   // Laut Klaus in msec  !!??!!##
    g_SystemTime_ms   = g_Time_in_Ticks;
    g_SystemTime_sec  = g_Time_in_Ticks / 1000;

    #ifdef LED_System_Timer
    Run_Flash_Sequence ( LED_System_Timer );
    #endif

    // ========================================================================
    // ========================================================================
    //
    //  Boot-Taster : blauer Taster B1 oder externer Taster an PE7
    //  ( 25.09.2026, siehe task_SystemTimer.h )
    //
    if ( ! Boot_Button_Pressed ( ) )
    {
      l_boot_armed  = true;
      l_boot_count  = 0;
    }
    else if ( l_boot_armed && ( l_boot_count < c_Boot_Button_Debounce ) )
    {
      l_boot_count++;
    }

    if ( l_boot_armed && ( l_boot_count >= c_Boot_Button_Debounce ) )
    {
      //F4_Clear_Signature_Sector_7 ();
      Bootloader_JumpToColdStart ();      // Good Luck !!
      //Bootloader_JumpToApplication ();    // Good Luck !!
    }
  }
}

//
// *****************************************************************************
// The End
// *****************************************************************************
