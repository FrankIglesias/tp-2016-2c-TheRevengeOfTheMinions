#!/bin/bash

if [ "$#" -ne 1 ] || [ ! -d $1 ] ; then
	echo "Uso: $0 [Punto de Montaje de OSADA]"
	echo 
	exit 33
fi

OSADADIR=$1
DIRTREE="dir1/dir2/dir3"
NEWDIRTREE="mydir/dir2/dir3"

FILE="file-40900.txt"
NEWFILE="file-1000.txt"

echo "Usando $OSADADIR..."

mkdir -p $OSADADIR/$DIRTREE
truncate -s 40900 $OSADADIR/$DIRTREE/$FILE
truncate -s 1000 $OSADADIR/$DIRTREE/$FILE
mv $OSADADIR/$DIRTREE/$FILE $OSADADIR/$DIRTREE/$NEWFILE
mv $OSADADIR/dir1 $OSADADIR/mydir
stat $OSADADIR/$NEWDIRTREE/$NEWFILE

