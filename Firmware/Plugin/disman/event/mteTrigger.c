/*
 * DisMan Event MIB:
 *     Core implementation of the trigger handling behaviour
 */

#include "mteTrigger.h"
#include "System/Util/Alarm.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "TableData.h"
#include "mteEvent.h"

Tdata* trigger_table_data;

oid _sysUpTime_instance[] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };
size_t _sysUpTime_inst_len = asnOID_LENGTH( _sysUpTime_instance );

long mteTriggerFailures;

/*
     * Initialize the container for the (combined) mteTrigger*Table,
     * regardless of which table initialisation routine is called first.
     */

void init_trigger_table_data( void )
{
    DEBUG_MSGTL( ( "disman:event:init", "init trigger container\n" ) );
    if ( !trigger_table_data ) {
        trigger_table_data = TableTdata_createTable( "mteTriggerTable", 0 );
        if ( !trigger_table_data ) {
            Logger_log( LOGGER_PRIORITY_ERR, "failed to create mteTriggerTable" );
            return;
        }
        DEBUG_MSGTL( ( "disman:event:init", "create trigger container (%p)\n",
            trigger_table_data ) );
    }
    mteTriggerFailures = 0;
}

/** Initializes the mteTrigger module */
void init_mteTrigger( void )
{
    init_trigger_table_data();
}

/* ===================================================
     *
     * APIs for maintaining the contents of the (combined)
     *    mteTrigger*Table container.
     *
     * =================================================== */

void _mteTrigger_dump( void )
{
    struct mteTrigger* entry;
    TdataRow* row;
    int i = 0;

    for ( row = TableTdata_rowFirst( trigger_table_data );
          row;
          row = TableTdata_rowNext( trigger_table_data, row ) ) {
        entry = ( struct mteTrigger* )row->data;
        DEBUG_MSGTL( ( "disman:event:dump", "TriggerTable entry %d: ", i ) );
        DEBUG_MSGOID( ( "disman:event:dump", row->oid_index.oids, row->oid_index.len ) );
        DEBUG_MSG( ( "disman:event:dump", "(%s, %s)",
            row->indexes->value.string,
            row->indexes->next->value.string ) );
        DEBUG_MSG( ( "disman:event:dump", ": %p, %p\n", row, entry ) );
        i++;
    }
    DEBUG_MSGTL( ( "disman:event:dump", "TriggerTable %d entries\n", i ) );
}

/*
 * Create a new row in the trigger table 
 */
TdataRow*
mteTrigger_createEntry( const char* mteOwner, char* mteTName, int fixed )
{
    struct mteTrigger* entry;
    TdataRow* row;
    size_t mteOwner_len = ( mteOwner ) ? strlen( mteOwner ) : 0;
    size_t mteTName_len = ( mteTName ) ? strlen( mteTName ) : 0;

    DEBUG_MSGTL( ( "disman:event:table", "Create trigger entry (%s, %s)\n",
        mteOwner, mteTName ) );
    /*
     * Create the mteTrigger entry, and the
     * (table-independent) row wrapper structure...
     */
    entry = MEMORY_MALLOC_TYPEDEF( struct mteTrigger );
    if ( !entry )
        return NULL;

    row = TableTdata_createRow();
    if ( !row ) {
        MEMORY_FREE( entry );
        return NULL;
    }
    row->data = entry;

    /*
     * ... initialize this row with the indexes supplied
     *     and the default values for the row...
     */
    if ( mteOwner )
        memcpy( entry->mteOwner, mteOwner, mteOwner_len );
    TableData_rowAddIndex( row, asnOCTET_STR,
        entry->mteOwner, mteOwner_len );
    if ( mteTName )
        memcpy( entry->mteTName, mteTName, mteTName_len );
    TableData_rowAddIndex( row, asnPRIV_IMPLIED_OCTET_STR,
        entry->mteTName, mteTName_len );

    /* entry->mteTriggerTest         = MTE_TRIGGER_BOOLEAN; */
    entry->mteTriggerValueID_len = 2; /* .0.0 */
    entry->mteTriggerFrequency = 600;
    memcpy( entry->mteDeltaDiscontID, _sysUpTime_instance,
        sizeof( _sysUpTime_instance ) );
    entry->mteDeltaDiscontID_len = _sysUpTime_inst_len;
    entry->mteDeltaDiscontIDType = MTE_DELTAD_TTICKS;
    entry->flags |= MTE_TRIGGER_FLAG_SYSUPT;
    entry->mteTExTest = ( MTE_EXIST_PRESENT | MTE_EXIST_ABSENT );
    entry->mteTExStartup = ( MTE_EXIST_PRESENT | MTE_EXIST_ABSENT );
    entry->mteTBoolComparison = MTE_BOOL_UNEQUAL;
    entry->flags |= MTE_TRIGGER_FLAG_BSTART;
    entry->mteTThStartup = MTE_THRESH_START_RISEFALL;

    if ( fixed )
        entry->flags |= MTE_TRIGGER_FLAG_FIXED;

    /*
     * ... and insert the row into the (common) table container
     */
    TableTdata_addRow( trigger_table_data, row );
    DEBUG_MSGTL( ( "disman:event:table", "Trigger entry created\n" ) );
    return row;
}

/*
 * Remove a row from the trigger table 
 */
void mteTrigger_removeEntry( TdataRow* row )
{
    struct mteTrigger* entry;

    if ( !row )
        return; /* Nothing to remove */
    entry = ( struct mteTrigger* )
        TableTdata_removeAndDeleteRow( trigger_table_data, row );
    if ( entry ) {
        mteTrigger_disable( entry );
        MEMORY_FREE( entry );
    }
}

