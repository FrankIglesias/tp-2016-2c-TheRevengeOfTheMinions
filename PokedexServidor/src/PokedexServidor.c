/*
 ============================================================================
 Averiguar el formato de fechas
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <commons/bitarray.h>
#include <tiposDato.h>
#include <sw_sockets.h>
#include <log.h>
#include <sys/mman.h>
#define BLOCK_SIZE 64

typedef enum
	__attribute__((packed)) { // wtf ???
		BORRADO = '\0',
	ARCHIVO = '\1',
	DIRECTORIO = '\2',
} osada_file_state;
typedef struct OSADA_HEADER {
	unsigned char identificador[7];
	uint8_t version;
	uint32_t fs_blocks; //en bloques
	uint32_t bitmap_blocks; //en bloques
	uint32_t inicioTablaAsignaciones; //bloque de inicio tabla de asignaciones
	uint32_t data_blocks; // cantidad de bloques asignadas para datos
	char relleno[40];
} osadaHeader;
typedef struct osadaFile {
	osada_file_state estado; //0 borrado, 1 ocupado, 2 directorio
	unsigned char nombreArchivo[17];
	uint16_t bloquePadre;
	uint32_t tamanioArchivo;
	uint32_t fechaUltimaModif; //como hago fechas?
	uint32_t bloqueInicial;
} archivos_t;

osadaHeader fileHeader;
archivos_t * tablaDeArchivos;
FILE * osadaFile;
char* bitmap;
int * tablaDeAsignaciones;
char * bloquesDeDatos;
int puerto = 10000;
t_log * log;
void * data;

int tamanioTablaAsignacion() { //devuelve el tamaño en bloques
	int f = fileHeader.fs_blocks;
	int n = fileHeader.bitmap_blocks;
	return ((f - 1 - n - 1024) * 4) / BLOCK_SIZE;
}
void inicializarBitArray() {
	bitmap = malloc(fileHeader.fs_blocks); //tantos bits como bloques tenga el FS
	int tablaAsignacion = tamanioTablaAsignacion();
	int calculo = (fileHeader.bitmap_blocks + 1025 + tamanioTablaAsignacion());
	memset(bitmap, 1, calculo);
	memset(bitmap + calculo, 0, fileHeader.fs_blocks - calculo);
}
int tamanioStructAdministrativa() {
	return 1 + fileHeader.bitmap_blocks + 1024 + tamanioTablaAsignacion();
}

/* void atenderPeticiones(int socket, header unHeader, char * ruta) { // es necesario la ruta de montaje?
 mensaje_CLIENTE_SERVIDOR mensaje;
 while (recv(socket,(void*) mensaje, sizeof(mensaje_CLIENTE_SERVIDOR), 0) > 0){ // no esta echa la serealizacion
 switch (mensaje.protocolo){
 case LEER:
 mensaje.data = leerArchivo(mensaje.path,mensaje.offset,mensaje.start);
 if(mensaje.data == NULL){
 mensaje.protocolo = ERROR;
 enviarMensaje(mensaje,socket(),0);
 break;
 }
 }
 break;
 case CREARDIR:
 break;
 case GUARDAR:
 break;
 default:
 log_trace(log,
 "Falta implementar BORRAR,CREARARCHIVO,BORRARDIR,RENOMBRAR");
 }*/
void atenderClientes(void) {
	fd_set master;
	fd_set read_fds;
	struct sockaddr_in remoteaddr;
	int tamanioMaximoDelFd;
	int socketListen;
	int nuevoSocketAceptado;
	int addrlen;
	int i;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	socketListen = crearSocketServidor(puerto);
	FD_SET(socketListen, &master);
	tamanioMaximoDelFd = socketListen;
	//mensaje_ENTRENADOR_MAPA * mensaje;
	while (1) {
		read_fds = master;
		if (select(tamanioMaximoDelFd + 1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(1);
		}
		for (i = 0; i <= tamanioMaximoDelFd; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == socketListen) {
					addrlen = sizeof(remoteaddr);
					if ((nuevoSocketAceptado = accept(socketListen,
							(struct sockaddr *) &remoteaddr, &addrlen)) == -1) {
						perror("accept");
					} else {
						FD_SET(nuevoSocketAceptado, &master);
						if (nuevoSocketAceptado > tamanioMaximoDelFd) {
							tamanioMaximoDelFd = nuevoSocketAceptado;
						}
					}
				} else {
					header nuevoHeader;
					if (recv(i, &nuevoHeader, sizeof(header), 0)) {
						close(i);
						FD_CLR(i, &master);
					} else {
						char * ruta; // RUTA RECIBIDA POR EL POKEDEX CLIENTE
						//			atenderPeticiones(i, nuevoHeader, ruta);
					}

				}
			}
		}
	}
}

