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
#define BLOCK_SIZE 64

#pragma pack(push, 1) // wtf?

typedef enum atributo {
	BORRADO = '\0', ARCHIVO = '\1', DIRECTORIO = '\2',
} osada_state;

typedef enum
	__attribute__((packed)) { // wtf ???
		DELETED = '\0',
	REGULAR = '\1',
	DIRECTORY = '\2',
} osada_file_state;

typedef struct tamaniosOsada {
	int bitmap;
	int tablaDeArchivos;
	int tablaDeAsignaciones;
} tamanios_t;

typedef struct OSADA_HEADER {
	unsigned char identificador[7];
	uint8_t version;
	uint32_t fs_blocks; //en bloques
	uint32_t bitmap_blocks; //en bloques
	uint32_t inicioTablaAsignaciones; //bloque de inicio tabla de asignaciones
	uint32_t data_blocks; // cantidad de bloques asignadas para datos
	char relleno[40];
} osadaHeader;
#pragma pack(pop) // :(

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
tamanios_t tam;
t_log * log;

int tamanioTablaAsignacion() { //devuelve el tamaño en bloques
	int f = fileHeader.fs_blocks;
	int n = fileHeader.bitmap_blocks;
	return ((f - 1 - n - 1024) * 4) / BLOCK_SIZE;
}
void inicializarBitArray(void) {
	bitmap = malloc(fileHeader.fs_blocks); //tantos bits como bloques tenga el FS
	int tablaAsignacion = tamanioTablaAsignacion();
	int calculo = (fileHeader.bitmap_blocks + 1025 + tamanioTablaAsignacion());
	memset(bitmap, 1, calculo);
	memset(bitmap + calculo, 0, fileHeader.fs_blocks - calculo);
}

void atenderPeticiones(int socket, header unHeader, char * ruta) { // es necesario la ruta de montaje?
	recv(socket, &unHeader, sizeof(header), 0);
	switch (unHeader.mensaje) {
	case LEER:
		/*SOlicitar path, tamaño e inicio?
		 * char * contenido = leerArchivo(ruta);
		 * Deberia leer una cantidad de bytes
		 *Deberia indicar desde donde leer?
		 */
		break;
	case CREARDIR:
		/* solicitar path y nombre
		 * crearArchivo(path,nombre)
		 * ok u error;
		 */
		break;
	case GUARDAR:
		/*Solicitar path, contenido e inicio?
		 * guardarArchivo(path,contenido,inicio?)
		 * return ok o error por falta de espacio
		 */
		break;
	default:
		log_trace(log,
				"Falta implementar BORRAR,CREARARCHIVO,BORRARDIR,RENOMBRAR");
	}
}
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
						atenderPeticiones(i, nuevoHeader, ruta);
					}

				}
			}
		}
	}
}

int string_contains(char *path, char letra) {
	int i;
	for (i = 0; path[i] != '\0'; i++) {
		if (path[i] == letra)
			return 1;
	}
	return 0;
}
int buscarNroTArchivos(char ** path) {
	int i;
	int padre = -1;
	int contador = 0;
	while (!string_contains(path[contador], '.')) {
		for (i = 0; i < 1024; i++) {
			if ((tablaDeArchivos[i].estado == 2)
					&& (padre == tablaDeArchivos[i].bloquePadre)) {
				padre = tablaDeArchivos[i].bloquePadre;
				contador++;
				break;
			}
		}
	}
	printf("llegue al directorio felicitenme :D");
	for (i = 0; i < 1024; i++) {
		if ((tablaDeArchivos[i].estado == 1)
				&& (padre == tablaDeArchivos[i].bloquePadre)
				&& (string_equals_ignore_case(tablaDeArchivos[i].nombreArchivo,
						path[contador]))) {
			return i;
		}
	}
	return -1;
}
int obtenerBloqueInicial(char * path) {
	char ** ruta = string_split(path, "/");
	int numeroDeArchivo = buscarNroTArchivos(ruta);
	return tablaDeArchivos[numeroDeArchivo].bloqueInicial;
}
int buscarBloqueLibre() {
	int i;
	for (i = 0; i < fileHeader.fs_blocks; ++i) {
		if (bitmap[i] == 0) {
			bitmap[i] = 1;
			return i;
		}
	}
	return 0;
}
void asignarBloqueLibre(int bloqueAnterior) {

}
int verificarBloqueSiguiente(int actual, int siguiente) {
	if (tablaDeAsignaciones[siguiente] == -1) {
		if (asignarBloqueLibre(actual) == -1)
			return -1;
	}
	actual = siguiente;
	siguiente = tablaDeAsignaciones[siguiente];
	return 0;
}