/* ===================================================
     *
     * APIs for evaluating a trigger,
     *   and firing the appropriate event
     *
     * =================================================== */
const char* _ops[] = { "",
    "!=", /* MTE_BOOL_UNEQUAL      */
    "==", /* MTE_BOOL_EQUAL        */
    "<", /* MTE_BOOL_LESS         */
    "<=", /* MTE_BOOL_LESSEQUAL    */
    ">", /* MTE_BOOL_GREATER      */
    ">=" /* MTE_BOOL_GREATEREQUAL */ };

void _mteTrigger_failure( /* int error, */ const char* msg )
{
    /*
     * XXX - Send an mteTriggerFailure trap
     *           (if configured to do so)
     */
    mteTriggerFailures++;
    Logger_log( LOGGER_PRIORITY_ERR, "%s\n", msg );
    return;
}

void mteTrigger_run( unsigned int reg, void* clientarg )
{
    struct mteTrigger* entry = ( struct mteTrigger* )clientarg;
    VariableList *var, *vtmp;
    VariableList *vp1, *vp1_prev;
    VariableList *vp2, *vp2_prev;
    VariableList* dvar = NULL;
    VariableList *dv1 = NULL, *dv2 = NULL;
    VariableList sysUT_var;
    int cmp = 0, n, n2;
    long value;
    const char* reason;

    if ( !entry ) {
        Alarm_unregister( reg );
        return;
    }
    if ( !( entry->flags & MTE_TRIGGER_FLAG_ENABLED ) || !( entry->flags & MTE_TRIGGER_FLAG_ACTIVE ) || !( entry->flags & MTE_TRIGGER_FLAG_VALID ) ) {
        return;
    }

    {
        extern AgentSession* agent_processingSet;
        if ( agent_processingSet ) {
            /*
	     * netsnmp_handle_request will not be responsive to our efforts to
	     *	Retrieve the requested MIB value(s)...
	     * so we will skip it.
	     * https://sourceforge.net/tracker/
	     *	index.php?func=detail&aid=1557406&group_id=12694&atid=112694
	     */
            DEBUG_MSGTL( ( "disman:event:trigger:monitor",
                "Skipping trigger (%s) while agent_processingSet\n",
                entry->mteTName ) );
            return;
        }
    }

    /*
     * Retrieve the requested MIB value(s)...
     */
    DEBUG_MSGTL( ( "disman:event:trigger:monitor", "Running trigger (%s)\n", entry->mteTName ) );
    var = ( VariableList* )MEMORY_MALLOC_TYPEDEF( VariableList );
    if ( !var ) {
        _mteTrigger_failure( "failed to create mteTrigger query varbind" );
        return;
    }
    Client_setVarObjid( var, entry->mteTriggerValueID,
        entry->mteTriggerValueID_len );
    if ( entry->flags & MTE_TRIGGER_FLAG_VWILD ) {
        n = Client_queryWalk( var, entry->session );
    } else {
        n = Client_queryGet( var, entry->session );
    }
    if ( n != PRIOT_ERR_NOERROR ) {
        DEBUG_MSGTL( ( "disman:event:trigger:monitor", "Trigger query (%s) failed: %d\n",
            ( ( entry->flags & MTE_TRIGGER_FLAG_VWILD ) ? "walk" : "get" ), n ) );
        _mteTrigger_failure( "failed to run mteTrigger query" );
        Api_freeVarbind( var );
        return;
    }

    /*
     * ... canonicalise the results (to simplify later comparisons)...
     */

    vp1 = var;
    vp1_prev = NULL;
    vp2 = entry->old_results;
    vp2_prev = NULL;
    entry->count = 0;
    while ( vp1 ) {
        /*
            * Flatten various missing values/exceptions into a single form
            */
        switch ( vp1->type ) {
        case PRIOT_NOSUCHINSTANCE:
        case PRIOT_NOSUCHOBJECT:
        case asnPRIV_RETRY: /* Internal only ? */
            vp1->type = asnNULL;
        }
        /*
            * Keep track of how many entries have been retrieved.
            */
        entry->count++;

        /*
            * Ensure previous and current result match
            *  (with corresponding entries in both lists)
            * and set the flags indicating which triggers are armed
            */
        if ( vp2 ) {
            cmp = Api_oidCompare( vp1->name, vp1->nameLength,
                vp2->name, vp2->nameLength );
            if ( cmp < 0 ) {
                /*
                 * If a new value has appeared, insert a matching
                 * dummy entry into the previous result list.
                 *
                 * XXX - check how this is best done.
                 */
                vtmp = MEMORY_MALLOC_TYPEDEF( VariableList );
                if ( !vtmp ) {
                    _mteTrigger_failure(
                        "failed to create mteTrigger temp varbind" );
                    Api_freeVarbind( var );
                    return;
                }
                vtmp->type = asnNULL;
                Client_setVarObjid( vtmp, vp1->name, vp1->nameLength );
                vtmp->next = vp2;
                if ( vp2_prev ) {
                    vp2_prev->next = vtmp;
                } else {
                    entry->old_results = vtmp;
                }
                vp2_prev = vtmp;
                vp1->index = MTE_ARMED_ALL; /* XXX - plus a new flag */
                vp1_prev = vp1;
                vp1 = vp1->next;
            } else if ( cmp == 0 ) {
                /*
                 * If it's a continuing entry, just copy across the armed flags
                 */
                vp1->index = vp2->index;
                vp1_prev = vp1;
                vp1 = vp1->next;
                vp2_prev = vp2;
                vp2 = vp2->next;
            } else {
                /*
                 * If a value has just disappeared, insert a
                 * matching dummy entry into the current result list.
                 *
                 * XXX - check how this is best done.
                 *
                 */
                if ( vp2->type != asnNULL ) {
                    vtmp = MEMORY_MALLOC_TYPEDEF( VariableList );
                    if ( !vtmp ) {
                        _mteTrigger_failure(
                            "failed to create mteTrigger temp varbind" );
                        Api_freeVarbind( var );
                        return;
                    }
                    vtmp->type = asnNULL;
                    Client_setVarObjid( vtmp, vp2->name, vp2->nameLength );
                    vtmp->next = vp1;
                    if ( vp1_prev ) {
                        vp1_prev->next = vtmp;
                    } else {
                        var = vtmp;
                    }
                    vp1_prev = vtmp;
                    vp2_prev = vp2;
                    vp2 = vp2->next;
                } else {
                    /*
                     * But only if this entry has *just* disappeared.  If the
                     * entry from the last run was a dummy too, then remove it.
                     *   (leaving vp2_prev unchanged)
                     */
                    vtmp = vp2;
                    if ( vp2_prev ) {
                        vp2_prev->next = vp2->next;
                    } else {
                        entry->old_results = vp2->next;
                    }
                    vp2 = vp2->next;
                    vtmp->next = NULL;
                    Api_freeVarbind( vtmp );
                }
            }
        } else {
            /*
             * No more old results to compare.
             * Either all remaining values have only just been created ...
             *   (and we need to create dummy 'old' entries for them)
             */
            if ( vp2_prev ) {
                vtmp = MEMORY_MALLOC_TYPEDEF( VariableList );
                if ( !vtmp ) {
                    _mteTrigger_failure(
                        "failed to create mteTrigger temp varbind" );
                    Api_freeVarbind( var );
                    return;
                }
                vtmp->type = asnNULL;
                Client_setVarObjid( vtmp, vp1->name, vp1->nameLength );
                vtmp->next = vp2_prev->next;
                vp2_prev->next = vtmp;
                vp2_prev = vtmp;
            }
            /*
             * ... or this is the first run through
             *   (and there were no old results at all)
             *
             * In either case, mark the current entry as armed and new.
             * Note that we no longer need to maintain 'vp1_prev'
             */
            vp1->index = MTE_ARMED_ALL; /* XXX - plus a new flag */
            vp1 = vp1->next;
        }
    }

    /*
     * ... and then work through these result(s), deciding
     *     whether or not to trigger the corresponding event.
     *
     *  Note that there's no point in evaluating Existence or
     *    Boolean tests if there's no corresponding event.
     *   (Even if the trigger matched, nothing would be done anyway).
     */
    if ( ( entry->mteTriggerTest & MTE_TRIGGER_EXISTENCE ) && ( entry->mteTExEvent[ 0 ] != '\0' ) ) {
        /*
         * If we don't have a record of previous results,
         * this must be the first time through, so consider
         * the mteTriggerExistenceStartup tests.
         */
        if ( !entry->old_results ) {
            /*
             * With the 'present(0)' test, the trigger should fire
             *   for each value in the varbind list returned
             *   (whether the monitored value is wildcarded or not).
             */
            if ( entry->mteTExTest & entry->mteTExStartup & MTE_EXIST_PRESENT ) {
                for ( vp1 = var; vp1; vp1 = vp1->next ) {
                    DEBUG_MSGTL( ( "disman:event:trigger:fire",
                        "Firing initial existence test: " ) );
                    DEBUG_MSGOID( ( "disman:event:trigger:fire",
                        vp1->name, vp1->nameLength ) );
                    DEBUG_MSG( ( "disman:event:trigger:fire",
                        " (present)\n" ) );
                    ;
                    entry->mteTriggerXOwner = entry->mteTExObjOwner;
                    entry->mteTriggerXObjects = entry->mteTExObjects;
                    entry->mteTriggerFired = vp1;
                    n = entry->mteTriggerValueID_len;
                    mteEvent_fire( entry->mteTExEvOwner, entry->mteTExEvent,
                        entry, vp1->name + n, vp1->nameLength - n );
                }
            }
            /*
             * An initial 'absent(1)' test only makes sense when
             *   monitoring a non-wildcarded OID (how would we know
             *   which rows of the table "ought" to exist, but don't?)
             */
            if ( entry->mteTExTest & entry->mteTExStartup & MTE_EXIST_ABSENT ) {
                if ( !( entry->flags & MTE_TRIGGER_FLAG_VWILD ) && var->type == asnNULL ) {
                    DEBUG_MSGTL( ( "disman:event:trigger:fire",
                        "Firing initial existence test: " ) );
                    DEBUG_MSGOID( ( "disman:event:trigger:fire",
                        var->name, var->nameLength ) );
                    DEBUG_MSG( ( "disman:event:trigger:fire",
                        " (absent)\n" ) );
                    ;
                    entry->mteTriggerXOwner = entry->mteTExObjOwner;
                    entry->mteTriggerXObjects = entry->mteTExObjects;
                    /*
                     * It's unclear what value the 'mteHotValue' payload
                     *  should take when a monitored instance does not
                     *  exist on startup. The only sensible option is
                     *  to report a NULL value, but this clashes with
                     * the syntax of the mteHotValue MIB object.
                     */
                    entry->mteTriggerFired = var;
                    n = entry->mteTriggerValueID_len;
                    mteEvent_fire( entry->mteTExEvOwner, entry->mteTExEvent,
                        entry, var->name + n, var->nameLength - n );
                }
            }
        } /* !old_results */
        /*
             * Otherwise, compare the current set of results with
             * the previous ones, looking for changes.  We can
             * assume that the two lists match (see above).
             */
        else {
            for ( vp1 = var, vp2 = entry->old_results;
                  vp1;
                  vp1 = vp1->next, vp2 = vp2->next ) {

                /* Use this field to indicate that the trigger should fire */
                entry->mteTriggerFired = NULL;
                reason = NULL;

                if ( ( entry->mteTExTest & MTE_EXIST_PRESENT ) && ( vp1->type != asnNULL ) && ( vp2->type == asnNULL ) ) {
                    /* A new instance has appeared */
                    entry->mteTriggerFired = vp1;
                    reason = "(present)";

                } else if ( ( entry->mteTExTest & MTE_EXIST_ABSENT ) && ( vp1->type == asnNULL ) && ( vp2->type != asnNULL ) ) {

                    /*
                     * A previous instance has disappeared.
                     *
                     * It's unclear what value the 'mteHotValue' payload
                     *  should take when this happens - the previous
                     *  value (vp2), or a NULL value (vp1) ?
                     * NULL makes more sense logically, but clashes
                     *  with the syntax of the mteHotValue MIB object.
                     */
                    entry->mteTriggerFired = vp2;
                    reason = "(absent)";

                } else if ( ( entry->mteTExTest & MTE_EXIST_CHANGED ) && ( ( vp1->valueLength != vp2->valueLength ) || ( memcmp( vp1->value.string, vp2->value.string,
                                                                                                                   vp1->valueLength )
                                                                                                                 != 0 ) ) ) {
                    /*
                     * This comparison detects changes in *any* type
                     *  of value, numeric or string (or even OID).
                     *
                     * Unfortunately, the default 'mteTriggerFired'
                     *  notification payload can't report non-numeric
                     *  changes properly (see syntax of 'mteHotValue')
                     */
                    entry->mteTriggerFired = vp1;
                    reason = "(changed)";
                }
                if ( entry->mteTriggerFired ) {
                    /*
                     * One of the above tests has matched,
                     *   so fire the trigger.
                     */
                    DEBUG_MSGTL( ( "disman:event:trigger:fire",
                        "Firing existence test: " ) );
                    DEBUG_MSGOID( ( "disman:event:trigger:fire",
                        vp1->name, vp1->nameLength ) );
                    DEBUG_MSG( ( "disman:event:trigger:fire",
                        " %s\n", reason ) );
                    ;
                    entry->mteTriggerXOwner = entry->mteTExObjOwner;
                    entry->mteTriggerXObjects = entry->mteTExObjects;
                    n = entry->mteTriggerValueID_len;
                    mteEvent_fire( entry->mteTExEvOwner, entry->mteTExEvent,
                        entry, vp1->name + n, vp1->nameLength - n );
                }
            }
        } /* !old_results - end of else block */
    } /* MTE_TRIGGER_EXISTENCE */

    /*
     * We'll need sysUpTime.0 regardless...
     */
    DEBUG_MSGTL( ( "disman:event:delta", "retrieve sysUpTime.0\n" ) );
    memset( &sysUT_var, 0, sizeof( VariableList ) );
    Client_setVarObjid( &sysUT_var, _sysUpTime_instance, _sysUpTime_inst_len );
    Client_queryGet( &sysUT_var, entry->session );

    if ( ( entry->mteTriggerTest & MTE_TRIGGER_BOOLEAN ) || ( entry->mteTriggerTest & MTE_TRIGGER_THRESHOLD ) ) {
        /*
         * Although Existence tests can work with any syntax values,
         * Boolean and Threshold tests are integer-only.  Ensure that
         * the returned value(s) are appropriate.
         *
         * Note that we only need to check the first value, since all
         *  instances of a given object should have the same syntax.
         */
        switch ( var->type ) {
        case asnINTEGER:
        case asnCOUNTER:
        case asnGAUGE:
        case asnTIMETICKS:
        case asnUINTEGER:
        case asnCOUNTER64:
        case asnOPAQUE_COUNTER64:
        case asnOPAQUE_U64:
        case asnOPAQUE_I64:
            /* OK */
            break;
        default:
            /*
             * Other syntax values can't be used for Boolean/Theshold
             * tests. Report this as an error, and then rotate the
             * results ready for the next run, (which will presumably
             * also detect this as an error once again!)
             */
            DEBUG_MSGTL( ( "disman:event:trigger:fire",
                "Returned non-integer result(s): " ) );
            DEBUG_MSGOID( ( "disman:event:trigger:fire",
                var->name, var->nameLength ) );
            DEBUG_MSG( ( "disman:event:trigger:fire",
                " (boolean/threshold) %d\n", var->type ) );
            ;
            Api_freeVarbind( entry->old_results );
            entry->old_results = var;
            return;
        }

        /*
         * Retrieve the discontinuity markers for delta-valued samples.
         * (including sysUpTime.0 if not specified explicitly).
         */
        if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {

            if ( !( entry->flags & MTE_TRIGGER_FLAG_SYSUPT ) ) {
                /*
                 * ... but only retrieve the configured discontinuity
                 *      marker(s) if they refer to something different.
                 */
                DEBUG_MSGTL( ( "disman:event:delta",
                    "retrieve discontinuity marker(s): " ) );
                DEBUG_MSGOID( ( "disman:event:delta", entry->mteDeltaDiscontID,
                    entry->mteDeltaDiscontID_len ) );
                DEBUG_MSG( ( "disman:event:delta", " %s\n",
                    ( entry->flags & MTE_TRIGGER_FLAG_DWILD ? " (wild)" : "" ) ) );

                dvar = ( VariableList* )
                    MEMORY_MALLOC_TYPEDEF( VariableList );
                if ( !dvar ) {
                    _mteTrigger_failure(
                        "failed to create mteTrigger delta query varbind" );
                    return;
                }
                Client_setVarObjid( dvar, entry->mteDeltaDiscontID,
                    entry->mteDeltaDiscontID_len );
                if ( entry->flags & MTE_TRIGGER_FLAG_DWILD ) {
                    n = Client_queryWalk( dvar, entry->session );
                } else {
                    n = Client_queryGet( dvar, entry->session );
                }
                if ( n != PRIOT_ERR_NOERROR ) {
                    _mteTrigger_failure( "failed to run mteTrigger delta query" );
                    Api_freeVarbind( dvar );
                    return;
                }
            }

            /*
             * We can't calculate delta values the first time through,
             *  so there's no point in evaluating the remaining tests.
             *
             * Save the results (and discontinuity markers),
             *   ready for the next run.
             */
            if ( !entry->old_results ) {
                entry->old_results = var;
                entry->old_deltaDs = dvar;
                entry->sysUpTime = *sysUT_var.value.integer;
                return;
            }
            /*
             * If the sysUpTime marker has been reset (or strictly,
             *   has advanced by less than the monitor frequency),
             *  there's no point in trying the remaining tests.
             */

            if ( *sysUT_var.value.integer < entry->sysUpTime ) {
                DEBUG_MSGTL( ( "disman:event:delta",
                    "single discontinuity: (sysUT)\n" ) );
                Api_freeVarbind( entry->old_results );
                Api_freeVarbind( entry->old_deltaDs );
                entry->old_results = var;
                entry->old_deltaDs = dvar;
                entry->sysUpTime = *sysUT_var.value.integer;
                return;
            }
            /*
             * Similarly if a separate (non-wildcarded) discontinuity
             *  marker has changed, then there's no
             *  point in trying to evaluate these tests either.
             */
            if ( !( entry->flags & MTE_TRIGGER_FLAG_DWILD ) && !( entry->flags & MTE_TRIGGER_FLAG_SYSUPT ) && ( !entry->old_deltaDs || ( entry->old_deltaDs->value.integer != dvar->value.integer ) ) ) {
                DEBUG_MSGTL( ( "disman:event:delta", "single discontinuity: (" ) );
                DEBUG_MSGOID( ( "disman:event:delta", entry->mteDeltaDiscontID,
                    entry->mteDeltaDiscontID_len ) );
                DEBUG_MSG( ( "disman:event:delta", ")\n" ) );
                Api_freeVarbind( entry->old_results );
                Api_freeVarbind( entry->old_deltaDs );
                entry->old_results = var;
                entry->old_deltaDs = dvar;
                entry->sysUpTime = *sysUT_var.value.integer;
                return;
            }

            /*
             * Ensure that the list of (wildcarded) discontinuity 
             *  markers matches the list of monitored values
             *  (inserting/removing discontinuity varbinds as needed)
             *
             * XXX - An alternative approach would be to use the list
             *    of monitored values (instance subidentifiers) to build
             *    the exact list of delta markers to retrieve earlier.
             */
            if ( entry->flags & MTE_TRIGGER_FLAG_DWILD ) {
                vp1 = var;
                vp2 = dvar;
                vp2_prev = NULL;
                n = entry->mteTriggerValueID_len;
                n2 = entry->mteDeltaDiscontID_len;
                while ( vp1 ) {
                    /*
                     * For each monitored instance, check whether
                     *   there's a matching discontinuity entry.
                     */
                    cmp = Api_oidCompare( vp1->name + n, vp1->nameLength - n,
                        vp2->name + n2, vp2->nameLength - n2 );
                    if ( cmp < 0 ) {
                        /*
                         * If a discontinuity entry is missing,
                         *   insert a (dummy) varbind.
                         * The corresponding delta calculation will
                         *   fail, but this simplifies the later code.
                         */
                        vtmp = ( VariableList* )
                            MEMORY_MALLOC_TYPEDEF( VariableList );
                        if ( !vtmp ) {
                            _mteTrigger_failure(
                                "failed to create mteTrigger discontinuity varbind" );
                            Api_freeVarbind( dvar );
                            return;
                        }
                        Client_setVarObjid( vtmp, entry->mteDeltaDiscontID,
                            entry->mteDeltaDiscontID_len );
                        /* XXX - append instance subids */
                        vtmp->next = vp2;
                        vp2_prev->next = vtmp;
                        vp2_prev = vtmp;
                        vp1 = vp1->next;
                    } else if ( cmp == 0 ) {
                        /*
                         * Matching discontinuity entry -  all OK.
                         */
                        vp2_prev = vp2;
                        vp2 = vp2->next;
                        vp1 = vp1->next;
                    } else {
                        /*
                         * Remove unneeded discontinuity entry
                         */
                        vtmp = vp2;
                        vp2_prev->next = vp2->next;
                        vp2 = vp2->next;
                        vtmp->next = NULL;
                        Api_freeVarbind( vtmp );
                    }
                }
                /*
                 * XXX - Now need to ensure that the old list of
                 *   delta discontinuity markers matches as well.
                 */
            }
        } /* delta samples */
    } /* Boolean/Threshold test checks */

    /*
     * Only run the Boolean tests if there's an event to be triggered
     */
    if ( ( entry->mteTriggerTest & MTE_TRIGGER_BOOLEAN ) && ( entry->mteTBoolEvent[ 0 ] != '\0' ) ) {

        if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {
            vp2 = entry->old_results;
            if ( entry->flags & MTE_TRIGGER_FLAG_DWILD ) {
                dv1 = dvar;
                dv2 = entry->old_deltaDs;
            }
        }
        for ( vp1 = var; vp1; vp1 = vp1->next ) {
            /*
             * Determine the value to be monitored...
             */
            if ( !vp1->value.integer ) { /* No value */
                if ( vp2 )
                    vp2 = vp2->next;
                continue;
            }
            if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {
                if ( entry->flags & MTE_TRIGGER_FLAG_DWILD ) {
                    /*
                     * We've already checked any non-wildcarded
                     *   discontinuity markers (inc. sysUpTime.0).
                     * Validate this particular sample against
                     *   the relevant wildcarded marker...
                     */
                    if ( ( dv1->type == asnNULL ) || ( dv1->type != dv2->type ) || ( *dv1->value.integer != *dv2->value.integer ) ) {
                        /*
                         * Bogus or changed discontinuity marker.
                         * Need to skip this sample.
                         */
                        DEBUG_MSGTL( ( "disman:event:delta", "discontinuity occurred: " ) );
                        DEBUG_MSGOID( ( "disman:event:delta", vp1->name,
                            vp1->nameLength ) );
                        DEBUG_MSG( ( "disman:event:delta", " \n" ) );
                        vp2 = vp2->next;
                        continue;
                    }
                }
                /*
                 * ... and check there is a previous sample to calculate
                 *   the delta value against (regardless of whether the
                 *   discontinuity marker was wildcarded or not).
                 */
                if ( vp2->type == asnNULL ) {
                    DEBUG_MSGTL( ( "disman:event:delta", "missing sample: " ) );
                    DEBUG_MSGOID( ( "disman:event:delta", vp1->name,
                        vp1->nameLength ) );
                    DEBUG_MSG( ( "disman:event:delta", " \n" ) );
                    vp2 = vp2->next;
                    continue;
                }
                value = ( *vp1->value.integer - *vp2->value.integer );
                DEBUG_MSGTL( ( "disman:event:delta", "delta sample: " ) );
                DEBUG_MSGOID( ( "disman:event:delta", vp1->name,
                    vp1->nameLength ) );
                DEBUG_MSG( ( "disman:event:delta", " (%ld - %ld) = %ld\n",
                    *vp1->value.integer, *vp2->value.integer, value ) );
                vp2 = vp2->next;
            } else {
                value = *vp1->value.integer;
            }

            /*
             * ... evaluate the comparison ...
             */
            switch ( entry->mteTBoolComparison ) {
            case MTE_BOOL_UNEQUAL:
                cmp = ( value != entry->mteTBoolValue );
                break;
            case MTE_BOOL_EQUAL:
                cmp = ( value == entry->mteTBoolValue );
                break;
            case MTE_BOOL_LESS:
                cmp = ( value < entry->mteTBoolValue );
                break;
            case MTE_BOOL_LESSEQUAL:
                cmp = ( value <= entry->mteTBoolValue );
                break;
            case MTE_BOOL_GREATER:
                cmp = ( value > entry->mteTBoolValue );
                break;
            case MTE_BOOL_GREATEREQUAL:
                cmp = ( value >= entry->mteTBoolValue );
                break;
            }
            DEBUG_MSGTL( ( "disman:event:delta", "Bool comparison: (%ld %s %ld) %d\n",
                value, _ops[ entry->mteTBoolComparison ],
                entry->mteTBoolValue, cmp ) );

            /*
             * ... and decide whether to trigger the event.
             *    (using the 'index' field of the varbind structure
             *     to remember whether the trigger has already fired)
             */
            if ( cmp ) {
                if ( vp1->index & MTE_ARMED_BOOLEAN ) {
                    vp1->index &= ~MTE_ARMED_BOOLEAN;
                    /*
                 * NB: Clear the trigger armed flag even if the
                 *   (starting) event dosn't actually fire.
                 *   Otherwise initially true (but suppressed)
                 *   triggers will fire on the *second* probe.
                 */
                    if ( entry->old_results || ( entry->flags & MTE_TRIGGER_FLAG_BSTART ) ) {
                        DEBUG_MSGTL( ( "disman:event:trigger:fire",
                            "Firing boolean test: " ) );
                        DEBUG_MSGOID( ( "disman:event:trigger:fire",
                            vp1->name, vp1->nameLength ) );
                        DEBUG_MSG( ( "disman:event:trigger:fire", "%s\n",
                            ( entry->old_results ? "" : " (startup)" ) ) );
                        entry->mteTriggerXOwner = entry->mteTBoolObjOwner;
                        entry->mteTriggerXObjects = entry->mteTBoolObjects;
                        /*
                     * XXX - when firing a delta-based trigger, should
                     *   'mteHotValue' report the actual value sampled
                     *   (as here), or the delta that triggered the event ?
                     */
                        entry->mteTriggerFired = vp1;
                        n = entry->mteTriggerValueID_len;
                        mteEvent_fire( entry->mteTBoolEvOwner, entry->mteTBoolEvent,
                            entry, vp1->name + n, vp1->nameLength - n );
                    }
                }
            } else {
                vp1->index |= MTE_ARMED_BOOLEAN;
            }
        }
    }

    /*
     * Only run the basic threshold tests if there's an event to
     *    be triggered.  (Either rising or falling will do)
     */
    if ( ( entry->mteTriggerTest & MTE_TRIGGER_THRESHOLD ) && ( ( entry->mteTThRiseEvent[ 0 ] != '\0' ) || ( entry->mteTThFallEvent[ 0 ] != '\0' ) ) ) {

        /*
         * The same delta-sample validation from Boolean
         *   tests also applies here too.
         */
        if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {
            vp2 = entry->old_results;
            if ( entry->flags & MTE_TRIGGER_FLAG_DWILD ) {
                dv1 = dvar;
                dv2 = entry->old_deltaDs;
            }
        }
        for ( vp1 = var; vp1; vp1 = vp1->next ) {
            /*
             * Determine the value to be monitored...
             */
            if ( !vp1->value.integer ) { /* No value */
                if ( vp2 )
                    vp2 = vp2->next;
                continue;
            }
            if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {
                if ( entry->flags & MTE_TRIGGER_FLAG_DWILD ) {
                    /*
                     * We've already checked any non-wildcarded
                     *   discontinuity markers (inc. sysUpTime.0).
                     * Validate this particular sample against
                     *   the relevant wildcarded marker...
                     */
                    if ( ( dv1->type == asnNULL ) || ( dv1->type != dv2->type ) || ( *dv1->value.integer != *dv2->value.integer ) ) {
                        /*
                         * Bogus or changed discontinuity marker.
                         * Need to skip this sample.
                         */
                        vp2 = vp2->next;
                        continue;
                    }
                }
                /*
                 * ... and check there is a previous sample to calculate
                 *   the delta value against (regardless of whether the
                 *   discontinuity marker was wildcarded or not).
                 */
                if ( vp2->type == asnNULL ) {
                    vp2 = vp2->next;
                    continue;
                }
                value = ( *vp1->value.integer - *vp2->value.integer );
                vp2 = vp2->next;
            } else {
                value = *vp1->value.integer;
            }

            /*
             * ... evaluate the single-value comparisons,
             *     and decide whether to trigger the event.
             */
            cmp = vp1->index; /* working copy of 'armed' flags */
            if ( value >= entry->mteTThRiseValue ) {
                if ( cmp & MTE_ARMED_TH_RISE ) {
                    cmp &= ~MTE_ARMED_TH_RISE;
                    cmp |= MTE_ARMED_TH_FALL;
                    /*
                 * NB: Clear the trigger armed flag even if the
                 *   (starting) event dosn't actually fire.
                 *   Otherwise initially true (but suppressed)
                 *   triggers will fire on the *second* probe.
                 * Similarly for falling thresholds (see below).
                 */
                    if ( entry->old_results || ( entry->mteTThStartup & MTE_THRESH_START_RISE ) ) {
                        DEBUG_MSGTL( ( "disman:event:trigger:fire",
                            "Firing rising threshold test: " ) );
                        DEBUG_MSGOID( ( "disman:event:trigger:fire",
                            vp1->name, vp1->nameLength ) );
                        DEBUG_MSG( ( "disman:event:trigger:fire", "%s\n",
                            ( entry->old_results ? "" : " (startup)" ) ) );
                        /*
                     * If no riseEvent is configured, we need still to
                     *  set the armed flags appropriately, but there's
                     *  no point in trying to fire the (missing) event.
                     */
                        if ( entry->mteTThRiseEvent[ 0 ] != '\0' ) {
                            entry->mteTriggerXOwner = entry->mteTThObjOwner;
                            entry->mteTriggerXObjects = entry->mteTThObjects;
                            entry->mteTriggerFired = vp1;
                            n = entry->mteTriggerValueID_len;
                            mteEvent_fire( entry->mteTThRiseOwner,
                                entry->mteTThRiseEvent,
                                entry, vp1->name + n, vp1->nameLength - n );
                        }
                    }
                }
            }

            if ( value <= entry->mteTThFallValue ) {
                if ( cmp & MTE_ARMED_TH_FALL ) {
                    cmp &= ~MTE_ARMED_TH_FALL;
                    cmp |= MTE_ARMED_TH_RISE;
                    /* Clear the trigger armed flag (see above) */
                    if ( entry->old_results || ( entry->mteTThStartup & MTE_THRESH_START_FALL ) ) {
                        DEBUG_MSGTL( ( "disman:event:trigger:fire",
                            "Firing falling threshold test: " ) );
                        DEBUG_MSGOID( ( "disman:event:trigger:fire",
                            vp1->name, vp1->nameLength ) );
                        DEBUG_MSG( ( "disman:event:trigger:fire", "%s\n",
                            ( entry->old_results ? "" : " (startup)" ) ) );
                        /*
                     * Similarly, if no fallEvent is configured,
                     *  there's no point in trying to fire it either.
                     */
                        if ( entry->mteTThRiseEvent[ 0 ] != '\0' ) {
                            entry->mteTriggerXOwner = entry->mteTThObjOwner;
                            entry->mteTriggerXObjects = entry->mteTThObjects;
                            entry->mteTriggerFired = vp1;
                            n = entry->mteTriggerValueID_len;
                            mteEvent_fire( entry->mteTThFallOwner,
                                entry->mteTThFallEvent,
                                entry, vp1->name + n, vp1->nameLength - n );
                        }
                    }
                }
            }
            vp1->index = cmp;
        }
    }

    /*
     * The same processing also works for delta-threshold tests (if configured)
     */
    if ( ( entry->mteTriggerTest & MTE_TRIGGER_THRESHOLD ) && ( ( entry->mteTThDRiseEvent[ 0 ] != '\0' ) || ( entry->mteTThDFallEvent[ 0 ] != '\0' ) ) ) {

        /*
         * Delta-threshold tests can only be used with
         *   absolute valued samples.
         */
        vp2 = entry->old_results;
        if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {
            DEBUG_MSGTL( ( "disman:event:trigger",
                "Delta-threshold on delta-sample\n" ) );
        } else if ( vp2 != NULL ) {
            for ( vp1 = var; vp1; vp1 = vp1->next ) {
                /*
             * Determine the value to be monitored...
             *  (similar to previous delta-sample processing,
             *   but without the discontinuity marker checks)
             */
                if ( !vp2 ) {
                    break; /* Run out of 'old' values */
                }
                if ( ( !vp1->value.integer ) || ( vp2->type == asnNULL ) ) {
                    vp2 = vp2->next;
                    continue;
                }
                value = ( *vp1->value.integer - *vp2->value.integer );
                vp2 = vp2->next;

                /*
             * ... evaluate the single-value comparisons,
             *     and decide whether to trigger the event.
             */
                cmp = vp1->index; /* working copy of 'armed' flags */
                if ( value >= entry->mteTThDRiseValue ) {
                    if ( vp1->index & MTE_ARMED_TH_DRISE ) {
                        DEBUG_MSGTL( ( "disman:event:trigger:fire",
                            "Firing rising delta threshold test: " ) );
                        DEBUG_MSGOID( ( "disman:event:trigger:fire",
                            vp1->name, vp1->nameLength ) );
                        DEBUG_MSG( ( "disman:event:trigger:fire", "\n" ) );
                        cmp &= ~MTE_ARMED_TH_DRISE;
                        cmp |= MTE_ARMED_TH_DFALL;
                        /*
                     * If no riseEvent is configured, we need still to
                     *  set the armed flags appropriately, but there's
                     *  no point in trying to fire the (missing) event.
                     */
                        if ( entry->mteTThDRiseEvent[ 0 ] != '\0' ) {
                            entry->mteTriggerXOwner = entry->mteTThObjOwner;
                            entry->mteTriggerXObjects = entry->mteTThObjects;
                            entry->mteTriggerFired = vp1;
                            n = entry->mteTriggerValueID_len;
                            mteEvent_fire( entry->mteTThDRiseOwner,
                                entry->mteTThDRiseEvent,
                                entry, vp1->name + n, vp1->nameLength - n );
                        }
                    }
                }

                if ( value <= entry->mteTThDFallValue ) {
                    if ( vp1->index & MTE_ARMED_TH_DFALL ) {
                        DEBUG_MSGTL( ( "disman:event:trigger:fire",
                            "Firing falling delta threshold test: " ) );
                        DEBUG_MSGOID( ( "disman:event:trigger:fire",
                            vp1->name, vp1->nameLength ) );
                        DEBUG_MSG( ( "disman:event:trigger:fire", "\n" ) );
                        cmp &= ~MTE_ARMED_TH_DFALL;
                        cmp |= MTE_ARMED_TH_DRISE;
                        /*
                     * Similarly, if no fallEvent is configured,
                     *  there's no point in trying to fire it either.
                     */
                        if ( entry->mteTThDRiseEvent[ 0 ] != '\0' ) {
                            entry->mteTriggerXOwner = entry->mteTThObjOwner;
                            entry->mteTriggerXObjects = entry->mteTThObjects;
                            entry->mteTriggerFired = vp1;
                            n = entry->mteTriggerValueID_len;
                            mteEvent_fire( entry->mteTThDFallOwner,
                                entry->mteTThDFallEvent,
                                entry, vp1->name + n, vp1->nameLength - n );
                        }
                    }
                }
                vp1->index = cmp;
            }
        }
    }

    /*
     * Finally, rotate the results - ready for the next run.
     */
    Api_freeVarbind( entry->old_results );
    entry->old_results = var;
    if ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) {
        Api_freeVarbind( entry->old_deltaDs );
        entry->old_deltaDs = dvar;
        entry->sysUpTime = *sysUT_var.value.integer;
    }
}

