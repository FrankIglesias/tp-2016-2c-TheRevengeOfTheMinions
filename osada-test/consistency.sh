#!/bin/bash

if [ "$#" -ne 1 ] || [ ! -d $1 ] ; then
	echo "Uso: $0 [Punto de Montaje de OSADA]"
	echo 
	exit 33
fi

OSADADIR=$1
OUTDIR="$OSADADIR/dir1/dir2"
FILENAME="pkmnLogo.$$.txt"
TMPPATH="/tmp"
echo "Usando $OSADADIR..."

mkdir -p $OUTDIR

dd if=/dev/urandom of=$TMPPATH/$FILENAME bs=1024 count=1024

cp $TMPPATH/$FILENAME $OUTDIR

md5sum $TMPPATH/$FILENAME $OUTDIR/$FILENAME



