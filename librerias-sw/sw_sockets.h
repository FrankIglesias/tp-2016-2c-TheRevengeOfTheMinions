#ifndef LIBRERIAS_SF_SOCKETS_H_
#define LIBRERIAS_SF_SOCKETS_H_
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "tiposDato.h"
#include <stdint.h>
#include <errno.h>

//int crearSocketEscucha(int puerto);
int crearSocketServidor(char PUERTO[]);
#endif /* LIBRERIAS_SF_SOCKETS_H_ */