int string_contains(const char *path, char letra) {
	int teta;
	for (teta = 0; path[teta] != '\0'; teta++) {
		if (path[teta] == letra)
			return 1;
	}
	return 0;
}
void borrarPrimeraLetra(char * palabra) { // En caso de que haya un /123 comparando
	int i = 1;
	while (palabra[i]) {
		palabra[i - 1] = palabra[i];
		i++;
	}
}
int compararStrings(char * x, char *y) {
	int i = 0;
	int retorno =1;
	for(i=0;i<strlen(y) && (retorno == 1);y++){
		sleep(1);
		log_trace(log,"%c-%c",x[i],y[i]);
		if (x[i] == y[i]) {
			i++;
		} else {
			log_trace(log,"MOJOJOJO");
			retorno = 0;
			break;
		}
	}
	return retorno;
}

int buscarNroTArchivos(char ** path) { // En caso de archivo
	int i;
	uint16_t padre = -1;
	int contador = 0;
	while (!string_contains(path[contador], '.')) {
		for (i = 0; i < 1024; i++) {
			if ((tablaDeArchivos[i].estado == DIRECTORIO)
					&& (padre == tablaDeArchivos[i].bloquePadre)) {
				log_trace(log,"tabla %s",tablaDeArchivos[i].nombreArchivo);
				log_trace(log,"path %s",path[contador]);
				log_trace(log,"padre: %u",padre);
				sleep(1);
				if (strcmp(tablaDeArchivos[i].nombreArchivo,
						path[contador]) == 0) {
					padre = tablaDeArchivos[i].bloqueInicial;
					contador++;
					break;
				}
			}
		}
	}
	log_trace(log, "log: %u", padre);
	for (i = 0; i < 1024; i++) {
		if (tablaDeArchivos[i].estado == ARCHIVO) {
			if (padre == tablaDeArchivos[i].bloquePadre) {
				if (string_equals_ignore_case(tablaDeArchivos[i].nombreArchivo,
						path[contador])) {
					return i;
				}
			}
		}
	}
	log_error(log, "No encontre el archivo");
	return -1;
}
archivos_t * obtenerArchivo(char * path) {
	char ** ruta = string_split(path, "/");
	int numeroDeArchivo = buscarNroTArchivos(ruta);
	if (numeroDeArchivo == -1)
		log_error(log, "No se encontro el archivo");
	return &tablaDeArchivos[numeroDeArchivo];
}
int buscarBloqueLibre() {
	int i;
	for (i = 0; i < fileHeader.fs_blocks; ++i) {
		if (bitmap[i] == 0) {
			bitmap[i] = 1;
			return i;
		}
	}
	return -1;
}
int asignarBloqueLibre(int *bloqueAnterior) {
	int bloqueLibre = buscarBloqueLibre();
	log_trace(log, "Bloque libre a ocupar: %d", bloqueLibre);
	if (bloqueLibre == -1) {
		return -1;
	}
	tablaDeAsignaciones[*bloqueAnterior] = bloqueLibre;
	return 0;
}
int verificarBloqueSiguiente(int *actual, int *siguiente) {
	if (tablaDeAsignaciones[*siguiente] == 0) {
		log_trace(log, "No hay un bloque siguiente");
		if (asignarBloqueLibre(actual) == -1)
			return -1;
	}
	log_trace(log, "bloque siguiente: %d", *siguiente);
	*actual = tablaDeAsignaciones[*actual];
	*siguiente = tablaDeAsignaciones[*actual];
	return 0;
}
uint16_t buscarAlPadre(char *path) { // Del ultimo directorio sirve Directorios o archivos
	int i = 0;
	int j = 0;
	uint16_t padre = -1;
	char ** ruta = string_split(path, "/");
	while (ruta[i]) {
		if (tablaDeArchivos[j].bloquePadre == padre
				&& (strcmp(tablaDeArchivos[j].nombreArchivo, ruta[i]) == 0)
				&& tablaDeArchivos[j].estado == DIRECTORIO) {
			padre = tablaDeArchivos[j].bloqueInicial;
			i++;
			j = 0;
		} else {
			j++;
		}
	}
	return padre;
}

