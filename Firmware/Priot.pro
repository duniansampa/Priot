QT      -= core gui
TEMPLATE = subdirs
CONFIG  -= qt shared
#ensure that the subdirectories are built in the order in which they are specified
CONFIG += ordered

#message("message: $$CONFIG")
unix {

    SUBDIRS += corelib_proj \
               modlib_proj \
               priotlib_proj \
               daemon_proj

    corelib_proj.subdir  = Corelib
    modlib_proj.subdir   = Modlib
    priotlib_proj.subdir = Priotlib
    daemon_proj.subdir   = Daemon


    daemon_proj.depends = baselib_proj

}else:win32{
    message("Buildings for Windows")
}else{
   message ("Unknown configuration")
}

SUBDIRS += \
    PL