char * leerArchivo(char * path, int tamano, int offset) {
	archivos_t bloque = tablaDeArchivos[obtenerBloqueInicial(path)];
	char * archivo = malloc(tamano);
	int bloqueSiguiente = bloque.bloqueInicial;
	int contador = 0;

	while (offset > BLOCK_SIZE) { // Muevo al nodo que tenga algo para leer
		if (bloqueSiguiente != -1) {
			log_trace(log, "ERROR");
		}
		bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
		offset -= BLOCK_SIZE;
	}
	if (tamano > BLOCK_SIZE - offset) { // Leo el contenido del primer nodo a partir offset
		memcpy(archivo,
				bloquesDeDatos + (bloqueSiguiente * BLOCK_SIZE) + offset,
				BLOCK_SIZE - offset);
		bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
	} else {
		memcpy(
				archivo, // Si es menor de lo que le queda al nodo
				bloquesDeDatos + (bloqueSiguiente * BLOCK_SIZE) + offset,
				tamano);
		return archivo;
	}
	while (tamano > 0) { // prosigo con la lectura si hay que leer mas
		if (tamano > BLOCK_SIZE) {
			memcpy(archivo + offset + (contador * BLOCK_SIZE),
					bloquesDeDatos + (bloqueSiguiente * BLOCK_SIZE),
					BLOCK_SIZE);
			tamano -= BLOCK_SIZE;
			contador++;
			bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
		} else {
			memcpy(archivo + offset + (contador * BLOCK_SIZE),
					bloquesDeDatos + (bloqueSiguiente * BLOCK_SIZE), tamano);
			tamano = 0;
		}
	}
	return archivo;
}
int guardarArchivo(char * path, char * buffer, int offset) {
	int tam = strlen(buffer);
	int puntero = 0;
	int contador = 0;
	int bloqueActual = tablaDeArchivos[obtenerBloqueInicial(path)].bloqueInicial;
	int bloqueSiguiente;
	while (offset > BLOCK_SIZE) { // Muevo al nodo que tenga algo para escribir
		if (verificarBloqueSiguiente(bloqueActual, bloqueSiguiente) == -1)
			return -1;
		offset -= BLOCK_SIZE;
	}
	if (tam > BLOCK_SIZE - offset) {
		memcpy(bloquesDeDatos[bloqueActual] + offset, buffer,
		BLOCK_SIZE - offset);
		puntero += BLOCK_SIZE - offset;
		if (verificarBloqueSiguiente(bloqueActual, bloqueSiguiente) == -1)
			return -1;

	} else {
		memcpy(bloquesDeDatos[bloqueActual] + offset, buffer, tam);
		return 1;
	}
	while (tam > puntero) {
		if ((tam - puntero) >= BLOCK_SIZE) {
			memcpy(bloquesDeDatos + (BLOCK_SIZE * bloqueActual),
					buffer + puntero,
					BLOCK_SIZE);
			puntero += BLOCK_SIZE;
			if (verificarBloqueSiguiente(bloqueActual, bloqueSiguiente) == -1)
				return -1;
		} else {
			memcpy(bloquesDeDatos + (BLOCK_SIZE * bloqueActual),
					buffer + puntero, tam - puntero);
		}
	}
	return 0;
}
void crearArchivo(char * path, char * nombre) {
	archivos_t nuevoArchivo;
	char ** ruta = string_split(path, "/");
	int i = 0;
	int j = 0;
	int padre = -1;
	while (!ruta[i]) { //busco el nodo Padre
		if (tablaDeArchivos[j].bloquePadre == padre
				&& tablaDeArchivos[j].nombreArchivo == ruta[i]) {
			padre = tablaDeArchivos[j].bloqueInicial;
			i++;
			j = 0;
		} else {
			j++;
		}
	}
	nuevoArchivo.bloqueInicial = buscarBloqueLibre();
	nuevoArchivo.bloquePadre = padre;
	nuevoArchivo.estado = 2;
	nuevoArchivo.fechaUltimaModif = 0; // Emmm fechas ?
	strcpy(nuevoArchivo.nombreArchivo, nombre);
	nuevoArchivo.tamanioArchivo = BLOCK_SIZE;

}
void borrar(void) {
}
;
void crearDir(void) {
}
;
void borrarDir(void) {
}
;
void renombrar(void) {
}
;

void levantarHeader() {
	log_trace(log, "Levantando Header");
	char * contenido = malloc(BLOCK_SIZE);
	fread(&fileHeader, sizeof(osadaHeader), 1, osadaFile);
	fileHeader.identificador[7] = '\0';
	log_trace(log, "Identificador: %s", fileHeader.identificador);
	log_trace(log, "Version:%d", fileHeader.version);
	log_trace(log, "Fs_blocks:%u", fileHeader.fs_blocks);
	log_trace(log, "Bitmap_blocks:%u", fileHeader.bitmap_blocks);
	log_trace(log, "InicioTablaAsignaciones:%u",
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
			log_trace(log, "Nombre de carpeta: %s",
					tablaDeArchivos[i].nombreArchivo);
		}
	}
}
void levantarTablaDeAsignaciones() {
	tablaDeAsignaciones = malloc(sizeof(int) * tamanioTablaAsignacion());
	fread(tablaDeAsignaciones, tamanioTablaAsignacion(), 1, osadaFile);
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
}

int main(int argc, void *argv[]) {
	log = log_create("log", "Osada", 1, 0);
	tam.bitmap = fileHeader.fs_blocks;
	tam.tablaDeArchivos = 1024 * BLOCK_SIZE;
	tam.tablaDeAsignaciones = sizeof(int) * tamanioTablaAsignacion();
	osadaFile =
			fopen(
					"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/PokedexServidor/Debug/disco.bin",
					"rb");
	levantarOsada();
	log_trace(log, "Estoy solito y vacio en el mundo");
	//dump();
	return EXIT_SUCCESS;
}
