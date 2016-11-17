#!/bin/bash

if [ "$#" -ne 2 ] || [ ! -d $1 ] ; then
	echo "Uso: $0 [Punto de Montaje de OSADA] [Tamanio en KBs]"
	echo 
	exit 33
fi

OSADADIR=$1
KBCOUNT=$2
BIGFILE="bigfile.bin"

echo "Usando $OSADADIR..."

echo "dd if=/dev/urandom of=$OSADADIR/$BIGFILE bs=1024 count=$KBCOUNT"

