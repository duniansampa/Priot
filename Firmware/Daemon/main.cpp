#include <iostream>


//#include "siglogd.h"
#include "Settings.h"
#include "Generals.h"
#include "Options.h"
#include "Signals.h"

#include "DefaultStore.h"


using namespace std;

int main(int argc, char *argv[])
{


    DefaultStore_setString(DEFAULTSTORE_STORAGE::LIBRARY_ID, DEFAULTSTORE_LIB_STRING::CONTEXT, "ola mundo!");

    char * value = DefaultStore_getString(DEFAULTSTORE_STORAGE::LIBRARY_ID, DEFAULTSTORE_LIB_STRING::CONTEXT);

    cout << "Value: " << value << endl;
    return 0;
}
