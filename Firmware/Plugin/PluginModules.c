#include "PluginModules.h"
#include "Api.h"
#include "System/Util/Callback.h"
#include "ModuleConfig.h"
#include "ModuleIncludes.h"
#include "Priot.h"
#include "System/Util/Assert.h"
#include "Vars.h"

static int _needShutdown = 0;

static int _PluginModules_shutdownModules( int majorID, int minorID,
    void* serve, void* client )
{
    config_UNUSED( majorID );
    config_UNUSED( minorID );
    config_UNUSED( serve );
    config_UNUSED( client );

    if ( !_needShutdown ) {
        Assert_assert( _needShutdown == 1 );
    } else {
#include "ModuleShutdown.h"
        _needShutdown = 0;
    }

    return ErrorCode_SUCCESS; /* callback rc ignored */
}

void PluginModules_initModules( void )
{
    static int once = 0;

    netsnmp_access_interface_init();
#include "ModuleInits.h"

    _needShutdown = 1;

    if ( once == 0 ) {
        int rc;
        once = 1;
        rc = Callback_register( CallbackMajor_LIBRARY, CallbackMinor_SHUTDOWN,
            _PluginModules_shutdownModules, NULL );

        if ( rc != PRIOT_ERR_NOERROR )
            Logger_log( LOGGER_PRIORITY_ERR,
                "error registering for SHUTDOWN callback "
                "for mib modules\n" );
    }
}
