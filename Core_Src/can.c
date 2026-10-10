#include "can.h"
#include "main.h"
#include "queue.h"
#include "task.h"
#include "Flash_F4xx_Lib.h"
#include "my_assert.h"          //  TRAP_IF_DEBUGGER  10.10.2026
//

extern  QueueHandle_t CAN1_rxQueue_Id;
extern  QueueHandle_t CAN2_rxQueue_Id;

// *****************************************************************************
//
//  10.10.2026 : CAN-Selbstueberwachung ( F4_GenBL und F4_Audio )
//
//  Horsts Vorgabe : das Audio-Board muss selbst erkennen, dass sein CAN
//  klemmt, und es selbst neu aufsetzen.
//
//  Bisher gab es drei Fallen :
//
//  1. CAN_Error_Handler() rief bei Bus-Off HAL_CAN_Stop() / HAL_CAN_Start()
//     direkt in der Interrupt-Routine. Scheitert HAL_CAN_Start() dort ( es
//     wartet auf 11 rezessive Bits ), bleibt hcan->State auf
//     HAL_CAN_STATE_ERROR stehen. Ab dann weist HAL_CAN_AddTxMessage() jeden
//     Rahmen ab - das Board EMPFAENGT weiter ( die Hardware ist wieder am
//     Bus ), SENDET aber nie wieder. Weil es die HeartBeats des AD57 weiter
//     hoert, schlug auch die 30-s-Selbstheilung im Audio nie zu.
//
//  2. Scheiterte CAN_Init() beim Start, folgte ein Software-Sprung
//     Bootloader_JumpToColdStart(). Der setzt keine Reset-Ursache, GenBLs
//     Control_Loop() sieht reset_cause == 0, haelt das fuer eine
//     Flash-Anforderung und wartet endlos auf einen CAN-MDP-Reflash, den
//     niemand schickt. Das Audio kommt dann nicht hoch.
//
//  3. ( F4_Audio main.c ) Ein Fehler in HAL_CAN_Init() endete im
//     Error_Handler() mit gesperrten Interrupts - Stillstand bis zum
//     Aus- und Einschalten.
//
//  Neu :
//
//  - Die Interrupt-Routine setzt bei Bus-Off nur noch eine Anforderung.
//  - CAN_Supervise() laeuft alle 10 ms im Task CAN_Bus_Sender und setzt
//    CAN1 komplett neu auf ( CAN_Reinit() ), wenn
//      a) die Interrupt-Routine Bus-Off gemeldet hat,
//      b) hcan->State nicht mehr HAL_CAN_STATE_LISTENING ist,
//      c) seit c_CAN_Tx_Stuck_ms kein Rahmen mehr in eine Mailbox passte,
//      d) nach bereits empfangenem Verkehr seit c_CAN_Rx_Silence_ms nichts
//         mehr ankam.
//    Zwischen zwei Neuaufsetzungen liegen mindestens c_CAN_Reinit_Pause_ms.
//  - Scheitert die Neuaufsetzung c_CAN_Reinit_Max_Fail Mal hintereinander,
//    folgt ein echter Hardware-Reset ( NVIC_SystemReset ). Der setzt
//    RCC_FLAG_SFTRST, GenBL springt damit wie nach dem Einschalten in die
//    gueltige Anwendung.
//  - Scheitert CAN_Init() beim Start, ebenfalls NVIC_SystemReset() statt
//    Software-Sprung ( Falle 2 ).
//
//  Allein am Bus ( kein AD57 ) sind c ) und d ) normal : dann wird alle
//  paar Sekunden neu aufgesetzt, was nichts stoert. Ein Hardware-Reset
//  folgt daraus nicht, weil die Neuaufsetzung selbst gelingt.
//
//  Unter dem Debugger gibt es keinen Hardware-Reset, nur TRAP_IF_DEBUGGER.
//
//  10.10.2026 mittags, nach Horsts Gegentest : Das Neuaufsetzen im laufenden
//  Betrieb ( CAN_Reinit ) brachte das Audio zum Schweigen, sobald es allein
//  am Bus hing - dort laufen die Sende-Mailboxen mangels Quittung immer
//  voll, Fall c) schlug alle 2 s zu. Neue, engere Regel :
//
//    - Es wird nur noch eingegriffen, wenn der Bus LEBT : innerhalb der
//      letzten c_CAN_Rx_Alive_ms wurde etwas empfangen. Allein am Bus
//      ( oder nach dem Abschalten des AD57 ) geschieht gar nichts ; dafuer
//      bleibt die 30-s-Selbstheilung in task_CAN_Bus_Receiver.c zustaendig.
//    - Eingriff, wenn bei lebendem Bus seit c_CAN_Fault_ms
//        a) Bus-Off gemeldet ist oder
//        b) hcan->State nicht HAL_CAN_STATE_LISTENING ist oder
//        c) kein Rahmen mehr in eine Mailbox passt.
//      Das ist genau Falle 1 : empfangen ja, senden nein.
//    - Eingriff = echter Hardware-Reset ( CAN_Hard_Reset ), KEIN
//      Neuaufsetzen im laufenden Betrieb mehr. GenBL springt danach wie nach
//      dem Einschalten in die Anwendung.
//    - Fall d) ( Empfangs-Stille ) entfaellt.
//
//  CAN_Reinit() bleibt als Funktion stehen, wird aber nicht mehr gerufen.
//
//  10.10.2026 mittags : Schalter fuer den Gegentest.
//    1 = Selbstueberwachung wie oben beschrieben
//    0 = Verhalten wie vor dem 10.10.2026 : CAN_Supervise() tut nichts,
//        Bus-Off wird wie frueher in der ISR mit Stop/Start behandelt,
//        ein Fehler in CAN_Init() endet wie frueher im Software-Sprung.
//    Die Laengenbegrenzung DLC <= 8 im Empfang bleibt in beiden Faellen
//    ( AD57_FE sendet 0x311 mit DLC 10 ).
//
#define c_CAN_SUPERVISE_ON        1

