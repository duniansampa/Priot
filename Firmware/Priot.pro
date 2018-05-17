QT      -= core gui
TEMPLATE = subdirs
CONFIG  -= qt shared
#ensure that the subdirectories are built in the order in which they are specified
CONFIG += ordered

#message("message: $$CONFIG")
unix {

    SUBDIRS += core_proj \
               priot_proj \
               plugin_proj \
               daemon_proj \
               test_proj

    core_proj.subdir   = Core
    priot_proj.subdir  = Priot
    plugin_proj.subdir = Plugin
    daemon_proj.subdir = Daemon
    test_proj.subdir =  Test


    daemon_proj.depends = core_proj  priot_proj plugin_proj

}else:win32{
    message("Buildings for Windows")
}else{
   message ("Unknown configuration")
}


