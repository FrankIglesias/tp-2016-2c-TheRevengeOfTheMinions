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

typedef struct OSADA_HEADER {
	char identificador[7];
	uint8_t version;
	uint32_t cantBloquesFS; //en bloques
	uint32_t cantBloquesBitmap; //en bloques
	uint32_t inicioTablaAsignaciones; //bloque de inicio tabla de asignaciones
	uint32_t cantidadDeBloquesDeDatos; // cantidad de bloques asignadas para datos
	char relleno[40];
} osadaHeader;

typedef struct osadaFile {
	uint8_t estado; //0 borrado, 1 ocupado, 2 directorio
	char nombreArchivo[17];
	uint16_t bloquePadre;
	uint32_t tamanioArchivo;
	uint32_t fechaUltimaModif; //como hago fechas?
	uint32_t bloqueInicial;
} osadaFile;

osadaHeader fileHeader;
osadaFile tablaDeArchivos[1024];
char* bitmap;
int * tablaDeAsignaciones;
char * bloquesDeDatos;
int puerto = 10000;
t_log * log;

int tamanioTablaAsignacion(void) { //devuelve el tamaño en bloques
	int f = fileHeader.cantBloquesFS;
	int n = fileHeader.cantBloquesBitmap;
	return ((f - 1 - n - 1024) * 4) / BLOCK_SIZE;
}
void inicializarBitArray(void) {
	bitmap = malloc(fileHeader.cantBloquesFS); //tantos bits como bloques tenga el FS
	int tablaAsignacion = tamanioTablaAsignacion();
	int calculo = (fileHeader.cantBloquesBitmap + 1025
			+ tamanioTablaAsignacion());
	memset(bitmap, 1, calculo);
	memset(bitmap + calculo, 0, fileHeader.cantBloquesFS - calculo);
}

