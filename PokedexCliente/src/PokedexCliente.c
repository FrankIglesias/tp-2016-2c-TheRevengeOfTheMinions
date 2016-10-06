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
#include <commons/collections/list.h>
#include <log.h>

t_log * log;
char ipPokedexCliente;
int puertoPokedexCliente;
int socketParaServidor;

typedef enum
	__attribute__((packed)) { // wtf ???
		BORRADO = '\0',
	ARCHIVO = '\1',
	DIRECTORIO = '\2',
} osada_file_state;

typedef struct osadaFile {
	osada_file_state estado; //0 borrado, 1 ocupado, 2 directorio
	unsigned char nombreArchivo[17];
	uint16_t bloquePadre;
	uint32_t tamanioArchivo;
	uint32_t fechaUltimaModif; //como hago fechas?
	uint32_t bloqueInicial;
} archivos_t;

char * ip = "127.0.0.1";
int puerto = 6000;

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
	log_trace(log,"Se quiere saber los atributos del path %s",path);

	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));

	//Creo el mensaje
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protolo=GETATTR;
	mensaje->path=path;

	//Envio el mensaje
	enviarMensaje(tipoMensaje,socketParaServidor,(void *) &mensaje);

	//Recibo el mensaje, casteando lo que me devuelve recibirMensaje a una estructura entendible
	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketParaServidor);

	if(respuesta->protolo==ERROR){
		return -ENOENT;
	}

	//Segun el tipo de archivo que sea el path, lleno la estructura stbuf
	if(respuesta->tipoArchivo==2){//Si es un directorio
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}else if (respuesta->tipoArchivo==1){//Si es un archivo (tambien lleno su tamaño)
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size =respuesta->tamano;
	}else{
		res = -ENOENT;
	}

	free(mensaje);
	free(respuesta);
	return res;
}

static int leerDirectorio(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi) {

	log_trace(log,"Se quiere leer el directorio de path %s",path);
	int res=0;
	int i=0;
	archivos_t* nuevoArchivo;

	//Creo el mensaje
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protolo=LEERDIR;
	mensaje->path=path;

	//Envio el mensaje
	enviarMensaje(tipoMensaje,socketParaServidor,(void *) &mensaje);

	//Recibo el mensaje, casteando lo que me devuelve recibirMensaje a una estructura entendible
	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketParaServidor);

	//Casteo el buffer a una lista de archivos. Itero la lista y lleno el buf con el nombre de cada archivo
	t_list * listaArchivos = list_create();
	listaArchivos = (t_list*) respuesta->buffer;

	for(i=0;i<=list_size(listaArchivos);i++){
		nuevoArchivo = (archivos_t*) list_get(listaArchivos, i);
		filler(buf,nuevoArchivo->nombreArchivo, NULL, 0);
	}

	free(mensaje);
	free(respuesta);
	list_destroy(listaArchivos);
	return res;
}

static int abrirArchivo(const char *path, struct fuse_file_info *fi) {

	/*
	log_trace(log,"Se abrio el archivo %s",path);

	if (strcmp(path, DEFAULT_FILE_PATH) != 0)
			return -ENOENT;

		if ((fi->flags & 3) != O_RDONLY)
			return -EACCES;

	*/
	return 0;
}

static int leerArchivo(const char * path, char *buffer, size_t size,
		off_t offset, struct fuse_file_info *fi) {

	log_trace(log,"Se quiere leer el archivo con path %s,un size de %d,desde el offset %d",path,size,offset);

	//Creo el mensaje
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protolo=LEER;
	mensaje->path=path;
	mensaje->offset=offset;
	mensaje->tamano=size;

	//Envio el mensaje
	enviarMensaje(tipoMensaje,socketParaServidor,(void *) &mensaje);

	//Recibo el mensaje, casteando lo que me devuelve recibirMensaje a una estructura entendible
	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketParaServidor);

	//Si no hay ningun error, copio en el buffer lo que me haya respondido el servidor
	if (respuesta->protolo==ERROR){
		return -1;
	}else{
		memcpy(buffer,respuesta->buffer,size);
	}

	int retorno = strlen(respuesta->buffer);
	free(mensaje);
	free(respuesta);
	return retorno; //La funcion debe retornar lo que se lea realmente. Esto lo determina el servidor.

}

