/*
 * This file was generated by mib2c and is intended for use as
 * a mib module for the ucd-snmp snmpd agent. 
 */

/*
 * This should always be included first before anything else 
 */
#include "snmpNotifyFilterProfileTable.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "Impl.h"
#include "ReadConfig.h"
#include "System/Util/Trace.h"
#include "TextualConvention.h"
#include "header_complex.h"

/*
 * snmpNotifyFilterProfileTable_variables_oid:
 *   this is the top level oid that we want to register under.  This
 *   is essentially a prefix, with the suffix appearing in the
 *   variable below.
 */

oid snmpNotifyFilterProfileTable_variables_oid[]
    = { 1, 3, 6, 1, 6, 3, 13, 1, 2 };

/*
 * Variable2_s snmpNotifyFilterProfileTable_variables:
 *   this variable defines function callbacks and type return information 
 *   for the snmpNotifyFilterProfileTable mib section 
 */

struct Variable2_s snmpNotifyFilterProfileTable_variables[] = {
/*
     * magic number        , variable type , ro/rw , callback fn  , L, oidsuffix 
     */
#define SNMPNOTIFYFILTERPROFILENAME 3
    { SNMPNOTIFYFILTERPROFILENAME, asnOCTET_STR, IMPL_OLDAPI_RWRITE,
        var_snmpNotifyFilterProfileTable, 2, { 1, 1 } },
#define SNMPNOTIFYFILTERPROFILESTORTYPE 4
    { SNMPNOTIFYFILTERPROFILESTORTYPE, asnINTEGER, IMPL_OLDAPI_RWRITE,
        var_snmpNotifyFilterProfileTable, 2, { 1, 2 } },
#define SNMPNOTIFYFILTERPROFILEROWSTATUS 5
    { SNMPNOTIFYFILTERPROFILEROWSTATUS, asnINTEGER, IMPL_OLDAPI_RWRITE,
        var_snmpNotifyFilterProfileTable, 2, { 1, 3 } },

};
/*
 * (L = length of the oidsuffix) 
 */

/*
 * global storage of our data, saved in and configured by header_complex() 
 */
static struct header_complex_index* snmpNotifyFilterProfileTableStorage = NULL;

/*
 * init_snmpNotifyFilterProfileTable():
 *   Initialization routine.  This is called when the agent starts up.
 *   At a minimum, registration of your variables should take place here.
 */
void init_snmpNotifyFilterProfileTable( void )
{
    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable", "initializing...  " ) );

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB( "snmpNotifyFilterProfileTable",
        snmpNotifyFilterProfileTable_variables, Variable2_s,
        snmpNotifyFilterProfileTable_variables_oid );

    /*
     * register our config handler(s) to deal with registrations 
     */
    AgentReadConfig_priotdRegisterConfigHandler( "snmpNotifyFilterProfileTable",
        parse_snmpNotifyFilterProfileTable, NULL,
        NULL );

    /*
     * we need to be called back later to store our data 
     */
    Callback_register( CallbackMajor_LIBRARY, CallbackMinor_STORE_DATA,
        store_snmpNotifyFilterProfileTable, NULL );

    /*
     * place any other initialization junk you need here 
     */

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable", "done.\n" ) );
}

/*
 * snmpNotifyFilterProfileTable_add(): adds a structure node to our data set 
 */
int snmpNotifyFilterProfileTable_add( struct snmpNotifyFilterProfileTable_data* thedata )
{
    VariableList* vars = NULL;
    int retVal;

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable", "adding data...  " ) );
    /*
     * add the index variables to the varbind list, which is 
     * used by header_complex to index the data 
     */

    Api_varlistAddVariable( &vars, NULL, 0, asnPRIV_IMPLIED_OCTET_STR,
        ( u_char* )thedata->snmpTargetParamsName,
        thedata->snmpTargetParamsNameLen );

    if ( header_complex_maybe_add_data( &snmpNotifyFilterProfileTableStorage, vars,
             thedata, 1 )
        != NULL ) {
        DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable", "registered an entry\n" ) );
        retVal = ErrorCode_SUCCESS;
    } else {
        retVal = ErrorCode_GENERR;
    }

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable", "done.\n" ) );
    return retVal;
}

