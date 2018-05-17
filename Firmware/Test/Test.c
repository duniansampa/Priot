#include "Test.h"
#include "System/String.h"
#include "System/String.h"

void printResult( const char* testName, bool ok )
{
    if ( ok == true ) {
        printf( "--------[ %s ]--------------------->[ PASS ]\n", testName );
    } else {
        printf( "--------[ %s ]--------------------->[ FAIL ]\n", testName );
    }
}

void Test_String()
{
    printf( "-----[ String ]----- \n\n" );

    if ( 1 ) { /** String_split */
        Map* map = String_split( "Internet of Things (IoT)", " " );
        Map* tmp = map;
        char* expected[] = { "Internet", "of", "Things", "(IoT)" };
        bool ok = false;
        int count = 0;
        while ( tmp ) {
            if ( String_equals( tmp->key, String_setInt32( count ) ) && String_equals( tmp->value, expected[ count ] ) )
                ok = true;
            else {
                ok = false;
                break;
            }
            tmp = tmp->next;
            count++;
        }
        printResult( "String_split", ok );

        Map_clear( map );
    }
    if ( 1 ) { /** String_appendLittle **/
        const char* expected = "Internet of Things";
        bool ok = false;
        char destination[ 200 ];
        String_fill( destination, 0, 1 );
        String_appendLittle( destination, "Internet", 8 );
        String_appendLittle( destination, " of Things", 10 );

        if ( String_equals( destination, expected ) )
            ok = true;

        printResult( "String_appendLittle", ok );
    }
    if ( 1 ) { /** String_appendTruncate */
        const char* expected = "Internet of Things";
        bool ok = false;
        char buff[ 19 ];
        String_fill( buff, 0, 1 );
        strcat( buff, "Internet " );
        String_appendTruncate( buff, "of Things - IoT", 19 );
        ok = String_equals( buff, expected );
        printResult( "String_appendTruncate", ok );
    }
    if ( 1 ) { /** String_copyTruncate */

        const char* expected = "Internet of Things";
        bool ok = false;
        char buff[ 19 ];
        String_copyTruncate( buff, "Internet of Things - IoT", 19 );
        ok = String_equals( buff, expected );
        printResult( "String_copyTruncate", ok );
    }
}

int main()
{
    printf( "-----[ Start Test ]----- \n\n" );

    Test_String();

    printf( "\n-----[ End Test ]----- \n\n" );

    return 0;
}
