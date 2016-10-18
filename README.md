# tp-2016-2c-TheRevengeOfTheMinions

<strong>Comandos Utiles </strong>

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/librerias-sw/Debug
<em>Te linkea con nuestras librerias propias</em>

<strong>Para instalar NCURSES </strong>

sudo apt-get install libncurses5-dev

sudo apt-get install apt-file

apt-file update

<strong>Para pokedex Cliente:</strong>

-Linkearle como librerias "fuse" y "pthreads"
-Cuando lo ejecutas, ejecutalo como ./[nombreDelProyectoEnEclipse] /[rutaDelArchvio] -f  (el -f lo deja "en background)
-DFUSE_USE_VERSION=27
-D_FILE_OFFSET_BITS=64

<strong> Para crear un archivo y formatearlo osada </strong>

truncate --size 100M disco.bin

./osada-format disco.bin

<strong>Para ver las actualizaciones de un archivo</strong>

<em> tail -f <NOMBRE DEL ARCHIVO></em>


<font size = 8><strong>VAMOS EQUIPO!</strong></font>

Para correr el cliente de fuse con el mock:
1)Correr el mock hasta que se bloquee
2)Tirar por consola "sudo umount /home/utnso/puntoMontada"
3)Correr el cliente y lo corres hasta que haga el primer getattr con ./[nombreDelProyectoEnEclipse] /[rutaDelArchvio] -f
TIPS:Cuando debugeas el cliente(fuse) ponele en argumentos "-s /home/utnso/puntoMontada -f"


<strong> Para levantar el pokedex servidor </strong>
./PokedexServidor <puertoServidor> <NOmbreArchivoBin>
Ej: ./PokedexServidor 9999 disco.bin
Consideraciones: El puerto es el que quieran utilizar como servidor, el que ingresa al select.
EL archibo  bin que hay en el repo es disco.bin pero pueden poner el challenge.bin o el basic.bin que nos dieron ellos, o crear uno como indica arriba


