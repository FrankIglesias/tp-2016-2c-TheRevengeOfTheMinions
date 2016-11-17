#!/bin/bash

if [ "$#" -ne 1 ] || [ ! -d $1 ] ; then
	echo "Uso: $0 [Punto de Montaje de OSADA]"
	echo 
	exit 33
fi

OSADADIR=$1

echo "Usando $OSADADIR..."

echo "MiAbuelitaPlanchaResortes" > 01234567890123456789.dat

