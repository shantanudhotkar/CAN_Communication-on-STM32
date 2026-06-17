#ifndef CIA402_H
#define CIA402_H

#include <stdint.h>
#include <stdbool.h>

#include "301/CO_driver.h"
#include "CANopen.h"
#include "OD.h"

/* -----------------------------------------------------------
 * CiA402 Object Dictionary References
 * ----------------------------------------------------------- */

#define OD_Controlword     OD_RAM.x6040_controlword
#define OD_Statusword      OD_RAM.x6041_statusword
#define OD_ErrorCode       OD_RAM.x603F_errorCode



/* -----------------------------------------------------------
 * CiA402 States
 * ----------------------------------------------------------- */
typedef enum
{
    CIA402_STATE_NOT_READY_TO_SWITCH_ON = 0,
    CIA402_STATE_SWITCH_ON_DISABLED,
    CIA402_STATE_READY_TO_SWITCH_ON,
    CIA402_STATE_SWITCHED_ON,
    CIA402_STATE_OPERATION_ENABLED,
    CIA402_STATE_QUICK_STOP_ACTIVE,
    CIA402_STATE_FAULT_REACTION_ACTIVE,
    CIA402_STATE_FAULT
} CIA402_State_t;

/* -----------------------------------------------------------
 * API
 * ----------------------------------------------------------- */
void CIA402_Init(void);
void CIA402_Process(void);

void CIA402_SetFault(uint16_t error_code);

#endif
