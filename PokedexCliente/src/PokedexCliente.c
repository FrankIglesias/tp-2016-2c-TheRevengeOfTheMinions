#include <stddef.h>
#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <tiposDato.h>
#include <sw_sockets.c>
#include <log.h>

t_log * log;
char ipPokedexCliente;
int puertoPokedexCliente;
//int socket; //Lo creo aca asi lo tengo global

char * ip = "127.0.0.1";
int puerto = 9000;

#define _FILE_OFFSET_BITS   64
#define FUSE_USE_VERSION    26

#define DEFAULT_FILE_CONTENT "KEKEKEKanananan"
#define DEFAULT_FILE_NAME "hello"
#define DEFAULT_FILE_PATH "/" DEFAULT_FILE_NAME

/*
 * Esta es una estructura auxiliar utilizada para almacenar parametros
 * que nosotros le pasemos por linea de comando a la funcion principal
 * de FUSE
 */
struct t_runtime_options {
	char* welcome_msg;
} runtime_options;

/*
 * Esta Macro sirve para definir nuestros propios parametros que queremos que
 * FUSE interprete. Esta va a ser utilizada mas abajo para completar el campos
 * welcome_msg de la variable runtime_options
 */
#define CUSTOM_FUSE_OPT_KEY(t, p, v) { t, offsetof(struct t_runtime_options, p), v }

static int obtenerAtributo(const char *path, struct stat *stbuf) {
	int res = 0;

		memset(stbuf, 0, sizeof(struct stat));

		//Si path es igual a "/" nos estan pidiendo los atributos del punto de montaje

		if (strcmp(path, "/") == 0) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		} else if (strcmp(path, DEFAULT_FILE_PATH) == 0) {
			stbuf->st_mode = S_IFREG | 0444;
			stbuf->st_nlink = 1;
			stbuf->st_size = strlen(DEFAULT_FILE_CONTENT);
		} else {
			res = -ENOENT;
		}
	return res;
}

static int leerDirectorio(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi) {

	(void) offset;
		(void) fi;

		if (strcmp(path, "/") != 0)
			return -ENOENT;

		// "." y ".." son entradas validas, la primera es una referencia al directorio donde estamos parados
		// y la segunda indica el directorio padre
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		filler(buf, DEFAULT_FILE_NAME, NULL, 0);

	return 0;

}

static int abrirArchivo(const char *path, struct fuse_file_info *fi) {

	log_trace(log,"Se abrio el archivo %s",path);

	if (strcmp(path, DEFAULT_FILE_PATH) != 0)
			return -ENOENT;

		if ((fi->flags & 3) != O_RDONLY)
			return -EACCES;

	return 0;
}

static int leerArchivo(const char * path, char *buffer, size_t size,
		off_t offset, struct fuse_file_info *fi) {

	log_trace(log,"Se quiso leer %s path,%s buffer, %d size, %d offset",path,buffer,size,offset);
	/*
		mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
		mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
		mensaje->protocolo=LEER;
		mensaje->path=path;
		mensaje->offset=size;   //El OFFSET es el tamaño de lo que quiero leer
		mensaje->start=offset;  //El START es desde donde voy a leer
		*/

		//enviarMensaje(tipoMensaje,socket,mensaje);

		//RECIBIR EL MENSAJE, FALTA DEFINIR QUE ENVIA SERVIDOR A CLIENTE

	//Esto esta para saber que no esta rompiendo. Sin importar que tenga el archivo, lo que se lee es "jejox"
	char * hola = malloc(50);
	hola = "holaProbando";

	size_t len;
	len = strlen(hola);

	 memcpy(buffer, hola, size);

	 return size;
}

static int borrarArchivo(const char * path) {

	/*
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protocolo=BORRAR;
	mensaje->path=path;
	*/

	//enviarMensaje(tipoMensaje,socket,mensaje);


	return 0;
}
static int crearArchivo(const char * path, mode_t modo, dev_t unNumero) { //Nro que indica crear dispositivo o no o sea directorio

	log_trace(log,"Se quiso crear el archivo %s path",path);
	/*
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protocolo=CREAR;
	mensaje->path=path;
	*/

	//enviarMensaje(tipoMensaje,socket,mensaje);

	return 0;
}
static int crearDirectorio(const char * path, mode_t modo) {

	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protocolo=CREARDIR;
	mensaje->path=path;

	return 0;
}

static int escribirArchivo(const char * path, const char * buffer,
		size_t size, off_t offset, struct fuse_file_info * otracosa) {

	log_trace(log,"Se quiso escribir en %s path,%s buffer, %d size, %d offset",path,buffer,size,offset);
	/*
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protocolo=ESCRIBIR;
	mensaje->path=path;
	mensaje->offset=size;   //El OFFSET es el tamaño de lo que quiero leer
	mensaje->start=offset;  //El START es desde donde voy a leer
	*/

	//enviarMensaje(tipoMensaje,socket,mensaje);

	return size; //La funcion retorna lo que se escribio realmente. Puede que no sea este, eso depende lo que me devuelva el servidor
}