char * leerArchivo(char * path, int tamano, int offset) {
	log_info(log, "Se va a leer el archivo: %s", path);
	archivos_t * archivo = obtenerArchivo(path);
	if (archivo->tamanioArchivo + offset < tamano) {
		log_error(log, "Se va a leer basura");
		return NULL;
	}
	char * lectura = malloc(tamano);
	int bloqueSiguiente = archivo->bloqueInicial;
	int contador = 0;
	while (offset > BLOCK_SIZE) { // Muevo al nodo que tenga algo para leer
		if (bloqueSiguiente == 0) {
			log_trace(log, "ERROR");
			return NULL;
		}
		bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
		offset -= BLOCK_SIZE;
	}
	if (tamano > BLOCK_SIZE - offset) { // Leo el contenido del primer nodo a partir offset
		memcpy(lectura,
				bloquesDeDatos
						+ (bloqueSiguiente - tamanioStructAdministrativa())
								* BLOCK_SIZE + offset,
				BLOCK_SIZE - offset);
		offset = BLOCK_SIZE - offset;
		bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
	} else {  // Si es menor de lo que le queda al nodo
		memcpy(lectura,
				bloquesDeDatos + (bloqueSiguiente * BLOCK_SIZE) + offset,
				tamano);
		return lectura;
	}
	while (tamano > 0) { // prosigo con la lectura si hay que leer mas
		if (tamano > BLOCK_SIZE) {
			memcpy(lectura + offset + (contador * BLOCK_SIZE),
					bloquesDeDatos
							+ (bloqueSiguiente - tamanioStructAdministrativa())
									* BLOCK_SIZE,
					BLOCK_SIZE);
			tamano -= BLOCK_SIZE;
			contador++;
			bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
		} else {
			memcpy(lectura + offset + (contador * BLOCK_SIZE),
					bloquesDeDatos
							+ (bloqueSiguiente - tamanioStructAdministrativa())
									* BLOCK_SIZE, tamano);
			tamano = 0;
		}
	}
	return lectura;
}
int guardarArchivo(char * path, char * buffer, int offset) {
	log_info(log, "Escribiendo en: %s ,contenido:%s ", path, buffer);
	archivos_t * archivo = malloc(sizeof(archivos_t));
	archivo = obtenerArchivo(path);
	int aux = offset;
	int tam = strlen(buffer);
	int puntero = 0;
	int contador = 0;
	int bloqueActual = archivo->bloqueInicial;
	int bloqueSiguiente = tablaDeAsignaciones[bloqueActual];
	if (bloqueActual == -1) {
		log_error(log, "la cagamo amigo, nada para ver");
		return -1;
	}
	log_trace(log, "BLoque al cual se va a escribir: %d", bloqueActual);

	while (offset > BLOCK_SIZE) { // Muevo al nodo que tenga algo para escribir
		if (verificarBloqueSiguiente(&bloqueActual, &bloqueSiguiente) == -1)
			return -1;
		offset -= BLOCK_SIZE;
	}
	if (tam > BLOCK_SIZE - offset) {
		memcpy(
				bloquesDeDatos
						+ (bloqueActual - tamanioStructAdministrativa()) * 64
						+ offset, buffer,
				BLOCK_SIZE - offset);
		puntero += BLOCK_SIZE - offset;
		if (verificarBloqueSiguiente(&bloqueActual, &bloqueSiguiente) == -1)
			return -1;

	} else {
		memcpy(
				bloquesDeDatos
						+ (bloqueActual - tamanioStructAdministrativa()) * 64
						+ offset, buffer, tam);
		if (archivo->tamanioArchivo < tam + aux)
			archivo->tamanioArchivo = tam + aux;
		free(archivo);
		return 0;
	}
	while (tam > puntero) {
		if ((tam - puntero) >= BLOCK_SIZE) {
			memcpy(
					bloquesDeDatos
							+ (bloqueActual - tamanioStructAdministrativa())
									* 64, buffer + puntero,
					BLOCK_SIZE);
			puntero += BLOCK_SIZE;
			if (verificarBloqueSiguiente(&bloqueActual, &bloqueSiguiente) == -1)
				return -1;
		} else {
			memcpy(
					bloquesDeDatos
							+ (bloqueActual - tamanioStructAdministrativa())
									* 64, buffer + puntero, tam - puntero);
			log_trace(log, "Se ha escrito exitosamente bytes: %d", tam);
			if (archivo->tamanioArchivo < tam + aux)
				archivo->tamanioArchivo = aux + tam;
			return 0;
		}
	}
	free(archivo);
	return 0;

}
void crearArchivo(char * path, char * nombre) {
	log_trace(log, "creando archivo  %s", nombre);
	int i = 0;
	uint16_t padre = -1;
	if (path != "/") {
		padre = buscarAlPadre(path); // hay que verificar esto si devuelve que no tiene padre no deberia.
	}
	for (i = 0; i < 1024; ++i) {
		if (tablaDeArchivos[i].estado == BORRADO) {
			tablaDeArchivos[i].bloqueInicial = buscarBloqueLibre();
			tablaDeArchivos[i].bloquePadre = padre;
			tablaDeArchivos[i].estado = ARCHIVO;
			tablaDeArchivos[i].fechaUltimaModif = 0; // Emmm fechas ?
			if (nombre[0] == "/")
				borrarPrimeraLetra(nombre);
			strcpy(tablaDeArchivos[i].nombreArchivo, nombre);
			tablaDeArchivos[i].tamanioArchivo = 1;
			break;
		}
	}
	log_trace(log, "se ha creado un archivo en el bloque: %d, padre: %d",
			tablaDeArchivos[i].bloqueInicial, padre);
}
void borrar(char * path) { // no funca
	log_trace(log, "Borrar archivo  %s", path);
	char ** ruta = string_split(path, "/");
	archivos_t * archivo;
	int i = 0;
	int j = 0;
	uint16_t padre = -1;
	while (j > -1) {
		for (i = 0; i < 1024; i++) {
			log_trace(log, "Padre:%u", padre);
			log_trace(log, "Tabla:%s", tablaDeArchivos[i].nombreArchivo);
			log_trace(log, "Ruta:%s", ruta[j]);
			sleep(1);
			if ((padre == tablaDeArchivos[i].bloquePadre)
					&& (strcmp(tablaDeArchivos[i].nombreArchivo, ruta[j]) == 0)) {
				if (tablaDeArchivos[i].estado == ARCHIVO) {
					archivo = &tablaDeArchivos[i];
					j = -1000;
				}
				padre = tablaDeArchivos[i].bloqueInicial;
				j++;
				break;

			}

		}

	}
	while (archivo->bloqueInicial != 0) {
		j = archivo->bloqueInicial;
		bitmap[j] = 0;
		archivo->bloqueInicial = tablaDeAsignaciones[j];
		tablaDeAsignaciones[j] = 0;
	}
	archivo->bloqueInicial = -1;
	archivo->bloquePadre = -1;
	archivo->estado = BORRADO;
	archivo->tamanioArchivo = 0;
	log_trace(log, "Se ah borrado el archivo: %s", archivo->nombreArchivo);
}

