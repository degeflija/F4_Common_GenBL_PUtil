/**
 * @file    task_SystemTimer.h
 * @brief   New Vario
 * @author  Horst Rupp
 */
//#include  "System_Configuration.h"
//#include  "FreeRTOS_wrapper.h"
//#include  "Generic_Application_Includes.h"
//
// *****************************************************************************
//
#ifndef  __task_SystemTimer
  #define  __task_SystemTimer

  #include  "Generic_Includes.h"
  #include  "Generic_Const.h"
  #include  "LED_Blinker_Lib.h"
  #include  "Flash_F4xx_Lib.h"

  //
  void  SystemTimer              ( void * nix);
  void  create_task_SystemTimer  ( void );

#endif
//
// *****************************************************************************
// The End
// *****************************************************************************
