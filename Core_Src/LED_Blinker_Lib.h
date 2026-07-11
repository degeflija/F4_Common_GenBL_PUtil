/**
 * #############################################################################
 * @file     LED_Blinker_Lib.h
 * @brief    New Vario
 * @author   Horst Rupp
 * @brief
 *            This library of two routines
 *
 *
 */

//
// *****************************************************************************
//
#ifndef __LED_Blinker_Lib_H
  #define __LED_Blinker_Lib_H

  #include  "Generic_Includes.h"
  #include  "Portable_Utils.h"
  #include  "Generic_Common.h"
  #include  "Flash_F4xx_Lib.h"

  //
  //  **************************************************************************
  //
  typedef struct
  {
    uint16_t  ThePin;
    uint16_t  On_Off;
    uint16_t  On_Interval;
    uint16_t  Off_Interval;
    uint32_t  Last_Change;
  } LED_Blinker_Def_t;


  #ifdef  __cplusplus
      extern "C" {
  #endif

  //  **************************************************************************
  //
  void Initialize_Flash_Sequence (  uint8_t LED_Index,
                                    uint16_t On_Intval,
                                    uint16_t Off_Intval
                                 );

  void Run_Flash_Sequence ( uint8_t LED_Index );

  void Stop_Flash_Sequence ( uint8_t LED_Index );

  void Blink_a_Pattern ( uint8_t pattern, uint16_t speed_in_ms, uint8_t do_die );


#endif // __LED_Blinker_Lib_H
//
//  ****************************************************************************
//  EOF