/*
 * parse_snmpNotifyFilterProfileTable():
 *   parses .conf file entries needed to configure the mib.
 */
void parse_snmpNotifyFilterProfileTable( const char* token, char* line )
{
    size_t tmpint;
    struct snmpNotifyFilterProfileTable_data* StorageTmp = MEMORY_MALLOC_STRUCT( snmpNotifyFilterProfileTable_data );

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable", "parsing config...  " ) );

    if ( StorageTmp == NULL ) {
        ReadConfig_configPerror( "malloc failure" );
        return;
    }

    line = ReadConfig_readData( asnOCTET_STR, line,
        &StorageTmp->snmpTargetParamsName,
        &StorageTmp->snmpTargetParamsNameLen );
    if ( StorageTmp->snmpTargetParamsName == NULL ) {
        ReadConfig_configPerror( "invalid specification for snmpTargetParamsName" );
        return;
    }

    line = ReadConfig_readData( asnOCTET_STR, line,
        &StorageTmp->snmpNotifyFilterProfileName,
        &StorageTmp->snmpNotifyFilterProfileNameLen );
    if ( StorageTmp->snmpNotifyFilterProfileName == NULL ) {
        ReadConfig_configPerror( "invalid specification for snmpNotifyFilterProfileName" );
        MEMORY_FREE( StorageTmp );
        return;
    }

    line = ReadConfig_readData( asnINTEGER, line,
        &StorageTmp->snmpNotifyFilterProfileStorType,
        &tmpint );

    line = ReadConfig_readData( asnINTEGER, line,
        &StorageTmp->snmpNotifyFilterProfileRowStatus, &tmpint );

    if ( snmpNotifyFilterProfileTable_add( StorageTmp ) != ErrorCode_SUCCESS ) {
        MEMORY_FREE( StorageTmp->snmpTargetParamsName );
        MEMORY_FREE( StorageTmp->snmpNotifyFilterProfileName );
        MEMORY_FREE( StorageTmp );
    }

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable", "done.\n" ) );
}

/*
 * store_snmpNotifyFilterProfileTable():
 *   stores .conf file entries needed to configure the mib.
 */
int store_snmpNotifyFilterProfileTable( int majorID, int minorID,
    void* serverarg, void* clientarg )
{
    char line[ UTILITIES_MAX_BUFFER ];
    char* cptr;
    struct snmpNotifyFilterProfileTable_data* StorageTmp;
    struct header_complex_index* hcindex;

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable", "storing data...  " ) );

    for ( hcindex = snmpNotifyFilterProfileTableStorage; hcindex != NULL;
          hcindex = hcindex->next ) {
        StorageTmp = ( struct snmpNotifyFilterProfileTable_data* )hcindex->data;

        if ( ( StorageTmp->snmpNotifyFilterProfileStorType == tcSTORAGE_TYPE_NONVOLATILE ) || ( StorageTmp->snmpNotifyFilterProfileStorType == tcSTORAGE_TYPE_PERMANENT ) ) {

            memset( line, 0, sizeof( line ) );
            strcat( line, "snmpNotifyFilterProfileTable " );
            cptr = line + strlen( line );

            cptr = ReadConfig_storeData( asnOCTET_STR, cptr,
                &StorageTmp->snmpTargetParamsName,
                &StorageTmp->snmpTargetParamsNameLen );
            cptr = ReadConfig_storeData( asnOCTET_STR, cptr,
                &StorageTmp->snmpNotifyFilterProfileName,
                &StorageTmp->snmpNotifyFilterProfileNameLen );
            cptr = ReadConfig_storeData( asnINTEGER, cptr,
                &StorageTmp->snmpNotifyFilterProfileStorType,
                NULL );
            cptr = ReadConfig_storeData( asnINTEGER, cptr,
                &StorageTmp->snmpNotifyFilterProfileRowStatus,
                NULL );

            AgentReadConfig_priotdStoreConfig( line );
        }
    }
    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable", "done.\n" ) );
    return 0;
}

