#ifndef IOT_TEXTUALCONVENTION_H
#define IOT_TEXTUALCONVENTION_H

#include "Generals.h"

/*
 * TextualConvention.h: Provide some standard #defines for Textual Convention
 * related value information
 */

/** ============================[ Macros ============================ */

/*
 * TrueValue
 */
#define tcTRUE ( 1 )
#define tcFALSE ( 2 )

/*
 * RowStatus
 */
#define tcROW_STATUS_NONEXISTENT ( 0 )
#define tcROW_STATUS_ACTIVE ( 1 )
#define tcROW_STATUS_NOTINSERVICE ( 2 )
#define tcROW_STATUS_NOTREADY ( 3 )
#define tcROW_STATUS_CREATEANDGO ( 4 )
#define tcROW_STATUS_CREATEANDWAIT ( 5 )
#define tcROW_STATUS_DESTROY ( 6 )

#define tcROW_STATUS_IS_GOING_ACTIVE( x ) ( x == tcROW_STATUS_CREATEANDGO || x == tcROW_STATUS_ACTIVE )
#define tcROW_STATUS_IS_ACTIVE( x ) ( x == tcROW_STATUS_ACTIVE )
#define tcROW_STATUS_IS_NOT_ACTIVE( x ) ( !tcROW_STATUS_IS_GOING_ACTIVE( x ) )

/*
 * StorageType
 */
#define tcSTORAGE_TYPE_NONE 0
#define tcSTORAGE_TYPE_OTHER 1
#define tcSTORAGE_TYPE_VOLATILE 2
#define tcSTORAGE_TYPE_NONVOLATILE 3
#define tcSTORAGE_TYPE_PERMANENT 4
#define tcSTORAGE_TYPE_READONLY 5

/** =============================[ Functions Prototypes ]================== */

char TextualConvention_checkRowStatusTransition( int oldValue, int newValue );

char TextualConvention_checkRowStatusWithStorageTypeTransition( int oldValue, int newValue, int oldStorage );

char TextualConvention_checkStorageTransition( int oldValue, int newValue );

#endif // IOT_TEXTUALCONVENTION_H