void crearDir(char * path) {
	log_info(log, "creando directorio");
	char ** ruta = string_split(path, "/");
	int i = 0;
	int j = 0;
	uint32_t padre = -1;
	if (path != "/" && (ruta[i + 1])) {
		while (ruta[i + 1]) {
			for (j = 0; j < 1024; ++j) {
				if ((tablaDeArchivos[j].estado == DIRECTORIO)
						&& (strcmp(tablaDeArchivos[j].nombreArchivo,
								ruta[i]) == 0)) {
					padre = tablaDeArchivos[j].bloqueInicial;
					i++;
					j = 1025;
				}

			}
		}
	}
	uint32_t minion;
	for (j = 0; j < 1024; j++) {
		if (tablaDeArchivos[j].estado == BORRADO) {
			tablaDeArchivos[j].bloqueInicial = buscarBloqueLibre();
			tablaDeArchivos[j].bloquePadre = padre;
			minion = tablaDeArchivos[j].bloqueInicial;
			tablaDeArchivos[j].estado = DIRECTORIO;
			tablaDeArchivos[j].fechaUltimaModif = 0; // Emmm fechas ?
			strcpy(tablaDeArchivos[j].nombreArchivo, ruta[i]);
			tablaDeArchivos[j].tamanioArchivo = 1;
			break;
		}
	}
	log_trace(log, "Se ha creado el directorio: %s , padre %u", ruta[i],minion);
}

