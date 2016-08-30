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

typedef struct OSADA_HEADER {
	char identificador[7];
	char version;
	uint32_t cantBloquesFS; //en bloques
	uint32_t cantBloquesBitmap; //en bloques
	uint32_t inicioTablaAsignaciones; //bloque de inicio
	uint32_t tamanioDatosBloque;
	char relleno[40];
} osadaHeader;

typedef struct osadaFile {
	char estado; //0 borrado, 1 ocupado, 2 directorio
	char nombreArchivo[17];
	short bloquePadre;
	uint32_t tamanioArchivo;
	uint32_t fechaUltimaModif; //como hago fechas?
	uint32_t bloqueInicial;
} osadaFile;

osadaHeader header;
osadaFile tablaDeArchivos[2048];
char* bitmap;
int* tablaDeAsignaciones;
char * bloquesDeDatos;
int puerto = 10000;

/*void ocuparBitMap(int bloquesAOcupar){
 int x;
 for(x=0;x<=bloquesAOcupar;x++){
 bitmap[x]="1";
 }
 }*/

int tamanioTablaAsignacion(void) { //devuelve el tamaño en bloques
	int f = header.cantBloquesFS;
	int n = header.cantBloquesBitmap;
	return ((f - 1 - n - 1024) * 4) / 64;
}

void inicializarBitArray(void) {
	bitmap = malloc(header.cantBloquesFS); //tantos bits como bloques tenga el FS
	int tablaAsignacion = tamanioTablaAsignacion(header);
	int calculo = (header.cantBloquesBitmap + 1025 + tamanioTablaAsignacion());
	memset(bitmap, 1, calculo);
	memset(bitmap + calculo, 0, header.cantBloquesFS - calculo);
//	ocuparBitMap(1 + header.cantBloquesBitmap + 104 + tablaAsignacion);
}

void inicializarTablaAsignaciones(void) {
	int bloquesTablaAsignacion = tamanioTablaAsignacion(header);
	tablaDeAsignaciones = malloc(sizeof(int) * bloquesTablaAsignacion);
	tablaDeAsignaciones[0] = header.inicioTablaAsignaciones;
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
					/*if ((mensaje = (mensaje_ENTRENADOR_MAPA *) recibirMensaje(i))) {
						printf("Se cayo socket\n");
						close(i);
						FD_CLR(i, &master);
					} else {
						atenderClienteEntrenadores(i, mensaje);
					}*/
				}
			}
		}
	}
}

int main(void) {

	return EXIT_SUCCESS;
}
