# tp-2016-2c-TheRevengeOfTheMinions

<strong>Comandos Utiles </strong>

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/librerias-sw/Debug
<em>Te linkea con nuestras librerias propias</em>

<strong>Para instalar NCURSES </strong>

sudo apt-get install libncurses5-dev

sudo apt-get install apt-file

apt-file update

<strong>EN PATH AND SYMBOLS, EN LA PESTAÑA DE LIBRARIES AGREGAR <em>ncurses</em> (arregla cosas es magicoou)</strong>

Para hacer correr el cliente pokedex:
-Linkearle como librerias "fuse" y "pthreads"
-Cuando lo ejecutas, ejecutalo como ./[nombreDelProyectoEnEclipse] /[rutaDelArchvio] -f  (el -f lo deja "en background)


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


