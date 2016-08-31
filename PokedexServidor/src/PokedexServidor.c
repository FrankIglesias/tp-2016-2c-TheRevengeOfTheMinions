/*
 ============================================================================
 Name        : PokedexServidor.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <commons/bitarray.h>
#include <tiposDato.h>
#include <sw_sockets.h>
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

/*void ocuparBitMap(int bloquesAOcupar){
 int x;
 for(x=0;x<=bloquesAOcupar;x++){
 bitmap[x]="1";
 }
 }*/

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
//	ocuparBitMap(1 + header.cantBloquesBitmap + 104 + tablaAsignacion);
}

void inicializarTablaAsignaciones(void) {
	int bloquesTablaAsignacion = tamanioTablaAsignacion();
	tablaDeAsignaciones = malloc(sizeof(int) * bloquesTablaAsignacion);
	tablaDeAsignaciones[0] = fileHeader.inicioTablaAsignaciones;
}
void atenderPeticiones(int socket, header unHeader, char * ruta) {

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
int buscarEstructura(char ** path) {
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
	int numeroDeArchivo = buscarEstructura(ruta);
	return tablaDeArchivos[numeroDeArchivo].bloqueInicial;
}

char * contenidoDelArchivo(char * path) {
	osadaFile bloque = tablaDeArchivos[obtenerBloqueInicial(path)];
	char * archivo = malloc(bloque.tamanioArchivo);
	int bloqueSiguiente = bloque.bloqueInicial;
	int contador = 0;
	do {
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
	} while (bloqueSiguiente != -1);
	return archivo;
}

int main(void) {

	return EXIT_SUCCESS;
}
