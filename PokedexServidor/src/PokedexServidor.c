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
	uint32_t tamanioBloquesFS; //en bloques
	uint32_t tamanioBloquesBitmap; //en bloques
	uint32_t inicioTablaAsignaciones; //bloque de inicio
	uint32_t tamanioDatosBloque;
	char relleno[40];
}osadaHeader;

typedef struct osadaFile{
	char estado; //0 borrado, 1 ocupado, 2 directorio
	char nombreArchivo[17];
	short bloquePadre;
	uint32_t tamanioArchivo;
	uint32_t fechaUltimaModif; //como hago fechas?
	uint32_t bloqueInicial;
}osadaFile;

osadaHeader header;
osadaFile tablaDeArchivos [2048];
char* bitmap;
int* tablaDeAsignaciones;

void ocuparBitMap(int bloquesAOcupar){
	int x;
	for(x=0;x<=bloquesAOcupar;x++){
		bitmap[x]="1";
	}
}

int tamanioTablaAsignacion(osadaHeader header){ //devuelve el tamaño en bloques
	int f = header.tamanioBloquesFS;
	int n = header.tamanioBloquesBitmap;
	return ((f-1-n-1024)*4)/64;
}

void inicializarBitArray(osadaHeader header){
	bitmap = malloc(sizeof(char)*header.tamanioBloquesFS); //tantos bits como bloques tenga el FS
	int tablaAsignacion = tamanioTablaAsignacion(header);
	ocuparBitMap(1 + header.tamanioBloquesBitmap + 1024 + tablaAsignacion);
}

void inicializarTablaAsignaciones(osadaHeader header){
	int bloquesTablaAsignacion = tamanioTablaAsignacion(header);
	tablaDeAsignaciones=malloc (sizeof(int)*bloquesTablaAsignacion);
	tablaDeAsignaciones[0]= header.inicioTablaAsignaciones;
}

int main(void) {

	return EXIT_SUCCESS;
}