void atenderPeticiones(int socket, header unHeader, char * ruta) { // es necesario la ruta de montaje?
	recv(socket, &unHeader, sizeof(header), 0);
	switch (unHeader) {
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
	for (i = 0; i < fileHeader.cantBloquesFS; ++i) {
		if (bitmap[i] == 0) {
			bitmap[i] = 1;
			return i;
		}
	}
	return 0;
}

char * leerArchivo(char * path, int tamano, int offset) {
	osadaFile bloque = tablaDeArchivos[obtenerBloqueInicial(path)];
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

	/*do {
		if (tablaDeAsignaciones[bloqueSiguiente] == -1) {
			memcpy(archivo + (contador * BLOCK_SIZE),
					bloquesDeDatos + (bloqueSiguiente * BLOCK_SIZE),
					bloque.tamanioArchivo - (contador * BLOCK_SIZE));
		} else {
			memcpy(archivo + (contador * BLOCK_SIZE),
					bloquesDeDatos + (bloqueSiguiente * BLOCK_SIZE),
					BLOCK_SIZE);
		}
		contador++;
		bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
	} while (bloqueSiguiente != -1); */
	return archivo;
}
void guardarArchivo(char * path, char * buffer) {
	int tam = strlen(buffer);
	int contador = 0;
	int bloqueLibre;
	int bloqueActual = tablaDeArchivos[obtenerBloqueInicial(path)].bloqueInicial;
	int bloqueSiguiente;
	while (1) {
		memcpy(bloquesDeDatos + (BLOCK_SIZE * bloqueActual),
				buffer + (contador * BLOCK_SIZE),
				BLOCK_SIZE);
		tam -= BLOCK_SIZE;
		contador++;
		if (tam > 0) {
			bloqueSiguiente = tablaDeAsignaciones[bloqueActual];
			if (bloqueSiguiente == -1) {
				bloqueLibre = buscarBloqueLibre();
				tablaDeAsignaciones[bloqueActual] = bloqueLibre;
				bloqueSiguiente = tablaDeAsignaciones[bloqueActual];
			}
		} else {
			break;
		}
	}
}
void crearArchivo(char * path, char * nombre) {
	osadaFile nuevoArchivo;
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

void levantarHeader(char * buffer) {
	char * contenido = malloc(BLOCK_SIZE);
	memcpy(contenido, buffer, BLOCK_SIZE);
	memcpy(fileHeader.identificador, contenido, 7);
	memcpy(fileHeader.version, contenido + 7, 1);
	memcpy(fileHeader.cantBloquesFS, contenido + 8, 4);
	memcpy(fileHeader.cantBloquesBitmap, contenido + 12, 4);
	memcpy(fileHeader.inicioTablaAsignaciones, contenido + 16, 4);
	memcpy(fileHeader.cantidadDeBloquesDeDatos, contenido + 20, 4);
	memcpy(fileHeader.relleno, contenido + 24, 40);
	free(contenido);
}
void levantarTablaDeArchivos(char * buffer) {
	char * contenido = malloc(1024 * BLOCK_SIZE);
	memcpy(contenido, buffer + BLOCK_SIZE + fileHeader.cantBloquesBitmap,
			1024 * BLOCK_SIZE);
	int i;
	int filaTabla = 0;
	for (i = 0; i < 1024; ++i) {
		filaTabla = i * sizeof(osadaHeader);
		memcpy(tablaDeArchivos[i].estado, contenido + filaTabla, 1);
		memcpy(tablaDeArchivos[i].nombreArchivo, contenido + filaTabla + 1, 17);
		memcpy(tablaDeArchivos[i].bloquePadre, contenido + filaTabla + 18, 2);
		memcpy(tablaDeArchivos[i].tamanioArchivo, contenido + filaTabla + 20,
				4);
		memcpy(tablaDeArchivos[i].fechaUltimaModif, contenido + filaTabla + 24,
				4);
		memcpy(tablaDeArchivos[i].bloqueInicial, contenido + filaTabla + 24, 4);
	}
	free(contenido);
}
void levantarTablaDeAsignaciones(char * buffer) {
	int bloquesTablaAsignacion = tamanioTablaAsignacion();
	tablaDeAsignaciones = malloc(sizeof(int) * bloquesTablaAsignacion);
//tablaDeAsignaciones[0] = fileHeader.inicioTablaAsignaciones;
	memcpy(tablaDeAsignaciones,
			buffer + BLOCK_SIZE + fileHeader.cantBloquesFS + 1024,
			sizeof(int) * bloquesTablaAsignacion);
}
void levantarOsada(char * buffer) {
	levantarHeader(buffer);
	inicializarBitArray();
	memcpy(bitmap, buffer + BLOCK_SIZE, fileHeader.cantBloquesBitmap);
	levantarTablaDeArchivos(buffer);
	levantarTablaDeAsignaciones(buffer);
	bloquesDeDatos = malloc(fileHeader.cantidadDeBloquesDeDatos * BLOCK_SIZE);
	memcpy(bloquesDeDatos,
			buffer + BLOCK_SIZE * 1025 + tamanioTablaAsignacion()
					+ fileHeader.cantBloquesBitmap,
			fileHeader.cantidadDeBloquesDeDatos * BLOCK_SIZE);
}

void dump() {
	log_trace(log,
			"Identificador: %s     B-BitMap:%d    B-CantBLoquesFS:%d     B-CantDatos:%d",
			fileHeader.identificador, fileHeader.cantBloquesBitmap,
			fileHeader.cantBloquesFS, fileHeader.cantidadDeBloquesDeDatos);
	int ocupados = 0;
	int i;
	for (i = 0; i < fileHeader.cantBloquesFS; ++i) {
		if (bitmap[i] == 1)
			ocupados++;
	}
	log_trace(log, "Bitmap: Libres: &d    Ocupados:&d",
			fileHeader.cantBloquesBitmap - ocupados, ocupados);
}

int main(void) {
	log = log_create("log", "Osada", 1, 0);
	return EXIT_SUCCESS;
}