void borrarDir(void) {
}

void renombrar(void) {
}

void levantarHeader() {
	log_trace(log, "Levantando Header");
	fread(&fileHeader, sizeof(osadaHeader), 1, osadaFile);
	fileHeader.identificador[7] = '\0';
	log_trace(log, "Identificador: %s", fileHeader.identificador);
	log_trace(log, "Version:%d", fileHeader.version);
	log_trace(log, "Fs_blocks:%u", fileHeader.fs_blocks);
	log_trace(log, "Bitmap_blocks:%u", fileHeader.bitmap_blocks);
	log_trace(log, "Tabla_de_archivos :1024");
	log_trace(log, "Inicio de tabla de asignaciones:%u",
			fileHeader.inicioTablaAsignaciones);
	log_trace(log, "Data_blocks:%u", fileHeader.data_blocks);
}
void levantarTablaDeArchivos() {
	tablaDeArchivos = malloc(1024 * BLOCK_SIZE);
	fread(tablaDeArchivos, sizeof(archivos_t),
			(1024 * BLOCK_SIZE) / sizeof(archivos_t), osadaFile);
	int i;
	for (i = 0; i < (1024 * BLOCK_SIZE) / sizeof(archivos_t); ++i) {
		if (tablaDeArchivos[i].estado == 2) {
			//log_trace(log, "Nombre de carpeta: %s",
			//		tablaDeArchivos[i].nombreArchivo);
		}
	}
}
void levantarTablaDeAsignaciones() {
	tablaDeAsignaciones = malloc(sizeof(int) * tamanioTablaAsignacion());
	fread(tablaDeAsignaciones, tamanioTablaAsignacion() * sizeof(int), 1,
			osadaFile);
}
void levantarOsada() {
	log_trace(log, "Levantando osada");
	levantarHeader();
	inicializarBitArray();
	fread(bitmap, fileHeader.bitmap_blocks, 1, osadaFile);
	uint32_t ocupados = 0;
	int i;
	for (i = 0; i < fileHeader.fs_blocks; ++i) {
		if (bitmap[i] == 1)
			ocupados++;
	}
	log_trace(log, "Bitmap: Libres: %u    Ocupados:%u",
			fileHeader.fs_blocks - ocupados, ocupados);
	log_trace(log, "tamaño estructuras administrativas: %d en bloques",
			tamanioStructAdministrativa());
	levantarTablaDeArchivos();
	levantarTablaDeAsignaciones();
	bloquesDeDatos = malloc(fileHeader.data_blocks * BLOCK_SIZE);
	fread(bloquesDeDatos, fileHeader.data_blocks * BLOCK_SIZE, 1, osadaFile);
}

