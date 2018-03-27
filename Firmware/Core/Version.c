#include "Version.h"


static const char * _version_info = PACKAGE_VERSION;

const char * Version_getVersion(void)
{
    return _version_info;
}