/*
 * var_snmpNotifyFilterProfileTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_snmpNotifyFilterProfileTable above.
 */
unsigned char*
var_snmpNotifyFilterProfileTable( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len,
    WriteMethodFT** write_method )
{

    struct snmpNotifyFilterProfileTable_data* StorageTmp = NULL;
    int found = 1;

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable",
        "var_snmpNotifyFilterProfileTable: Entering...  \n" ) );
    /*
     * this assumes you have registered all your data properly
     */
    if ( ( StorageTmp = ( struct snmpNotifyFilterProfileTable_data* )
                 header_complex( ( struct header_complex_index* )
                                     snmpNotifyFilterProfileTableStorage,
                     vp, name,
                     length, exact, var_len, write_method ) )
        == NULL ) {
        found = 0;
    }

    switch ( vp->magic ) {
    case SNMPNOTIFYFILTERPROFILENAME:
        *write_method = write_snmpNotifyFilterProfileName;
        break;

    case SNMPNOTIFYFILTERPROFILESTORTYPE:
        *write_method = write_snmpNotifyFilterProfileStorType;
        break;

    case SNMPNOTIFYFILTERPROFILEROWSTATUS:
        *write_method = write_snmpNotifyFilterProfileRowStatus;
        break;
    default:
        *write_method = NULL;
    }

    if ( !found ) {
        return NULL;
    }

    /*
     * this is where we do the value assignments for the mib results.
     */
    switch ( vp->magic ) {

    case SNMPNOTIFYFILTERPROFILENAME:
        *var_len = StorageTmp->snmpNotifyFilterProfileNameLen;
        return ( u_char* )StorageTmp->snmpNotifyFilterProfileName;

    case SNMPNOTIFYFILTERPROFILESTORTYPE:
        *var_len = sizeof( StorageTmp->snmpNotifyFilterProfileStorType );
        return ( u_char* )&StorageTmp->snmpNotifyFilterProfileStorType;

    case SNMPNOTIFYFILTERPROFILEROWSTATUS:
        *var_len = sizeof( StorageTmp->snmpNotifyFilterProfileRowStatus );
        return ( u_char* )&StorageTmp->snmpNotifyFilterProfileRowStatus;

    default:
        IMPL_ERROR_MSG( "" );
    }

    return NULL;
}

static struct snmpNotifyFilterProfileTable_data* StorageNew;

int write_snmpNotifyFilterProfileName( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP,
    oid* name, size_t name_len )
{
    static char* tmpvar;
    struct snmpNotifyFilterProfileTable_data* StorageTmp = NULL;
    static size_t tmplen;
    size_t newlen = name_len - ( sizeof( snmpNotifyFilterProfileTable_variables_oid ) / sizeof( oid ) + 3 - 1 );

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable",
        "write_snmpNotifyFilterProfileName entering action=%d...  \n",
        action ) );
    if ( action != IMPL_RESERVE1 && ( StorageTmp = ( struct snmpNotifyFilterProfileTable_data* )header_complex( ( struct header_complex_index* )snmpNotifyFilterProfileTableStorage, NULL, &name[ sizeof( snmpNotifyFilterProfileTable_variables_oid ) / sizeof( oid ) + 3 - 1 ], &newlen, 1, NULL, NULL ) ) == NULL ) {
        if ( ( StorageTmp = StorageNew ) == NULL )
            return PRIOT_ERR_NOSUCHNAME; /* remove if you support creation here */
    }

    switch ( action ) {
    case IMPL_RESERVE1:
        if ( var_val_type != asnOCTET_STR ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len < 1 || var_val_len > 32 ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
        break;

    case IMPL_RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        tmpvar = StorageTmp->snmpNotifyFilterProfileName;
        tmplen = StorageTmp->snmpNotifyFilterProfileNameLen;
        StorageTmp->snmpNotifyFilterProfileName = ( char* )calloc( 1, var_val_len + 1 );
        if ( NULL == StorageTmp->snmpNotifyFilterProfileName )
            return PRIOT_ERR_RESOURCEUNAVAILABLE;
        break;

    case IMPL_FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;

    case IMPL_ACTION:
        /*
         * The variable has been stored in string for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the IMPL_UNDO case
         */
        memcpy( StorageTmp->snmpNotifyFilterProfileName, var_val, var_val_len );
        StorageTmp->snmpNotifyFilterProfileNameLen = var_val_len;
        break;

    case IMPL_UNDO:
        /*
         * Back out any changes made in the IMPL_ACTION case
         */
        MEMORY_FREE( StorageTmp->snmpNotifyFilterProfileName );
        StorageTmp->snmpNotifyFilterProfileName = tmpvar;
        StorageTmp->snmpNotifyFilterProfileNameLen = tmplen;
        break;

    case IMPL_COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        MEMORY_FREE( tmpvar );
        Api_storeNeeded( NULL );
        break;
    }
    return PRIOT_ERR_NOERROR;
}

