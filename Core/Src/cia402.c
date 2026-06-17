#include "cia402.h"

/* ============================================================
 * INTERNAL STATE
 * ============================================================ */

static CIA402_State_t cia402_state;

/* ============================================================
 * CONTROLWORD BIT DEFINITIONS (6040h)
 * IEC 61800-7-201 Table 27
 * ============================================================ */

#define CW_SWITCH_ON              (1U << 0)
#define CW_ENABLE_VOLTAGE         (1U << 1)
#define CW_QUICK_STOP             (1U << 2)
#define CW_ENABLE_OPERATION       (1U << 3)
#define CW_FAULT_RESET            (1U << 7)

/* ============================================================
 * STATUSWORD BIT DEFINITIONS (6041h)
 * IEC 61800-7-201 Figure 7
 * ============================================================ */

#define SW_READY_TO_SWITCH_ON     (1U << 0)
#define SW_SWITCHED_ON            (1U << 1)
#define SW_OPERATION_ENABLED      (1U << 2)
#define SW_FAULT                  (1U << 3)
#define SW_VOLTAGE_ENABLED        (1U << 4)
#define SW_QUICK_STOP             (1U << 5)
#define SW_SWITCH_ON_DISABLED     (1U << 6)
#define SW_WARNING                (1U << 7)
#define SW_REMOTE                 (1U << 9)
#define SW_TARGET_REACHED         (1U << 10)

/* ============================================================
 * HARDWARE CALLBACKS
 * USER MUST IMPLEMENT THESE
 * ============================================================ */

/*
 * Enable gate drivers / power stage.
 * Apply high voltage to inverter.
 */
static void HW_EnablePowerStage(void)
{
    // TODO:
    // Enable gate driver IC
    // Enable DC bus relay/contactor
    // Enable PWM timer outputs
}

/*
 * Disable inverter power stage safely.
 */
static void HW_DisablePowerStage(void)
{
    // TODO:
    // Disable PWM outputs
    // Disable gate driver
    // Open contactor if required
}

/*
 * Enable motor control loops.
 * Current loop / FOC / velocity loop etc.
 */
static void HW_EnableDriveFunction(void)
{
    // TODO:
    // Start current controllers
    // Enable FOC
    // Enable trajectory execution
}

/*
 * Disable motion generation while
 * keeping power stage alive.
 */
static void HW_DisableDriveFunction(void)
{
    // TODO:
    // Stop motion commands
    // Disable torque production
    // Hold current state safely
}

/*
 * Execute quick stop behaviour.
 */
static void HW_QuickStop(void)
{
    // TODO:
    // Decelerate motor safely
    // Controlled stop using configured ramp
    // DO NOT immediately kill power unless required
}

/*
 * Apply fault reaction.
 */
static void HW_FaultReaction(void)
{
    // TODO:
    // Emergency shutdown logic
    // Disable PWM immediately
    // Engage brake if required
    // Put hardware in safe state
}

/*
 * Clear fault condition.
 */
static bool HW_IsFaultCleared(void)
{
    // TODO:
    // Return true only when:
    // - Overcurrent cleared
    // - Undervoltage cleared
    // - Temperature normal
    // etc.

    return true;
}

/* ============================================================
 * STATUSWORD GENERATOR
 * ============================================================ */

static void CIA402_UpdateStatusword(void)
{
    uint16_t sw = 0;

    sw |= SW_REMOTE;

    switch(cia402_state)
    {
        case CIA402_STATE_NOT_READY_TO_SWITCH_ON:
            break;

        case CIA402_STATE_SWITCH_ON_DISABLED:
            sw |= SW_SWITCH_ON_DISABLED;
            break;

        case CIA402_STATE_READY_TO_SWITCH_ON:
            sw |= SW_READY_TO_SWITCH_ON;
            sw |= SW_QUICK_STOP;
            break;

        case CIA402_STATE_SWITCHED_ON:
            sw |= SW_READY_TO_SWITCH_ON;
            sw |= SW_SWITCHED_ON;
            sw |= SW_QUICK_STOP;
            sw |= SW_VOLTAGE_ENABLED;
            break;

        case CIA402_STATE_OPERATION_ENABLED:
            sw |= SW_READY_TO_SWITCH_ON;
            sw |= SW_SWITCHED_ON;
            sw |= SW_OPERATION_ENABLED;
            sw |= SW_QUICK_STOP;
            sw |= SW_VOLTAGE_ENABLED;
            break;

        case CIA402_STATE_QUICK_STOP_ACTIVE:
            sw |= SW_READY_TO_SWITCH_ON;
            sw |= SW_SWITCHED_ON;
            sw |= SW_OPERATION_ENABLED;
            sw |= SW_VOLTAGE_ENABLED;
            break;

        case CIA402_STATE_FAULT_REACTION_ACTIVE:
            sw |= SW_FAULT;
            break;

        case CIA402_STATE_FAULT:
            sw |= SW_FAULT;
            break;
    }

    OD_Statusword = sw;
}

/* ============================================================
 * INITIALIZATION
 * ============================================================ */

void CIA402_Init(void)
{
    cia402_state = CIA402_STATE_SWITCH_ON_DISABLED;

    CIA402_UpdateStatusword();
}

