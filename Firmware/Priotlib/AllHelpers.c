#include "AllHelpers.h"
#include "PriotSettings.h"
#include "DebugHandler.h"
#include "Serialize.h"
#include "ReadOnly.h"
#include "BulkToNext.h"
#include "RowMerge.h"
#include "StashCache.h"
#include "TableDataset.h"

/** call the initialization sequence for all handlers with init_ routines. */
void AllHelpers_initHelpers(void)
{
    DebugHandler_initDebugHelper();
    Serialize_initSerialize();
    ReadOnly_initReadOnlyHelper();
    BulkToNext_initBulkToNextHelper();
    TableDataset_initTableDataset();
    RowMerge_initRowMerge();
    StashCache_initStashCacheHelper();
}

/** @defgroup utilities utility_handlers
 *  Simplify request processing
 *  A group of handlers intended to simplify certain aspects of processing
 *  a request for a MIB object.  These helpers do not implement any MIB
 *  objects themselves.  Rather they handle specific generic situations,
 *  either returning an error, or passing a (possibly simpler) request
 *  down to lower level handlers.
 *  @ingroup handler
 */

/** @defgroup leaf leaf_handlers
 *  Process individual leaf objects
 *  A group of handlers to implement individual leaf objects and instances
 *  (both scalar objects, and individual objects and instances within a table).
 *  These handlers will typically allow control to be passed down to a lower
 *  level, user-provided handler, but this is (usually) optional.
 *  @ingroup handler
 */
