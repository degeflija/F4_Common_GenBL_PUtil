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
void SystemTimer ( void * nix)
{

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
    //  Blue Button
    //
    uint8_t l_button_pattern =  HAL_GPIO_ReadPin ( B1_GPIO_Port, B1_Pin );
    if ( l_button_pattern )
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
