/*
 * tiposDato.c

 *
 *  Created on: 21/9/2015
 *      Author: utnso
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "tiposDato.h"

char * mostrarProtocolo(instruccion_t instruccion) {
	switch (instruccion) {

	case OK:
		return "OK";
	case ATRAPAR:
		return "ATRAPAR";
	case PROXIMAPOKENEST:
		return "PROXIMAPOKENEST";
	case MORIR:
		return "MORIR";
	case POKEMON:
		return "POKEMON";
	case MOVE_UP:
		return "MOVE_UP";
	case MOVE_DOWN:
		return "MOVE_DOWN";
	case MOVE_LEFT:
		return "MOVE_LEFT";
	case MOVE_RIGHT:
		return "MOVE_RIGHT";
	case HANDSHAKE:
		return "HANDSHAKE";
	case POSICION:
		return "POSICION";
	case GUARDAR:
		return "GUARDAR";
	default:
		return "RECIBI MIERDA";
	}
}