void mteTrigger_enable( struct mteTrigger* entry )
{
    if ( !entry )
        return;

    if ( entry->alarm ) {
        /* XXX - or explicitly call mteTrigger_disable ?? */
        Alarm_unregister( entry->alarm );
        entry->alarm = 0;
    }

    if ( entry->mteTriggerFrequency ) {
        /*
         * register once to run ASAP, and another to run
         * at the trigger frequency
         */
        Alarm_register( 0, 0, mteTrigger_run, entry );
        entry->alarm = Alarm_register(
            entry->mteTriggerFrequency, AlarmFlag_REPEAT,
            mteTrigger_run, entry );
    }
}

void mteTrigger_disable( struct mteTrigger* entry )
{
    if ( !entry )
        return;

    if ( entry->alarm ) {
        Alarm_unregister( entry->alarm );
        entry->alarm = 0;
        /* XXX - perhaps release any previous results */
    }
}

long _mteTrigger_MaxCount = 0;
long _mteTrigger_countEntries( void )
{
    struct mteTrigger* entry;
    TdataRow* row;
    long count = 0;

    for ( row = TableTdata_rowFirst( trigger_table_data );
          row;
          row = TableTdata_rowNext( trigger_table_data, row ) ) {
        entry = ( struct mteTrigger* )row->data;
        count += entry->count;
    }

    return count;
}

long mteTrigger_getNumEntries( int max )
{
    long count;
    /* XXX - implement some form of caching ??? */
    count = _mteTrigger_countEntries();
    if ( count > _mteTrigger_MaxCount )
        _mteTrigger_MaxCount = count;

    return ( max ? _mteTrigger_MaxCount : count );
}
