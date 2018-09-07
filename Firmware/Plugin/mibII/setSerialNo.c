/**  
 *  This file implements the snmpSetSerialNo TestAndIncr counter
 */

#include "setSerialNo.h"
#include "AgentCallbacks.h"
#include "AgentReadConfig.h"
#include "System/Util/Callback.h"
#include "System/Util/Trace.h"
#include "Watcher.h"
/*
 * A watched spinlock can be fully implemented by the spinlock helper,
 *  but we still need a suitable variable to hold the value.
 */
static int setserialno;

/*
     * TestAndIncr values should persist across agent restarts,
     * so we need config handling routines to load and save the
     * current value (incrementing this whenever it's loaded).
     */
static void setserial_parse_config( const char* token, char* cptr )
{
    setserialno = atoi( cptr );
    setserialno++;
    DEBUG_MSGTL( ( "snmpSetSerialNo",
        "Re-setting SnmpSetSerialNo to %d\n", setserialno ) );
}
static int
setserial_store_config( int a, int b, void* c, void* d )
{
    char line[ UTILITIES_MAX_BUFFER_SMALL ];
    snprintf( line, UTILITIES_MAX_BUFFER_SMALL, "setserialno %d", setserialno );
    AgentReadConfig_priotdStoreConfig( line );
    return 0;
}

void init_setSerialNo( void )
{
    oid set_serial_oid[] = { 1, 3, 6, 1, 6, 3, 1, 1, 6, 1 };

    /*
     * If we can't retain the TestAndIncr value across an agent restart,
     *  then it should be initialised to a pseudo-random value.  So set it
     *  as such, before registering the config handlers to override this.
     */

    setserialno = random();

    DEBUG_MSGTL( ( "snmpSetSerialNo",
        "Initalizing SnmpSetSerialNo to %d\n", setserialno ) );
    AgentReadConfig_priotdRegisterConfigHandler( "setserialno", setserial_parse_config,
        NULL, "integer" );
    Callback_register( CallbackMajor_LIBRARY, CallbackMinor_STORE_DATA,
        setserial_store_config, NULL );

    /*
     * Register 'setserialno' as a watched spinlock object
     */
    Watcher_registerWatchedSpinlock(
        AgentHandler_createHandlerRegistration( "snmpSetSerialNo", NULL,
            set_serial_oid,
            asnOID_LENGTH( set_serial_oid ),
            HANDLER_CAN_RWRITE ),
        &setserialno );

    DEBUG_MSGTL( ( "scalar_int", "Done initalizing example scalar int\n" ) );
}