#define c_CAN_Rx_Alive_ms         1000    //  Bus lebt : zuletzt empfangen vor hoechstens ...
#define c_CAN_Fault_ms            2000    //  Fehler muss so lange ununterbrochen anstehen

static CAN_InitTypeDef      m_Init_Copy;            //  Bit-Timing aus MX_CAN1_Init()
static bool                 m_Init_Saved          = false;
static volatile bool        m_Recover_Request     = false;  //  aus der ISR
static volatile TickType_t  m_Last_Rx_Tick        = 0;      //  0 = noch nie etwas empfangen
static TickType_t           m_Tx_Stuck_Since      = 0;      //  0 = Senden klappt
static TickType_t           m_Fault_Since         = 0;      //  0 = kein Fehler anstehend

// *****************************************************************************
//
//  Echter Hardware-Reset, siehe Kopf. Kehrt nie zurueck ( ausser unter dem
//  Debugger ).
//
static void CAN_Hard_Reset ( void )
{
#if ( c_CAN_SUPERVISE_ON == 0 )
  if ( ! UNDER_DEBUGGER() )
  {
    HAL_Delay ( 300 );
    Bootloader_JumpToColdStart ();      //  alter Weg
  }
  return;
#endif

  if ( UNDER_DEBUGGER() )
  {
    TRAP_IF_DEBUGGER();
    return;
  }

  HAL_Delay ( 300 );
  NVIC_SystemReset ();
}

// *****************************************************************************
//
//  Filter, Interrupts, Start. Gemeinsam fuer CAN_Init() und CAN_Reinit().
//  Rueckgabe true = CAN1 laeuft ( HAL_CAN_STATE_LISTENING ).
//
static bool CAN_Configure_And_Start ( CAN_HandleTypeDef *hcan )
{
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

  if ( HAL_CAN_ConfigFilter ( hcan, &sFilterConfig ) != HAL_OK )
    return false;

  if  ( HAL_CAN_ActivateNotification ( hcan,
                                        CAN_IT_TX_MAILBOX_EMPTY |
                                        CAN_IT_RX_FIFO0_MSG_PENDING |
                                        CAN_IT_ERROR |
                                        CAN_IT_BUSOFF ) != HAL_OK )
    return false;

  if ( HAL_CAN_Start ( hcan ) != HAL_OK )
    return false;

  return true;
}

