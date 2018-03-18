QT      -= core gui
TEMPLATE = subdirs
CONFIG  -= qt shared
#ensure that the subdirectories are built in the order in which they are specified
CONFIG += ordered

#message("message: $$CONFIG")
unix {

    SUBDIRS += core_proj \
               plugin_proj \
               priot_proj \
               daemon_proj

    core_proj.subdir   = Core
    plugin_proj.subdir = Plugin
    priot_proj.subdir  = Priot
    daemon_proj.subdir = Daemon


    daemon_proj.depends = core_proj plugin_proj priot_proj

}else:win32{
    message("Buildings for Windows")
}else{
   message ("Unknown configuration")
}


