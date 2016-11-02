#ifndef LIBRERIAS_SF_TIPOSDATO_H_
#define LIBRERIAS_SF_TIPOSDATO_H_
#include <stdlib.h>
#include "collections/list.h"
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

typedef enum {
	OK,
	ATRAPAR,
	PROXIMAPOKENEST,
	MORIR,
	POKEMON,
	MOVE_UP,
	MOVE_DOWN,
	MOVE_LEFT,
	MOVE_RIGHT,
	HANDSHAKE,
	POSICION,
	GUARDAR,
} instruccion_t;
typedef enum {
	LEER, CREAR, ESCRIBIR, BORRAR, CREARDIR, BORRARDIR, RENOMBRAR, ERROR,GETATTR,LEERDIR,SGETATTR,SLEERDIR,SLEER,VACIO
} protocoloPokedex_t;
typedef enum {
	ENTRENADOR_MAPA,
	MAPA_ENTRENADOR,
	CLIENTE_SERVIDOR,
	SERVIDOR_CLIENTE
} mensaje_t;
typedef struct header_t {
	mensaje_t mensaje;
	uint32_t payload;
} header;
typedef struct posicionMapa_t {
	int posicionx;
	int posiciony;
} posicionMapa;
typedef struct mensaje_ENTRENADOR_MAPA_t {
	instruccion_t protocolo;
	char simbolo; // TANTO ENTRENADOR COMO POKENEST
} mensaje_ENTRENADOR_MAPA;
typedef struct mensaje_MAPA_ENTRENADOR_t {
	instruccion_t protocolo;
	posicionMapa posicion;
	char * nombrePokemon;
} mensaje_MAPA_ENTRENADOR;
typedef struct mensaje_CLIENTE_SERVIDOR_t {
	protocoloPokedex_t protolo;
	uint32_t path_payload;
	uint32_t offset;
	uint32_t tamano;
	uint32_t tipoArchivo;	//Usado en getattr para saber de que tipo es (1 archivo-2 directorio-0 nada)
	char * path;
	char * buffer;
} mensaje_CLIENTE_SERVIDOR;

#endif /* LIBRERIAS_SF_TIPOSDATO_H_ */
