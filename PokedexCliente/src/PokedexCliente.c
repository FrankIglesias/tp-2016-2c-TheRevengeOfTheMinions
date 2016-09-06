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
char ipPokedexCliente;
int puertoPokedexCliente;
char * ip = "127.0.0.1";
int puerto = 9000;

#define _FILE_OFFSET_BITS   64
#define FUSE_USE_VERSION    26

#define DEFAULT_FILE_CONTENT "KEKEKEK"
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

///////////////////////////////////////////CONEXIONES/////////////////////////////////////////////////////
// No tenemos juanma hermoso

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * @DESC
 *  Esta función va a ser llamada cuando a la biblioteca de FUSE le llege un pedido
 * para obtener la metadata de un archivo/directorio. Esto puede ser tamaño, tipo,
 * permisos, dueño, etc ...
 *
 * @PARAMETROS
 * 		path - El path es relativo al punto de montaje y es la forma mediante la cual debemos
 * 		       encontrar el archivo o directorio que nos solicitan
 * 		stbuf - Esta esta estructura es la que debemos completar
 *
 * 	@RETURN
 * 		O archivo/directorio fue encontrado. -ENOENT archivo/directorio no encontrado
 */
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

/*
 b * @DESC
 *  Esta función va a ser llamada cuando a la biblioteca de FUSE le llege un pedido
 * para obtener la lista de archivos o directorios que se encuentra dentro de un directorio
 *
 * @PARAMETROS
 * 		path - El path es relativo al punto de montaje y es la forma mediante la cual debemos
 * 		       encontrar el archivo o directorio que nos solicitan
 * 		buf - Este es un buffer donde se colocaran los nombres de los archivos y directorios
 * 		      que esten dentro del directorio indicado por el path
 * 		filler - Este es un puntero a una función, la cual sabe como guardar una cadena dentro
 * 		         del campo buf
 *
 * 	@RETURN
 * 		O directorio fue encontrado. -ENOENT directorio no encontrado
 */
static int leerDirectorio(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi) {
	if (strcmp(path, "/") != 0)
		return -ENOENT;
	int sockets = crearSocketCliente(ip, puerto);
	protocoloPokedex_t protocolo;
	//enviarMensaje(sockets,path);
	// Recibe, protocolo, tamaño del buffer, y contenido
	if (!recv(sockets, &protocolo, sizeof(protocoloPokedex_t), 0))
		return -ENOENT; // Esto es para error?
	//if(protocolo == ERROR)
	//return -ENOENT;
	return 0;

}

/*
 * @DESC
 *  Esta función va a ser llamada cuando a la biblioteca de FUSE le llege un pedido
 * para tratar de abrir un archivo
 *
 * @PARAMETROS
 * 		path - El path es relativo al punto de montaje y es la forma mediante la cual debemos
 * 		       encontrar el archivo o directorio que nos solicitan
 * 		fi - es una estructura que contiene la metadata del archivo indicado en el path
 *
 * 	@RETURN
 * 		O archivo fue encontrado. -EACCES archivo no es accesible
 */
static int abrirArchivo(const char *path, struct fuse_file_info *fi) {
	if (strcmp(path, DEFAULT_FILE_PATH) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

/*
 * @DESC
 *  Esta función va a ser llamada cuando a la biblioteca de FUSE le llege un pedido
 * para obtener el contenido de un archivo
 *
 * @PARAMETROS
 * 		path - El path es relativo al punto de montaje y es la forma mediante la cual debemos
 * 		       encontrar el archivo o directorio que nos solicitan
 * 		buf - Este es el buffer donde se va a guardar el contenido solicitado
 * 		size - Nos indica cuanto tenemos que leer
 * 		offset - A partir de que posicion del archivo tenemos que leer
 *
 * 	@RETURN
 * 		Si se usa el parametro direct_io los valores de retorno son 0 si  elarchivo fue encontrado
 * 		o -ENOENT si ocurrio un error. Si el parametro direct_io no esta presente se retorna
 * 		la cantidad de bytes leidos o -ENOENT si ocurrio un error. ( Este comportamiento es igual
 * 		para la funcion write )
 */
static int leerArchivo(const char * path, char *buffer, size_t size,
		off_t offset, struct fuse_file_info *fi) {
	int sockets = crearSocketCliente(ip, puerto);
	protocoloPokedex_t protocolo;
	//EnviarMensaje(sockets,path,tamano,offset)
	recv(sockets,&protocolo,size,0);
	recv(sockets,buffer,size,0);

	if (strcmp(path, DEFAULT_FILE_PATH) != 0)
		return -ENOENT;

	/*size_t len;
	len = strlen(buffer);
	char * hola = malloc(7);
	hola = "jejox";
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buffer, hola, size);
	} else
		size = 0;
		*/
	return size; // ???
}

static int borrarArchivo(const char * path) {
	return 0;
}
static int crearArchivo(const char * path, mode_t modo, dev_t unNumero) { //Nro que indica crear dispositivo o no o sea direcotior
	return 0;
}
static int crearDirectorio(const char * nombreDirectorio, mode_t modo) {
	int sockets = crearSocketCliente(ip, puerto);
	protocoloPokedex_t protocolo;
	//enviarMensaje(sockets,nombreDirectorio);
	if (!recv(sockets, &protocolo, sizeof(protocoloPokedex_t), 0))
		return -ENOENT; // Esto es para error?
	//if(protocolo == ERROR)
	//return -ENOENT;
	return 0;
}

static int escribirArchivo(const char * descriptor, const char * buffer,
		size_t tamano, off_t offset, struct fuse_file_info * otracosa) {
	return 0;
}

static int borrarDirectorio(const char * nombreDirectorio) { //EL DIRECTORIO DEBE ESTAR VACIO PARA BORRARSE
	return 0;
}

static int renombrarArchivo(const char * nombreViejo, const char * nombreNuevo) {
	return 0;
}

static int crearArchivoSiNoExiste(const char * path, mode_t modo,
		struct fuse_file_info * info) {
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
				borrarDirectorio, .rename = renombrarArchivo, .create =
				crearArchivoSiNoExiste,   //*1
		.fgetattr = obtenerAtributosArchivos, //*1

		//*1 Si el archivo no existía, se llama a tu función create() en lugar de open() - si existía,
		//entonces se llama a open(). Luego de llamar a tu función create(), llaman a tu fgetattr(),
		//aunque todavía no entendí muy bien por qué. Un posible uso es que podrías usarla para modificar
		//la semántica de crear un archivo al que no tenés acceso (la semántica estándar aplica unícamente
		//al modo de acceso de archivo de los open()s subsiguientes). Si el archivo no existía y el flag no
		//estaba seteado, FUSE sólo llama a tu función getattr() (en este caso no llama ni a tu create() ni
		//a tu open()). SACADO DE LA DOCUMENTACION

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
	/*ip = argv[1];
	 puerto = argv[2];*/
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

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