static int borrarDirectorio(const char * path) { //EL DIRECTORIO DEBE ESTAR VACIO PARA BORRARSE

	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protocolo=BORRARDIR;
	mensaje->path=path;

	//enviarMensaje(tipoMensaje,socket,mensaje);

	return 0;
}

static int renombrarArchivo(const char * nombreViejo, const char * nombreNuevo) {

	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protocolo=RENOMBRAR;
	mensaje->path=nombreViejo;
	mensaje->nuevoNombre = nombreNuevo;

	//enviarMensaje(tipoMensaje,socket,mensaje);

	return 0;
}

static int obtenerAtributosArchivos(const char * path, struct stat * stat,
		struct fuse_file_info * info) {
	return 0;
}


static struct fuse_operations operacionesFuse = { .getattr = obtenerAtributo,
		.readdir = leerDirectorio, .open = abrirArchivo, .read = leerArchivo,
		.unlink = borrarArchivo, .mknod = crearArchivo,
		.mkdir = crearDirectorio, .write = escribirArchivo, .rmdir =
				borrarDirectorio, .rename = renombrarArchivo, .fgetattr = obtenerAtributosArchivos, //*1

		//*1 Si el archivo no existía, se llama a tu función create() en lugar de open() - si existía,
		//entonces se llama a open(). Luego de llamar a tu función create(), llaman a tu fgetattr(),
		//aunque todavía no entendí muy bien por qué. Un posible uso es que podrías usarla para modificar
		//la semántica de crear un archivo al que no tenés acceso (la semántica estándar aplica unícamente
		//al modo de acceso de archivo de los open()s subsiguientes). Si el archivo no existía y el flag no
		//estaba seteado, FUSE sólo llama a tu función getattr() (en este caso no llama ni a tu create() ni
		//a tu open()). SACADO DE LA DOCUMENTACION

		//Esto creo que despues se va a borrar. Si ellos nos dan las cosas sin errores, no deberiamos
		//usar CREATE

		/*
		 read-Leer archivos
		 mknod-Crear archivos
		 write-Escribir y modificar archivos			DEBE ESTAR SINCRONIZADA
		 unlink-Borrar archivos						DEBE ESTAR SINCRONIZADA
		 mkdir-Crear directorios y subdirectorios.
		 rmdir-Borrar directorios vacíos				DEBE ESTAR SINCRONIZADA
		 rename-Renombrar archivos 					DEBE ESTAR SINCRONIZADA
		 */

		};

/** keys for FUSE_OPT_ options */
enum {
	KEY_VERSION, KEY_HELP,
};

/*
 * Esta estructura es utilizada para decirle a la biblioteca de FUSE que
 * parametro puede recibir y donde tiene que guardar el valor de estos
 */
static struct fuse_opt fuse_options[] = {
// Este es un parametro definido por nosotros
		CUSTOM_FUSE_OPT_KEY("--welcome-msg %s", welcome_msg, 0),

		// Estos son parametros por defecto que ya tiene FUSE
		FUSE_OPT_KEY("-V", KEY_VERSION),
		FUSE_OPT_KEY("--version", KEY_VERSION),
		FUSE_OPT_KEY("-h", KEY_HELP),
		FUSE_OPT_KEY("--help", KEY_HELP),
		FUSE_OPT_END, };

// Dentro de los argumentos que recibe nuestro programa obligatoriamente
// debe estar el path al directorio donde vamos a montar nuestro FS
int main(int argc, char *argv[]) {
	 log = log_create("Log","Fuse",0,0);
	/*ip = argv[1];
	 puerto = argv[2];*/
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	//Creo el socket cliente
	//socket = crearSocketCliente(ip, puerto);

	// Limpio la estructura que va a contener los parametros
	memset(&runtime_options, 0, sizeof(struct t_runtime_options));

	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1) {
		/** error parsing options */
		perror("Invalid arguments!");
		return EXIT_FAILURE;
	}

	// Si se paso el parametro --welcome-msg
	// el campo welcome_msg deberia tener el
	// valor pasado
	if (runtime_options.welcome_msg != NULL) {
		printf("%s\n", runtime_options.welcome_msg);
	}

	// Esta es la funcion principal de FUSE, es la que se encarga
	// de realizar el montaje, comuniscarse con el kernel, delegar todo
	// en varios threads
	return fuse_main(args.argc, args.argv, &operacionesFuse, NULL);
}
