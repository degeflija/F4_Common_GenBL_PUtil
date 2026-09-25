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

  //
  //  25.09.2026 : Boot-Taster
  //
  //  Zwei Taster mit derselben Wirkung :
  //    B1   blauer Taster auf dem Discovery-Board, PA0, gedrueckt = 1
  //         ( Pull-down auf dem Board )
  //    PE7  externer Taster zwischen PE7 und Masse ( GND ), gedrueckt = 0
  //         ( interner Pull-up, keine weiteren Bauteile ). Frueher der
  //         Eingang Gear_Extended der Seitenwand-Sensorik, die nicht mehr
  //         gebaut wird ( task_Flaps_Switch_SideWall_Sensors.c ).
  //
  //  Wirkung :
  //    - im laufenden Betrieb ( P_Util und GenBL, task_SystemTimer ) :
  //      Bootloader_JumpToColdStart ( ), sobald der Taster
  //      c_Boot_Button_Debounce Abfragen lang gedrueckt ist.
  //    - beim Start des GenBL nach einem Programm-Reset
  //      ( task_Control_Loop.c ) : Taster gedrueckt --> im Bootloader
  //      bleiben, sonst in die Anwendung springen.
  //
  //  Ein Taster, der schon beim Start der Task gedrueckt ist ( oder klemmt ),
  //  loest keinen Kaltstart aus, bis er einmal losgelassen wurde. Sonst
  //  liefe der GenBL in eine Neustart-Schleife, solange man den Taster haelt.
  //
  //  PE7 wird hier eingestellt, NICHT in main.c - CubeMX wuerde eine
  //  Aenderung in MX_GPIO_Init() beim naechsten Generieren ueberschreiben,
  //  und das .ioc des GenBL kennt Port E gar nicht.
  //
  #define   Boot_Button_Ext_Port      GPIOE
  #define   Boot_Button_Ext_Pin       GPIO_PIN_7
  #define   c_Boot_Button_Debounce    10      //  Abfragen zu 5 ms --> 50 ms

  void      Boot_Button_Init          ( void );
  uint8_t   Boot_Button_Pressed       ( void );

#endif
//
// *****************************************************************************
// The End
// *****************************************************************************
