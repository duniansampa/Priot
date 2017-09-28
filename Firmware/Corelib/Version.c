#include "Version.h"


static const char * version_info = PACKAGE_VERSION;

const char * Version_getVersion(void)
{
    return version_info;
}