int write_snmpNotifyFilterProfileStorType( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP,
    oid* name, size_t name_len )
{
    static int tmpvar;
    long value = *( ( long* )var_val );
    struct snmpNotifyFilterProfileTable_data* StorageTmp = NULL;
    size_t newlen = name_len - ( sizeof( snmpNotifyFilterProfileTable_variables_oid ) / sizeof( oid ) + 3 - 1 );

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable",
        "write_snmpNotifyFilterProfileStorType entering action=%d...  \n",
        action ) );
    if ( action != IMPL_RESERVE1 && ( StorageTmp = ( struct snmpNotifyFilterProfileTable_data* )header_complex( ( struct header_complex_index* )snmpNotifyFilterProfileTableStorage, NULL, &name[ sizeof( snmpNotifyFilterProfileTable_variables_oid ) / sizeof( oid ) + 3 - 1 ], &newlen, 1, NULL, NULL ) ) == NULL ) {
        if ( ( StorageTmp = StorageNew ) == NULL )
            return PRIOT_ERR_NOSUCHNAME; /* remove if you support creation here */
    }

    switch ( action ) {
    case IMPL_RESERVE1:
        if ( var_val_type != asnINTEGER ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long ) ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
        if ( value != PRIOT_STORAGE_OTHER && value != PRIOT_STORAGE_VOLATILE
            && value != PRIOT_STORAGE_NONVOLATILE ) {
            return PRIOT_ERR_WRONGVALUE;
        }
        break;

    case IMPL_RESERVE2:
        /*
         * memory reseveration, final preparation... 
         */
        break;

    case IMPL_FREE:
        /*
         * Release any resources that have been allocated 
         */
        break;

    case IMPL_ACTION:
        /*
         * The variable has been stored in long_ret for
         * you to use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in the IMPL_UNDO case
         */
        tmpvar = StorageTmp->snmpNotifyFilterProfileStorType;
        StorageTmp->snmpNotifyFilterProfileStorType = *( ( long* )var_val );
        break;

    case IMPL_UNDO:
        /*
         * Back out any changes made in the IMPL_ACTION case
         */
        StorageTmp->snmpNotifyFilterProfileStorType = tmpvar;
        break;

    case IMPL_COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        Api_storeNeeded( NULL );
        break;
    }
    return PRIOT_ERR_NOERROR;
}

