#ifndef MTEOBJECTS_H
#define MTEOBJECTS_H

#include "mteTrigger.h"

    /*
     * Flags relating to the mteObjectsTable
     */
#define MTE_OBJECT_FLAG_WILD    0x01    /* for mteObjectsIDWildcard   */
#define MTE_OBJECT_FLAG_ACTIVE  0x02    /* for mteObjectsEntryStatus  */
#define MTE_OBJECT_FLAG_FIXED   0x04    /* for snmpd.conf persistence */
#define MTE_OBJECT_FLAG_VALID   0x08    /* for row creation/undo      */
#define MTE_OBJECT_FLAG_NEXT    0x10    /* for appending a new row    */

#define MTE_STR1_LEN	32

/*
 * Data structure for an object row
 */
struct mteObject {
    /*
     * Index values 
     */
    char            mteOwner[MTE_STR1_LEN+1];
    char            mteOName[MTE_STR1_LEN+1];
    long            mteOIndex;

    /*
     * Column values
     */
    oid             mteObjectID[ASN01_MAX_OID_LEN];
    size_t          mteObjectID_len;

    long            flags;
};

  /*
   * Container structure for the mteObjectsTable,
   * and routine to create this.
   */
extern Tdata *objects_table_data;
extern void      init_objects_table_data(void);

void          init_mteObjects(void);
void               mteObjects_removeEntry(TdataRow *row);
void               mteObjects_removeEntries(const char *owner,  char *oname);
TdataRow *mteObjects_createEntry(  const char *owner,
					    const char *oname,
                                            int   oindex,  int  flags);
struct mteObject * mteObjects_addOID(const char *owner, const char *oname, int index,
                                     const char *oid_name_buf, int wild );

int  mteObjects_vblist(          VariableList *vblist,
                                 char *owner,   char   *oname,
                                 oid  *suffix,  size_t  sfx_len );
int  mteObjects_internal_vblist(  VariableList *vblist,
                                 char *oname,   struct mteTrigger *trigger,
                                 Types_Session *s);

#endif                          /* MTEOBJECTS_H */
