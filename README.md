# tp-2016-2c-TheRevengeOfTheMinions


export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/librerias-sw/Debug

sudo apt-get install libncurses5-dev

sudo apt-get install apt-file

apt-file update

EN PATH AND SYMBOLS, EN LA PESTAÑA DE LIBRARIES AGREGAR ncurses (arregla cosas es magicoou)

Para que no tire error fuse: van al path /usr/include/fuse y ponen sudo chmod 777 fuse.h (para darle permisos para modificar el archivo). Despues, adentro de fuse.h le ponen en donde dice FUSE_USE_VERSION un 26.

VAMOS EQUIPO!
