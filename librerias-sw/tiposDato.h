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
	LEER, CREAR, ESCRIBIR, BORRAR, CREARDIR, BORRARDIR, RENOMBRAR, ERROR
} protocoloPokedex_t;
typedef enum {
	ENTRENADOR_MAPA,
	MAPA_ENTRENADOR,
	POKEDEX_MAPA,
	MAPA_POKEDEX,
	POKEDEX_ENTRENADOR,
	ENTRENADOR_POKEDEX,
	SERVIDOR_CLIENTE,
	CLIENTE_SERVIDOR
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
	char id; // TANTO ENTRENADOR COMO POKENEST
	char * nombrePokemon;
} mensaje_ENTRENADOR_MAPA;
typedef struct mensaje_MAPA_ENTRENADOR_t {
	instruccion_t protocolo;
	posicionMapa posicion;
	char * nombrePokemon;
} mensaje_MAPA_ENTRENADOR;
typedef struct mensaje_CLIENTE_SERVIDOR_t{
	protocoloPokedex_t protocolo;
	char * path;
	uint32_t start;
	uint32_t offset;
	char * data; //Usado en rename, y para la devolucion del leer.
}mensaje_CLIENTE_SERVIDOR;
#endif /* LIBRERIAS_SF_TIPOSDATO_H_ */
