#ifndef ROWMERGE_H
#define ROWMERGE_H

#include "AgentHandler.h"

/*
 * This row_merge helper splits a whole bunch of requests into chunks
 * based on the row index that they refer to, and passes all requests
 * for a given row to the lower handlers.
 */

typedef struct RowMergeStatusX_s {
   char                  count;    /* number of requests */
   char                  rows;     /* number of rows (unique indexes) */
   char                  current;  /* current row number */
   char                  reserved; /* reserver for future use */

   RequestInfo  **saved_requests; /* internal use */
   char         *saved_status;    /* internal use */
} RowMergeStatus;

MibHandler*
RowMerge_getRowMergeHandler( int );

int
RowMerge_registerRowMerge( HandlerRegistration* reginfo );

void
RowMerge_initRowMerge( void );

int
RowMerge_statusFirst( HandlerRegistration* reginfo,
                                AgentRequestInfo*  reqinfo );

int
RowMerge_statusLast( HandlerRegistration* reginfo,
                             AgentRequestInfo*    reqinfo );

NodeHandlerFT RowMerge_helperHandler;


#endif // ROWMERGE_H