/* ============================================================
 * FAULT ENTRY
 * ============================================================ */

void CIA402_SetFault(uint16_t error_code)
{
    OD_ErrorCode = error_code;

    cia402_state = CIA402_STATE_FAULT_REACTION_ACTIVE;

    HW_FaultReaction();

    CIA402_UpdateStatusword();

    /*
     * IEC 61800-7-201:
     * Fault reaction active shall automatically
     * transition to Fault state after reaction completes.
     */
    cia402_state = CIA402_STATE_FAULT;

    CIA402_UpdateStatusword();
}

/* ============================================================
 * MAIN FSM PROCESS
 * Call cyclically (1 kHz typical)
 * ============================================================ */

void CIA402_Process(void)
{
    uint16_t cw = OD_Controlword;

    switch(cia402_state)
    {
        /* ----------------------------------------------------
         * SWITCH ON DISABLED
         * ---------------------------------------------------- */
        case CIA402_STATE_SWITCH_ON_DISABLED:
        {
            /*
             * Transition 2:
             * Shutdown command
             *
             * CW = 0x0006
             */
            if((cw & 0x0087) == 0x0006)
            {
                cia402_state = CIA402_STATE_READY_TO_SWITCH_ON;
            }

            break;
        }

        /* ----------------------------------------------------
         * READY TO SWITCH ON
         * ---------------------------------------------------- */
        case CIA402_STATE_READY_TO_SWITCH_ON:
        {
            /*
             * Transition 3:
             * Switch on
             *
             * CW = 0x0007
             */
            if((cw & 0x008F) == 0x0007)
            {
                /*
                 * High voltage may now be applied.
                 */
                HW_EnablePowerStage();

                cia402_state = CIA402_STATE_SWITCHED_ON;
            }

            /*
             * Transition 7:
             * Disable voltage
             */
            else if((cw & 0x0082) == 0x0000)
            {
                HW_DisablePowerStage();

                cia402_state = CIA402_STATE_SWITCH_ON_DISABLED;
            }

            break;
        }

        /* ----------------------------------------------------
         * SWITCHED ON
         * ---------------------------------------------------- */
        case CIA402_STATE_SWITCHED_ON:
        {
            /*
             * Transition 4:
             * Enable operation
             *
             * CW = 0x000F
             */
            if((cw & 0x008F) == 0x000F)
            {
                /*
                 * Motor control loops become active here.
                 */
                HW_EnableDriveFunction();

                cia402_state = CIA402_STATE_OPERATION_ENABLED;
            }

            /*
             * Transition 6:
             * Shutdown
             */
            else if((cw & 0x0087) == 0x0006)
            {
                HW_DisablePowerStage();

                cia402_state = CIA402_STATE_READY_TO_SWITCH_ON;
            }

            /*
             * Disable voltage
             */
            else if((cw & 0x0082) == 0x0000)
            {
                HW_DisablePowerStage();

                cia402_state = CIA402_STATE_SWITCH_ON_DISABLED;
            }

            break;
        }

        /* ----------------------------------------------------
         * OPERATION ENABLED
         * ---------------------------------------------------- */
        case CIA402_STATE_OPERATION_ENABLED:
        {
            /*
             * NORMAL MOTOR OPERATION OCCURS HERE
             *
             * Position mode
             * Velocity mode
             * Torque mode
             * CSP/CSV/CST etc.
             */

            /*
             * Transition 5:
             * Disable operation
             */
            if((cw & 0x008F) == 0x0007)
            {
                HW_DisableDriveFunction();

                cia402_state = CIA402_STATE_SWITCHED_ON;
            }

            /*
             * Transition 11:
             * Quick stop
             */
            else if((cw & CW_QUICK_STOP) == 0)
            {
                HW_QuickStop();

                cia402_state = CIA402_STATE_QUICK_STOP_ACTIVE;
            }

            /*
             * Disable voltage
             */
            else if((cw & 0x0082) == 0x0000)
            {
                HW_DisableDriveFunction();
                HW_DisablePowerStage();

                cia402_state = CIA402_STATE_SWITCH_ON_DISABLED;
            }

            break;
        }

        /* ----------------------------------------------------
         * QUICK STOP ACTIVE
         * ---------------------------------------------------- */
        case CIA402_STATE_QUICK_STOP_ACTIVE:
        {
            /*
             * Transition 16:
             * Enable operation after quick stop
             */
            if((cw & 0x008F) == 0x000F)
            {
                HW_EnableDriveFunction();

                cia402_state = CIA402_STATE_OPERATION_ENABLED;
            }

            break;
        }

        /* ----------------------------------------------------
         * FAULT
         * ---------------------------------------------------- */
        case CIA402_STATE_FAULT:
        {
            /*
             * Transition 15:
             * Fault reset
             */
            if(cw & CW_FAULT_RESET)
            {
                if(HW_IsFaultCleared())
                {
                    /*
                     * Clear internal protection latches,
                     * reset driver IC faults,
                     * clear overcurrent flags, etc.
                     */

                    cia402_state = CIA402_STATE_SWITCH_ON_DISABLED;
                }
            }

            break;
        }

        default:
            break;
    }

    CIA402_UpdateStatusword();
}