int write_snmpNotifyFilterProfileRowStatus( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP,
    oid* name, size_t name_len )
{
    struct snmpNotifyFilterProfileTable_data* StorageTmp = NULL;
    static struct snmpNotifyFilterProfileTable_data* StorageDel;
    size_t newlen = name_len - ( sizeof( snmpNotifyFilterProfileTable_variables_oid ) / sizeof( oid ) + 3 - 1 );
    static int old_value;
    int set_value = *( ( long* )var_val );
    VariableList* vars;
    struct header_complex_index* hciptr;

    DEBUG_MSGTL( ( "snmpNotifyFilterProfileTable",
        "write_snmpNotifyFilterProfileRowStatus entering action=%d...  \n",
        action ) );
    StorageTmp = ( struct snmpNotifyFilterProfileTable_data* )
        header_complex( ( struct header_complex_index* )
                            snmpNotifyFilterProfileTableStorage,
            NULL,
            &name[ sizeof( snmpNotifyFilterProfileTable_variables_oid ) / sizeof( oid ) + 3 - 1 ], &newlen, 1, NULL, NULL );

    switch ( action ) {
    case IMPL_RESERVE1:
        if ( var_val_type != asnINTEGER || var_val == NULL ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long ) ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
        if ( set_value < 1 || set_value > 6 || set_value == tcROW_STATUS_NOTREADY ) {
            return PRIOT_ERR_WRONGVALUE;
        }
        /*
         * stage one: test validity 
         */
        if ( StorageTmp == NULL ) {
            /*
             * create the row now? 
             */

            /*
             * ditch illegal values now 
             */
            if ( set_value == tcROW_STATUS_ACTIVE || set_value == tcROW_STATUS_NOTINSERVICE ) {
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
        } else {
            /*
             * row exists.  Check for a valid state change 
             */
            if ( set_value == tcROW_STATUS_CREATEANDGO
                || set_value == tcROW_STATUS_CREATEANDWAIT ) {
                /*
                 * can't create a row that exists 
                 */
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            if ( ( set_value == tcROW_STATUS_ACTIVE || set_value == tcROW_STATUS_NOTINSERVICE ) && StorageTmp->snmpNotifyFilterProfileNameLen == 0 ) {
                /*
                 * can't activate row without a profile name
                 */
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            /*
             * XXX: interaction with row storage type needed 
             */
        }

        /*
         * memory reseveration, final preparation... 
         */
        if ( StorageTmp == NULL && ( set_value == tcROW_STATUS_CREATEANDGO
                                       || set_value == tcROW_STATUS_CREATEANDWAIT ) ) {
            /*
             * creation 
             */
            vars = NULL;

            Api_varlistAddVariable( &vars, NULL, 0,
                asnPRIV_IMPLIED_OCTET_STR, NULL, 0 );

            if ( header_complex_parse_oid( &( name
                                                   [ sizeof( snmpNotifyFilterProfileTable_variables_oid ) / sizeof( oid ) + 2 ] ),
                     newlen, vars )
                != ErrorCode_SUCCESS ) {
                Api_freeVar( vars );
                return PRIOT_ERR_INCONSISTENTNAME;
            }

            StorageNew = MEMORY_MALLOC_STRUCT( snmpNotifyFilterProfileTable_data );
            if ( StorageNew == NULL )
                return PRIOT_ERR_GENERR;
            StorageNew->snmpTargetParamsName = ( char* )
                Memory_memdup( vars->value.string, vars->valueLength );
            StorageNew->snmpTargetParamsNameLen = vars->valueLength;
            StorageNew->snmpNotifyFilterProfileStorType = tcSTORAGE_TYPE_NONVOLATILE;

            StorageNew->snmpNotifyFilterProfileRowStatus = tcROW_STATUS_NOTREADY;
            Api_freeVar( vars );
        }

        break;

    case IMPL_RESERVE2:
        break;

    case IMPL_FREE:
        /*
         * XXX: free, zero vars 
         */
        /*
         * Release any resources that have been allocated 
         */
        if ( StorageNew != NULL ) {
            MEMORY_FREE( StorageNew->snmpTargetParamsName );
            MEMORY_FREE( StorageNew->snmpNotifyFilterProfileName );
            free( StorageNew );
            StorageNew = NULL;
        }
        break;

    case IMPL_ACTION:
        /*
         * The variable has been stored in set_value for you to
         * use, and you have just been asked to do something with
         * it.  Note that anything done here must be reversable in
         * the IMPL_UNDO case
         */

        if ( StorageTmp == NULL && ( set_value == tcROW_STATUS_CREATEANDGO || set_value == tcROW_STATUS_CREATEANDWAIT ) ) {
            /*
             * row creation, so add it 
             */
            if ( StorageNew != NULL )
                snmpNotifyFilterProfileTable_add( StorageNew );
            /*
             * XXX: ack, and if it is NULL? 
             */
        } else if ( set_value != tcROW_STATUS_DESTROY ) {
            /*
             * set the flag? 
             */
            if ( StorageTmp == NULL )
                return PRIOT_ERR_GENERR; /* should never ever get here */

            old_value = StorageTmp->snmpNotifyFilterProfileRowStatus;
            StorageTmp->snmpNotifyFilterProfileRowStatus = *( ( long* )var_val );
        } else {
            /*
             * destroy...  extract it for now 
             */
            if ( StorageTmp ) {
                hciptr = header_complex_find_entry( snmpNotifyFilterProfileTableStorage, StorageTmp );
                StorageDel = ( struct snmpNotifyFilterProfileTable_data* )
                    header_complex_extract_entry( ( struct
                                                      header_complex_index** )&snmpNotifyFilterProfileTableStorage,
                        hciptr );
            }
        }
        break;

    case IMPL_UNDO:
        /*
         * Back out any changes made in the IMPL_ACTION case
         */
        if ( StorageTmp == NULL && ( set_value == tcROW_STATUS_CREATEANDGO || set_value == tcROW_STATUS_CREATEANDWAIT ) ) {
            /*
             * row creation, so remove it again 
             */
            hciptr = header_complex_find_entry( snmpNotifyFilterProfileTableStorage, StorageNew );
            StorageDel = ( struct snmpNotifyFilterProfileTable_data* )
                header_complex_extract_entry( ( struct header_complex_index** )&snmpNotifyFilterProfileTableStorage,
                    hciptr );
            /*
             * XXX: free it 
             */
        } else if ( StorageDel != NULL ) {
            /*
             * row deletion, so add it again 
             */
            snmpNotifyFilterProfileTable_add( StorageDel );
            StorageDel = NULL;
        } else if ( set_value != tcROW_STATUS_DESTROY ) {
            if ( StorageTmp )
                StorageTmp->snmpNotifyFilterProfileRowStatus = old_value;
        }
        break;

    case IMPL_COMMIT:
        /*
         * Things are working well, so it's now safe to make the change
         * permanently.  Make sure that anything done here can't fail! 
         */
        if ( StorageDel != NULL ) {
            MEMORY_FREE( StorageDel->snmpTargetParamsName );
            MEMORY_FREE( StorageDel->snmpNotifyFilterProfileName );
            free( StorageDel );
            StorageDel = NULL;
        }
        if ( StorageTmp && set_value == tcROW_STATUS_CREATEANDGO ) {
            if ( StorageTmp->snmpNotifyFilterProfileNameLen )
                StorageTmp->snmpNotifyFilterProfileRowStatus = tcROW_STATUS_ACTIVE;
            StorageNew = NULL;
        } else if ( StorageTmp && set_value == tcROW_STATUS_CREATEANDWAIT ) {
            if ( StorageTmp->snmpNotifyFilterProfileNameLen )
                StorageTmp->snmpNotifyFilterProfileRowStatus = tcROW_STATUS_NOTINSERVICE;
            StorageNew = NULL;
        }
        Api_storeNeeded( NULL );
        break;
    }
    return PRIOT_ERR_NOERROR;
}

char* get_FilterProfileName( const char* paramName, size_t paramName_len,
    size_t* profileName_len )
{
    VariableList* vars = NULL;
    struct snmpNotifyFilterProfileTable_data* data;

    /*
     * put requested info into var structure 
     */
    Api_varlistAddVariable( &vars, NULL, 0, asnPRIV_IMPLIED_OCTET_STR,
        ( const u_char* )paramName, paramName_len );

    /*
     * get the data from the header_complex storage 
     */
    data = ( struct snmpNotifyFilterProfileTable_data* )
        header_complex_get( snmpNotifyFilterProfileTableStorage, vars );

    /*
     * free search index 
     */
    Api_freeVar( vars );

    /*
     * return the requested information (if this row is active) 
     */
    if ( data && data->snmpNotifyFilterProfileRowStatus == tcROW_STATUS_ACTIVE ) {
        *profileName_len = data->snmpNotifyFilterProfileNameLen;
        return data->snmpNotifyFilterProfileName;
    }

    *profileName_len = 0;
    return NULL;
}