// *****************************************************************************
//
//  Nur noch eine Anforderung setzen, siehe Kopf ( Falle 1 ). Aufgerufen aus
//  den HAL-Callbacks, also im Interrupt.
//
void CAN_Error_Handler(CAN_HandleTypeDef *hcan)
{
  uint32_t error = HAL_CAN_GetError(hcan);

  // Check for Bus-Off (BOF) error
  if (error & HAL_CAN_ERROR_BOF)
  {
#if ( c_CAN_SUPERVISE_ON == 0 )
    //  alter Weg : direkt in der ISR anhalten und neu starten
    HAL_CAN_Stop(hcan);
    HAL_CAN_Start(hcan);
    HAL_CAN_ActivateNotification(hcan,
        CAN_IT_RX_FIFO0_MSG_PENDING |
        CAN_IT_TX_MAILBOX_EMPTY |
        CAN_IT_BUSOFF |
        CAN_IT_ERROR);
#else
    m_Recover_Request = true;
#endif
  }

  //  HAL sammelt die Fehlerbits in hcan->ErrorCode nur auf ( |= ). Ohne
  //  Ruecksetzen wuerde jeder spaetere Aufruf das alte Bus-Off wieder sehen.
  //
  (void) HAL_CAN_ResetError ( hcan );
}

// *****************************************************************************
//
HAL_StatusTypeDef CAN_Init(CAN_HandleTypeDef *hcan)
{
  if(hcan->Instance == CAN1)
  {
    if ( CAN1_rxQueue_Id == 0 )           //  nur einmal anlegen
      CAN1_rxQueue_Id = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(CAN_Packet_t));
    if (CAN1_rxQueue_Id == 0)
    {
      Error_Handler();
    }
  }

  //  Bit-Timing usw. aus MX_CAN1_Init() merken, CAN_Reinit() braucht es.
  //
  m_Init_Copy  = hcan->Init;
  m_Init_Saved = true;

  if ( ! CAN_Configure_And_Start ( hcan ) )
  {
    /* Starting up with no CAN counterpart on the bus yet is a normal,
     * expected condition, not a fault -- do NOT halt here (Horst,
     * 2026-07-14). 10.10.2026 : real hardware reset instead of the former
     * software jump Bootloader_JumpToColdStart(), see header ( trap 2 ). */
    CAN_Hard_Reset ();
  }

  m_Tx_Stuck_Since   = 0;
  m_Fault_Since      = 0;

  return HAL_OK;
}

// *****************************************************************************
//
//  CAN1 komplett neu aufsetzen : Senden abbrechen, anhalten, DeInit,
//  Peripherie-Reset, Init mit dem gemerkten Bit-Timing, Filter, Start.
//  Rueckgabe true = CAN1 laeuft wieder.
//
bool CAN_Reinit ( CAN_HandleTypeDef *hcan )
{
  if ( ! m_Init_Saved )
    return false;

  (void) HAL_CAN_AbortTxRequest ( hcan, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2 );
  (void) HAL_CAN_Stop   ( hcan );
  (void) HAL_CAN_DeInit ( hcan );       //  sperrt auch die CAN-Interrupts ( MspDeInit )

  //  Wie in Bootloader_JumpToColdStart() : erst der RCC-Reset bringt die
  //  Register ( Fehlerzaehler, Bus-Off, Filter ) sicher auf Anfang.
  //
  __HAL_RCC_CAN1_FORCE_RESET   ( );
  __HAL_RCC_CAN1_RELEASE_RESET ( );

  hcan->Init  = m_Init_Copy;
  hcan->State = HAL_CAN_STATE_RESET;    //  damit HAL_CAN_Init() MspInit ruft

  if ( HAL_CAN_Init ( hcan ) != HAL_OK )
    return false;

  return CAN_Configure_And_Start ( hcan );
}

