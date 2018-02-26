#include "UcdCompat.h"
#include "DefaultStore.h"
#include "Mib.h"
#include "Api.h"
/*
 * use <Types_Session *)->s_snmp_errno instead
 */
int
UcdCompat_getErrno(void)
{
    return ErrorCode_SUCCESS;
}


void
UcdCompat_setDumpPacket(int x)
{
    DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
               DsBool_DUMP_PACKET, x);
}

int
UcdCompat_getDumpPacket(void)
{
    return DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                  DsBool_DUMP_PACKET);
}

void
UcdCompat_setQuickPrint(int x)
{
    DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
               DsBool_QUICK_PRINT, x);
}

int
UcdCompat_getQuickPrint(void)
{
    return DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                  DsBool_QUICK_PRINT);
}


void
UcdCompat_setSuffixOnly(int x)
{
    DefaultStore_setInt(DsStorage_LIBRARY_ID,
               DsInt_OID_OUTPUT_FORMAT, x);
}

int
UcdCompat_getSuffixOnly(void)
{
    return DefaultStore_getInt(DsStorage_LIBRARY_ID,
                  DsInt_OID_OUTPUT_FORMAT);
}

void
UcdCompat_setFullObjid(int x)
{
    DefaultStore_setInt(DsStorage_LIBRARY_ID, DsInt_OID_OUTPUT_FORMAT,
                                              MIB_OID_OUTPUT_FULL);
}

int
UcdCompat_getFullObjid(void)
{
    return (MIB_OID_OUTPUT_FULL ==
        DefaultStore_getInt(DsStorage_LIBRARY_ID, DsInt_OID_OUTPUT_FORMAT));
}

void
UcdCompat_setRandomAccess(int x)
{
    DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
               DsBool_RANDOM_ACCESS, x);
}

int
UcdCompat_getRandomAccess(void)
{
    return DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                  DsBool_RANDOM_ACCESS);
}

void
UcdCompat_setMibErrors(int err)
{
    DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
               DsBool_MIB_ERRORS, err);
}

void
UcdCompat_setMibWarnings(int warn)
{
    DefaultStore_setInt(DsStorage_LIBRARY_ID,
               DsInt_MIB_WARNINGS, warn);
}

void
UcdCompat_setSaveDescriptions(int save)
{
    DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
               DsBool_SAVE_MIB_DESCRS, save);
}

void
UcdCompat_setMibCommentTerm(int save)
{
    /*
     * 0=strict, 1=EOL terminated
     */
    DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
               DsBool_MIB_COMMENT_TERM, save);
}

void
UcdCompat_setMibParseLabel(int save)
{
    /*
     * 0=strict, 1=underscore OK in label
     */
    DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
               DsBool_MIB_PARSE_LABEL, save);
}

int
UcdCompat_setBoolean(int storeid, int which, int value)
{
  return DefaultStore_setBoolean(storeid, which, value);
}

int
UcdCompat_getBoolean (int storeid, int which)
{
  return DefaultStore_getBoolean(storeid, which);
}

int
UcdCompat_toggleBoolean	(int storeid, int which)
{
  return DefaultStore_toggleBoolean(storeid, which);
}

int
UcdCompat_setInt(int storeid, int which, int value)
{
  return DefaultStore_setInt(storeid, which, value);
}

int
UcdCompat_getInt(int storeid, int which)
{
  return DefaultStore_getInt(storeid, which);
}


int
UcdCompat_setString	(int storeid, int which, const char *value)
{
  return DefaultStore_setString(storeid, which, value);
}

char *
UcdCompat_getString(int storeid, int which)
{
  return DefaultStore_getString(storeid, which);
}

int
UcdCompat_setVoid(int storeid, int which, void *value)
{
  return DefaultStore_setVoid(storeid, which, value);
}

void *
UcdCompat_getVoid(int storeid, int which)
{
  return DefaultStore_getVoid(storeid, which);
}

int
UcdCompat_registerConfig (u_char type, const char *ftype,
             const char *token, int storeid, int which)
{
  return DefaultStore_registerConfig(type, ftype, token, storeid, which);
}

int
UcdCompat_registerPremib (u_char type, const char *ftype,
             const char *token, int storeid, int which)
{
  return DefaultStore_registerPremib(type, ftype, token, storeid, which);
}

void
UcdCompat_shutdown(void)
{
  DefaultStore_shutdown();
}