static int borrarArchivo(const char * path) {

	//Creo el mensaje
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protolo=BORRAR;
	mensaje->path=path;

	//Envio el mensaje
	enviarMensaje(tipoMensaje,socketParaServidor,(void *) &mensaje);

	//Recibo el mensaje, casteando lo que me devuelve recibirMensaje a una estructura entendible
	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketParaServidor);

	//Si no hubo errores retorno 0, caso contrario -1
	if(respuesta->protolo==ERROR){
		return -1;
	}

	free(mensaje);
	free(respuesta);
	return 0;
}
static int crearArchivo(const char * path, mode_t modo, dev_t unNumero) { //Nro que indica crear dispositivo o no o sea directorio

	log_trace(log,"Se quiere crear el archivo %s",path);

	//Creo el mensaje
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	//TODO Definir como pasarle el path y el nombre del archivo por separado

	//Envio el mensaje
	enviarMensaje(tipoMensaje,socketParaServidor,(void *) &mensaje);

	//Recibo el mensaje, casteando lo que me devuelve recibirMensaje a una estructura entendible
	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketParaServidor);

	//Si no hubo errores retorno 0, caso contrario -1
	if(respuesta->protolo==ERROR){
		return -1;
	}

	free(mensaje);
	free(respuesta);
	return 0;
}
static int crearDirectorio(const char * path, mode_t modo) {

	log_trace(log,"Se quiere crear el directorio %s",path);

	//Creo el mensaje
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protolo=CREARDIR;
	mensaje->path=path;

	//Envio el mensaje
	enviarMensaje(tipoMensaje,socketParaServidor,(void *) &mensaje);

	//Recibo el mensaje, casteando lo que me devuelve recibirMensaje a una estructura entendible
	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketParaServidor);

	//Si no hubo errores retorno 0, caso contrario -1
	if(respuesta->protolo==ERROR){
		return -1;
	}

	free(mensaje);
	free(respuesta);
	return 0;
}

static int escribirArchivo(const char * path, const char * buffer,
		size_t size, off_t offset, struct fuse_file_info * otracosa) {

	log_trace(log,"Se quiere escribir el archivo con path %s,un size de %d,desde el offset %d",path,size,offset);

		//Creo el mensaje
		mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
		mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
		mensaje->protolo=ESCRIBIR;
		mensaje->path=path;
		memcpy(mensaje->buffer,buffer,size); //Se escribe un tamaño [size] del buffer [buffer] recibido
		mensaje->offset=offset;

		//Envio el mensaje
		enviarMensaje(tipoMensaje,socketParaServidor,(void *) &mensaje);

		//Recibo el mensaje, casteando lo que me devuelve recibirMensaje a una estructura entendible
		mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
		respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketParaServidor);

		if (respuesta->protolo==ERROR){
			return -1;
		}

	free(mensaje);
	free(respuesta);
	return size; //La funcion write del servidor devuelve 0 si escribio OK. Asumo que escribio el size completo que le mande, por eso aca devuelvo el size.
}

static int borrarDirectorio(const char * path) { //EL DIRECTORIO DEBE ESTAR VACIO PARA BORRARSE

	//Creo el mensaje
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protolo=BORRARDIR;
	mensaje->path=path;

	//Envio el mensaje
	enviarMensaje(tipoMensaje,socketParaServidor,(void *) &mensaje);

	//Recibo el mensaje, casteando lo que me devuelve recibirMensaje a una estructura entendible
	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketParaServidor);

	//Si no hubo errores retorno 0, caso contrario -1
	if(respuesta->protolo==ERROR){
		return -1;
	}

	free(mensaje);
	free(respuesta);
	return 0;
}

static int renombrarArchivo(const char * nombreViejo, const char * nombreNuevo) {

	//Creo el mensaje
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	mensaje_CLIENTE_SERVIDOR * mensaje=malloc(sizeof(mensaje_CLIENTE_SERVIDOR));
	mensaje->protolo=RENOMBRAR;
	mensaje->path=nombreViejo;
	mensaje->buffer=nombreNuevo;

	//Envio el mensaje
	enviarMensaje(tipoMensaje,socketParaServidor,(void *) &mensaje);

	//Recibo el mensaje, casteando lo que me devuelve recibirMensaje a una estructura entendible
	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketParaServidor);

	//Si no hubo errores retorno 0, caso contrario -1
	if(respuesta->protolo==ERROR){
		return -1;
	}

	free(mensaje);
	free(respuesta);
	return 0;
}

static struct fuse_operations operacionesFuse = { .getattr = obtenerAtributo,
		.readdir = leerDirectorio, .open = abrirArchivo, .read = leerArchivo,
		.unlink = borrarArchivo, .mknod = crearArchivo,
		.mkdir = crearDirectorio, .write = escribirArchivo, .rmdir =
				borrarDirectorio, .rename = renombrarArchivo,
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

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	//Creo el socket cliente
	socketParaServidor = crearSocketCliente(ip, puerto);

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
