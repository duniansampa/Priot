#ifndef ASSERT_H
#define ASSERT_H

#include "Generals.h"
#include "Logger.h"
/*
 * MACROs don't need extern "C"
 */

/*
 *  if asserts weren't requested, just log, unless NETSNMP_NO_DEBUGGING specified
 */
#define ASSERT_FUNC_FMT " %s()\n"
#define ASSERT_FUNC_PARAM __func__


#define Assert_assert(x)                                                \
   do {                                                                 \
      if ( x )                                                          \
         ;                                                              \
      else                                                              \
         Logger_log(LOGGER_PRIORITY_ERR,                                \
                  "netsnmp_assert %s failed %s:%d" ASSERT_FUNC_FMT,     \
                  __STRING(x),__FILE__,__LINE__,                        \
                  ASSERT_FUNC_PARAM);                                   \
   }while(0)

#define Assert_assertOrReturn(x, y)                                     \
     do {                                                               \
          if ( x )                                                      \
             ;                                                          \
          else {                                                        \
             Logger_log(LOGGER_PRIORITY_ERR,                            \
                      "netsnmp_assert %s failed %s:%d" ASSERT_FUNC_FMT, \
                      __STRING(x),__FILE__,__LINE__,                    \
                      ASSERT_FUNC_PARAM);                               \
             return y;                                                  \
          }                                                             \
       }while(0)

#define Assert_assertOrMsgreturn(x, y, z)                               \
    do {                                                                \
      if ( x )                                                          \
         ;                                                              \
      else {                                                            \
         Logger_log(LOGGER_PRIORITY_ERR,                                \
                  "netsnmp_assert %s failed %s:%d" ASSERT_FUNC_FMT,     \
                  __STRING(x),__FILE__,__LINE__,                        \
                  ASSERT_FUNC_PARAM);                                   \
         Logger_log(LOGGER_PRIORITY_ERR, y);                            \
         return z;                                                      \
      }                                                                 \
   }while(0)



#define Assert_staticAssert(x) \
    do { switch(0) { case (x): case 0: ; } } while(0)


/*
 *  EXPERIMENTAL macros. May be removed without warning in future
 * releases. Use at your own risk
 *
 * The series of uppercase letters at or near the end of these macros give
 * an indication of what they do. The letters used are:
 *
 *   L  : log a message
 *   RN : return NULL
 *   RE : return a specific hardcoded error appropriate for the condition
 *   RV : return user specified value
 *
 */
#define Assert_mallocCheckLRN(ptr)           \
    Assert_assertOrReturn( (ptr) != NULL, NULL)

#define Assert_mallocCheckLRE(ptr)           \
    Assert_assertOrReturn( (ptr) != NULL, API_ERR_MALLOC)

#define Assert_mallocCheckLRV(ptr, val)                          \
    Assert_assertOrReturn( (ptr) != NULL, val)

#define Assert_requirePtrLRV( ptr, val ) \
    Assert_assertOrReturn( (ptr) != NULL, val)


#endif // ASSERT_H
