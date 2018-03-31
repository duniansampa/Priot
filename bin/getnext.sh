siglogDir=$(realpath "$0")
siglogDir=`dirname $siglogDir`
export LD_LIBRARY_PATH=$siglogDir/lib
./exe/siglogGetNext -v 3 -n "" -u duniansampa -a MD5 -A "minhasenha" -x DES -X "minhachave" -l authPriv localhost system