// *****************************************************************************
//
//  Alle 10 ms aus dem Task CAN_Bus_Sender aufrufen. Siehe Kopf.
//
void CAN_Supervise ( CAN_HandleTypeDef *hcan )
{
#if ( c_CAN_SUPERVISE_ON == 0 )
  (void) hcan;
  return;
#endif

  TickType_t  l_now       = xTaskGetTickCount ( );
  TickType_t  l_last_rx   = m_Last_Rx_Tick;
  bool        l_bus_alive = false;
  bool        l_fault     = false;

  //  Bus lebt : in den letzten c_CAN_Rx_Alive_ms etwas empfangen.
  //
  if  (
        ( l_last_rx != 0 )
      &&
        ( ( l_now - l_last_rx ) <= pdMS_TO_TICKS ( c_CAN_Rx_Alive_ms ) )
      )
    l_bus_alive = true;

  if ( m_Recover_Request )
    l_fault = true;                                                   //  a)

  if ( hcan->State != HAL_CAN_STATE_LISTENING )
    l_fault = true;                                                   //  b)

  if ( m_Tx_Stuck_Since != 0 )
    l_fault = true;                                                   //  c)

  //  Allein am Bus oder kein Fehler : nichts tun, Fehlerzeit verwerfen.
  //
  if ( ( ! l_bus_alive ) || ( ! l_fault ) )
  {
    m_Fault_Since     = 0;
    m_Recover_Request = false;
    return;
  }

  if ( m_Fault_Since == 0 )
  {
    m_Fault_Since = ( l_now == 0 ) ? 1 : l_now;
    return;
  }

  if ( ( l_now - m_Fault_Since ) >= pdMS_TO_TICKS ( c_CAN_Fault_ms ) )
    CAN_Hard_Reset ();
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
  HAL_StatusTypeDef st = HAL_CAN_AddTxMessage(hcan, &canHeader, p->data_b, &txMailbox);

  //  10.10.2026 : Buchfuehrung fuer CAN_Supervise(), Fall c).
  //  Erfolg heisst : der Rahmen passte in eine Mailbox. Klemmt der Bus,
  //  laufen alle drei Mailboxen voll, und jeder weitere Versuch scheitert.
  //
  if ( st == HAL_OK )
  {
    m_Tx_Stuck_Since = 0;
  }
  else
  {
    if ( m_Tx_Stuck_Since == 0 )
    {
      m_Tx_Stuck_Since = xTaskGetTickCount ( );
      if ( m_Tx_Stuck_Since == 0 )
        m_Tx_Stuck_Since = 1;               //  0 ist die Kennung "klappt"
    }
  }

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

  if ( HAL_CAN_GetRxMessage(hcan, 0, &rxfifo0_header, rxfifo0_rxData) != HAL_OK )
    return;

  //  10.10.2026 : fuer CAN_Supervise(), Fall d). 0 bleibt "noch nie".
  //
  {
    TickType_t l_tick = xTaskGetTickCountFromISR ( );
    m_Last_Rx_Tick = ( l_tick == 0 ) ? 1 : l_tick;
  }

  CAN_Packet_t p;
  p.id = (uint16_t)rxfifo0_header.StdId;
  p.dlc = rxfifo0_header.DLC;
  if ( p.dlc > 8 )
    p.dlc = 8;

  for (int i = 0; i < p.dlc; i++)
  {
    p.data_b[i] = rxfifo0_rxData[i];
  }

  if(hcan->Instance == CAN1)
  {
    //  Queue voll : der Rahmen faellt weg. Das ist kein CAN-Fehler, das
    //  Neuaufsetzen wuerde nichts bessern.
    //
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
  //  10.10.2026 : only flags the request, CAN_Supervise() does the work.
  CAN_Error_Handler(hcan);
}
