siglogDir=$(realpath "$0")
siglogDir=`dirname $siglogDir`
export LD_LIBRARY_PATH=$siglogDir/lib
./exe/siglogDaemon  -f -Lo -Dmib_init