void dump() {
	log_trace(log,
			"Identificador: %s     B-BitMap:%d    B-CantBLoquesFS:%d     B-CantDatos:%d",
			fileHeader.identificador, fileHeader.bitmap_blocks,
			fileHeader.fs_blocks, fileHeader.data_blocks);
	int ocupados = 0;
	int i;
	for (i = 0; i < fileHeader.fs_blocks; ++i) {
		if (bitmap[i] == 1)
			ocupados++;
	}
	log_trace(log, "Bitmap: Libres: &d    Ocupados:&d",
			fileHeader.bitmap_blocks - ocupados, ocupados);
	imprimirArbolDeDirectorios();
}
void imprimirArchivosDe(char* path) {
	log_trace(log, "Archivos de: %s", path);
	archivos_t archivito = tablaDeArchivos[buscarAlPadre(path)];
	int i;
	for (i = 0; i < 1024; ++i) {
		if (tablaDeArchivos[i].estado == ARCHIVO) {
			log_trace(log, "%s", tablaDeArchivos[i].nombreArchivo);
		}
	}
}
void imprimirDirectoriosRecursivo(archivos_t archivo, int nivel, uint16_t padre) {
	int i;
	for (i = 0; i < 1024; i++) {
		if (tablaDeArchivos[i].estado == DIRECTORIO
				&& tablaDeArchivos[i].bloquePadre == padre) {
			if (tablaDeArchivos[i].estado == DIRECTORIO) {
				char * coshita = string_repeat('-', nivel * 5);
				log_debug(log, "%s %s", coshita,
						tablaDeArchivos[i].nombreArchivo);
				imprimirDirectoriosRecursivo(tablaDeArchivos[i], nivel + 1,
						tablaDeArchivos[i].bloqueInicial);
			}
		}
	}
}
void mostrarTablaDeArchivos(archivos_t* tablaDeArchivos, t_log* log) {
	int i;
	for (i = 0; i < 1024; i++) {
		if (tablaDeArchivos[i].estado != BORRADO)
			log_trace(log, "%d .... %s", i, tablaDeArchivos[i].nombreArchivo);
	}
}
void imprimirArbolDeDirectorios() {
	log_trace(log, "Mostrando arbol de directorio");
	int i;
	uint16_t padre = -1;
	for (i = 0; i < 1024; i++) {
		if (tablaDeArchivos[i].estado == DIRECTORIO
				&& tablaDeArchivos[i].bloquePadre == padre) {
			log_debug(log, "%s", tablaDeArchivos[i].nombreArchivo);
			imprimirDirectoriosRecursivo(tablaDeArchivos[i], 1,
					tablaDeArchivos[i].bloqueInicial);
		}
	}
}

void mapearMemoria() {
//data = malloc(fileHeader.fs_blocks * BLOCK_SIZE);
	data = mmap(NULL, fileHeader.fs_blocks * BLOCK_SIZE,
	PROT_WRITE | PROT_READ,
	MAP_SHARED, osadaFile, 0);
	if (data == MAP_FAILED) {
		log_error(log, "error al mapeo de la memoria");
		perror(MAP_FAILED);
	}
	log_trace(log, "memoria mapeada");
}
void sincronizarMemoria() {
	memcpy(data, &fileHeader, BLOCK_SIZE);
	memcpy(data + BLOCK_SIZE, bitmap, fileHeader.bitmap_blocks * BLOCK_SIZE);
	memcpy(data + (1 + fileHeader.bitmap_blocks) * BLOCK_SIZE, tablaDeArchivos,
			1024 * BLOCK_SIZE);
	memcpy(data + (1025 + fileHeader.bitmap_blocks) * BLOCK_SIZE,
			tablaDeAsignaciones, tamanioTablaAsignacion() * BLOCK_SIZE);
	memcpy(
			data
					+ (1025 + tamanioTablaAsignacion()
							+ fileHeader.bitmap_blocks) * BLOCK_SIZE,
			bloquesDeDatos, fileHeader.data_blocks * BLOCK_SIZE);

}

int main(int argc, void *argv[]) {
	log = log_create("log", "Osada", 1, 0);
	osadaFile =
			fopen(
					"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/PokedexServidor/Debug/minidisk.bin",
					"r+");
	levantarOsada();
//	mapearMemoria();
//	sincronizarMemoria();
	// no considero que haya un archivo con el mismo nombre

//	crearArchivo("/", "solito");
//	imprimirArchivosDe("/");

//	char * lectura = malloc(100);
//mostrarTablaDeArchivos();
//	lectura = leerArchivo("/solito.txt", 100, 0);
	crearDir("/yasmila");
	crearDir("/geminis/");
	crearDir("/pollito/");
	crearDir("/yasmila/mira/");
	crearDir("/yasmila/frank/");
	crearDir("/yasmila/mira/santi/");
	crearDir("/yasmila/mira/pepe/");
	crearArchivo("/yasmila/mira/", "solito.txt");
	guardarArchivo("/yasmila/mira/solito.txt",
			"abc def ghi jkl mnñ opq rst uvw xyz 012 345 678 9ab cde fgh ijk lmn ñop qrs tuv wxy z01 234 567 89...",
			0);
	imprimirArchivosDe("yasmila/mira/");
	//imprimirArbolDeDirectorios();
	borrar("/yasmila/mira/solito.txt");
	free(log);
	free(tablaDeArchivos);
	free(tablaDeAsignaciones);
	free(bitmap);
	return 0;
}

