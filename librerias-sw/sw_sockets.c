#include "sw_sockets.h"
#include <stdio.h>
#include <stdlib.h>

void theMinionsRevengeSelect(char puerto[], void (*funcionAceptar(int n)),
		int (*funcionRecibir(int n))) {
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
						funcionAceptar(i);
					}
				} else {
					if (funcionRecibir(i)) {
						close(i);
						FD_CLR(i, &master);
					}
				}
			}
		}
	}
}
int crearSocketServidor(char *puerto) {
	int BACKLOG = 5;
	struct addrinfo hints;
	struct addrinfo* serverInfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	getaddrinfo(NULL, puerto, &hints, &serverInfo);
	int listenningSocket;
	listenningSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype,
			serverInfo->ai_protocol);
	int yes = 1;
	setsockopt(listenningSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	bind(listenningSocket, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo);

	listen(listenningSocket, BACKLOG);
	return listenningSocket;
}
int crearSocketCliente(char ip[], int puerto) {
	int socketCliente;
	struct sockaddr_in servaddr;

	if ((socketCliente = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Problem creando el Socket.");
		exit(2);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(ip);
	servaddr.sin_port = htons(puerto);

	if (connect(socketCliente, (struct sockaddr *) &servaddr, sizeof(servaddr))
			< 0) {
		perror("Problema al intentar la conexión con el Servidor");
		exit(3);
	}
	return socketCliente;
}

void * (*funcionesDesserializadoras[CLIENTE_SERVIDOR + 1])(char * buffer,
		header header) = {
			deserializarMensaje_ENTRENADOR_MAPA,deserializarMensaje_MAPA_ENTRENADOR
};

//FUNCIONES PARA PASE DE CHARS
/*int enviarMensaje(int socketCliente, char* mensaje) {
 int estadoDeEnvio = send(socketCliente, mensaje, strlen(mensaje) + 1, 0);
 if (estadoDeEnvio > 0)
 printf("El mensaje se envio correctamente\n");
 return socketCliente;
 }*/

void recibirEImprimirMensaje(int socketCliente, int tamanioMensaje) {
	char mensajeRecibido[tamanioMensaje];
	recv(socketCliente, (void*) mensajeRecibido, tamanioMensaje, 0);
	printf("%s\n", mensajeRecibido);
}
void *deserializarMensaje_ENTRENADOR_MAPA(char * buffer, header header) {
	mensaje_ENTRENADOR_MAPA * mensaje = malloc(sizeof(mensaje_ENTRENADOR_MAPA));
	memcpy(mensaje, buffer, sizeof(char) + sizeof(instruccion_t));
	if (header.payload > sizeof(char) + sizeof(instruccion_t)) {
		mensaje->nombrePokemon = malloc(
				header.payload - sizeof(char) - sizeof(instruccion_t));
		memcpy(mensaje->nombrePokemon,
				buffer + sizeof(char) + sizeof(instruccion_t),
				header.payload - sizeof(char) - sizeof(instruccion_t));
	}
	return mensaje;
}
void *deserializarMensaje_MAPA_ENTRENADOR(char * buffer, header header) {
	mensaje_MAPA_ENTRENADOR * mensaje = malloc(sizeof(mensaje_MAPA_ENTRENADOR));
	memcpy(mensaje, buffer, sizeof(posicionMapa) + sizeof(instruccion_t));
	if (header.payload > sizeof(posicionMapa) + sizeof(instruccion_t)) {
		mensaje->nombrePokemon = malloc(
				header.payload - sizeof(posicionMapa) - sizeof(instruccion_t));
		memcpy(mensaje->nombrePokemon,
				buffer + sizeof(posicionMapa) + sizeof(instruccion_t),
				header.payload - sizeof(posicionMapa) - sizeof(instruccion_t));
	}
	return mensaje;
}

void * recibirMensaje(int socket) {
	header header;
	void * mensaje;
	if (recv(socket, &header, sizeof(header), 0) <= 0) {
		return NULL;
	}
	char * buffer = malloc(header.payload);
	if (recv(socket, &buffer, header.payload, 0) <= 0) {
		return NULL;
	}
	if (header.mensaje == MAPA_ENTRENADOR || header.mensaje == ENTRENADOR_MAPA)
		mensaje = funcionesDesserializadoras[header.mensaje](buffer, header);

	switch (header.mensaje) {
	case POKEDEX_MAPA:
		break;
	case MAPA_POKEDEX:
		break;
	case POKEDEX_ENTRENADOR:
		break;
	case ENTRENADOR_POKEDEX:
		break;
	case SERVIDOR_CLIENTE:
		break;
	case CLIENTE_SERVIDOR:
		break;
	default:
		printf("\nLA ESTAMOS PIFIANDO MUCHACHOS\n");
		mensaje = NULL;
		break;
	}
	free(buffer);
	return mensaje;
}

void enviarMensaje(mensaje_t mensaje, int socket, void *data) {
	header nuevoHeader;
	char * buffer;
	nuevoHeader.mensaje = mensaje;
	if (mensaje == ENTRENADOR_MAPA) {
		mensaje_ENTRENADOR_MAPA * mensajeAEnviar =
				(mensaje_ENTRENADOR_MAPA *) data;
		int tamanioString = 0;
		if (mensajeAEnviar->protocolo == POKEMON) {
			tamanioString = strlen(mensajeAEnviar->nombrePokemon);
		}
		buffer = malloc(
				sizeof(header) + sizeof(instruccion_t) + 1 + tamanioString);
		nuevoHeader.payload = sizeof(instruccion_t) + 1 + tamanioString;
		memcpy(buffer, &nuevoHeader, sizeof(header));
		memcpy(buffer + sizeof(header), mensajeAEnviar,
				sizeof(instruccion_t) + sizeof(char));
		if (tamanioString > 0) {
			memcpy(
					buffer + sizeof(header) + sizeof(instruccion_t)
							+ sizeof(char), mensajeAEnviar->nombrePokemon,
					tamanioString);
		}

	} else if (mensaje == MAPA_ENTRENADOR) {
		mensaje_MAPA_ENTRENADOR * mensajeAEnviar =
				(mensaje_MAPA_ENTRENADOR *) data;
		int tamanioString = 0;
		if (mensajeAEnviar->protocolo == POKEMON) {
			tamanioString = strlen(mensajeAEnviar->nombrePokemon);
		}
		buffer = malloc(
				sizeof(instruccion_t) + sizeof(posicionMapa) + sizeof(header)
						+ tamanioString);
		nuevoHeader.payload = sizeof(instruccion_t) + sizeof(posicionMapa)
				+ tamanioString;
		memcpy(buffer, &nuevoHeader, sizeof(header));
		memcpy(buffer + sizeof(header), mensajeAEnviar,
				sizeof(instruccion_t) + sizeof(posicionMapa));
		if (tamanioString > 0) {
			memcpy(
					buffer + sizeof(header) + sizeof(instruccion_t)
							+ sizeof(posicionMapa), mensajeAEnviar,
					tamanioString);
		}
	}
	send(socket, buffer, sizeof(header) + nuevoHeader.payload, 0);
	free(buffer);
}
