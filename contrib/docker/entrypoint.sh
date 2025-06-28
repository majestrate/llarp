#!/usr/bin/env bash
export LLARP_DATA_DIR=${LLARP_DATA_DIR:-/data/}
mkdir -p $LLARP_DATA_DIR
mkdir -p $LLARP_DATA_DIR/nodedb/
if [ ! -e /data/bootstrap.signed ] ; then
    cp /var/lib/llarpd/bootstrap.signed /data/bootstrap.signed
fi
/usr/local/bin/llarpd -r /var/lib/llarpd/llarpd.ini

