#include "sw_sockets.h"
#include <stdio.h>
#include <stdlib.h>

void theMinionsRevengeSelect(char * puerto, void (*funcionAceptar)(int socket),
		int (*funcionRecibir)(int socket)) {
	fd_set master;
	fd_set read_fds;
	int fdmax;
	int listener;
	int i;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	listener = crearSocketServidor(puerto);
	FD_SET(listener, &master);
	fdmax = listener;
	for (;;) {
		read_fds = master;
		if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(4);
		}
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == listener) {
					struct sockaddr_storage remoteaddr;
					socklen_t addrlen;
					addrlen = sizeof remoteaddr;
					int newfd = accept(listener,
							(struct sockaddr *) &remoteaddr, &addrlen);
					FD_SET(newfd, &master);
					if (newfd > fdmax) {
						fdmax = newfd;
					}
					funcionAceptar(newfd);
				} else {

					if (funcionRecibir(i) <= 0) {
						close(i);
						FD_CLR(i, &master);
					}
				}
			}
		}
	}
}
int crearSocketServidor(char * puerto) {
	struct addrinfo hints;
	struct addrinfo *serverInfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// No importa si uso IPv4 o IPv6
	hints.ai_flags = AI_PASSIVE;// Asigna el address del localhost: 127.0.0.1
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(NULL, puerto, &hints, &serverInfo); // Notar que le pasamos NULL como IP, ya que le indicamos que use localhost en AI_PASSIVE

	int listenningSocket;
	listenningSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype,
			serverInfo->ai_protocol);
	int yes = 1;
	setsockopt(listenningSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	bind(listenningSocket, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo); // Ya no lo vamos a necesitar

	listen(listenningSocket, 5); // IMPORTANTE: listen() es una syscall BLOQUEANTE.
	return listenningSocket;
}
int crearSocketServidorMonoCliente(char *puerto) {
	int socketEscucha = crearSocketServidor(puerto);
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int socketCliente = accept(socketEscucha, (struct sockaddr *) &addr,
			&addrlen);
	return socketCliente;
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

void *deserializarMensaje_CLIENTE_SERVIDOR_bidireccional(char * buffer,
		header header) {
	mensaje_CLIENTE_SERVIDOR * mensaje = malloc(header.payload);
	memcpy(mensaje, buffer, sizeof(instruccion_t) + sizeof(uint32_t) * 4);
	int puntero = sizeof(instruccion_t) + sizeof(uint32_t) * 4;
	if (mensaje->path_payload > 0) {
		mensaje->path = malloc((mensaje->path_payload) + 1);
		memcpy(mensaje->path, buffer + puntero, mensaje->path_payload);
		mensaje->path[mensaje->path_payload] = '\0';
		puntero += mensaje->path_payload;
	}
	if (mensaje->tamano > 0 && mensaje->protolo != ERROR) {
		mensaje->buffer = malloc(mensaje->tamano + 1);
		memcpy(mensaje->buffer, buffer + puntero, mensaje->tamano);
	}
	free(buffer);
	return mensaje;
}
void *deserializarMensaje_ENTRENADOR_MAPA(char * buffer, header header) {
	mensaje_ENTRENADOR_MAPA * mensaje = malloc(sizeof(mensaje_ENTRENADOR_MAPA));
	memcpy(mensaje, buffer, sizeof(char) + sizeof(instruccion_t));
	int pasaje = sizeof(char) + sizeof(instruccion_t);
	if (header.payload > sizeof(char) + sizeof(instruccion_t)) {
		int tamanioNombre = header.payload - sizeof(char)
				- sizeof(instruccion_t) - sizeof(int);
		mensaje->pokemon.nombreDelFichero = malloc(tamanioNombre);
		memcpy(mensaje->pokemon.nombreDelFichero, buffer + pasaje,
				tamanioNombre);
		pasaje += tamanioNombre;
		memcpy(&mensaje->pokemon.nivel, buffer + pasaje, 4);
	}
	free(buffer);
	return mensaje;
}
void *deserializarMensaje_MAPA_ENTRENADOR(char * buffer, header header) {
	mensaje_MAPA_ENTRENADOR * mensaje = malloc(
			sizeof(posicionMapa) + sizeof(instruccion_t) + sizeof(char));
	memcpy(mensaje, buffer, sizeof(posicionMapa) + sizeof(instruccion_t));
	if (header.payload > sizeof(posicionMapa) + sizeof(instruccion_t)) {
		mensaje->nombrePokemon = malloc(
				header.payload - sizeof(posicionMapa) - sizeof(instruccion_t)
						+ 1);
		memcpy(mensaje->nombrePokemon,
				buffer + sizeof(posicionMapa) + sizeof(instruccion_t),
				header.payload - sizeof(posicionMapa) - sizeof(instruccion_t));
		mensaje->nombrePokemon[header.payload - sizeof(posicionMapa)
				- sizeof(instruccion_t)] = '\0';
	}
	free(buffer);
	return mensaje;
}

char * serializar_CLIENTE_SERVIDOR_bidireccionl(void * data,
		header * nuevoHeader) {
	mensaje_CLIENTE_SERVIDOR * mensaje = (mensaje_CLIENTE_SERVIDOR *) data;
	int tamanioVariable;
	if (mensaje->protolo == SLEERDIR) {
		mensaje->path_payload = 0;
	} else if (mensaje->protolo == SLEER) {
		mensaje->path_payload = 0;
	} else if (mensaje->protolo == LEER) {
		mensaje->path_payload = strlen(mensaje->path);
	} else if (mensaje->protolo == ESCRIBIR) {
		mensaje->path_payload = strlen(mensaje->path);
	} else if (mensaje->protolo == SGETATTR) {
		mensaje->path_payload = strlen(mensaje->path);
	} else if (mensaje->protolo == TRUNCAR) {
		mensaje->path_payload = strlen(mensaje->path);
	} else if (mensaje->protolo != ERROR) {
		mensaje->path_payload = strlen(mensaje->path);
		mensaje->tamano = strlen(mensaje->buffer);
	} else if (mensaje->protolo == RENOMBRAR) {
		mensaje->path_payload = strlen(mensaje->path);
		mensaje->tamano = strlen(mensaje->buffer);
	} else {
		mensaje->path_payload = 0;
		mensaje->tamano = 0;
	}
	nuevoHeader->payload = sizeof(instruccion_t) + sizeof(uint32_t) * 4
			+ mensaje->path_payload + mensaje->tamano;
	char * buffer = malloc(nuevoHeader->payload + sizeof(header));
	int pasaje = 0;
	memcpy(buffer, nuevoHeader, sizeof(header));
	pasaje = sizeof(header);
	memcpy(buffer + pasaje, mensaje,
			sizeof(instruccion_t) + sizeof(uint32_t) * 4);
	pasaje += sizeof(instruccion_t) + sizeof(uint32_t) * 4;
	if (mensaje->path_payload > 0) {
		memcpy(buffer + pasaje, mensaje->path, mensaje->path_payload);
		pasaje += mensaje->path_payload;
	}
	if (mensaje->tamano > 0 && mensaje->protolo != ERROR
			&& mensaje->protolo != SGETATTR) {
		memcpy(buffer + pasaje, mensaje->buffer, mensaje->tamano);
	}
	return buffer;
}
char * serializar_ENTRENADOR_MAPA(void * data, header * nuevoHeader) {
	mensaje_ENTRENADOR_MAPA * mensajeAEnviar = (mensaje_ENTRENADOR_MAPA *) data;
	char *buffer;
	if (mensajeAEnviar->protocolo == POKEMON) {
		buffer = malloc(sizeof(header) + sizeof(instruccion_t) + 1+strlen(mensajeAEnviar->pokemon.nombreDelFichero)+1+4);
		nuevoHeader->payload = sizeof(instruccion_t) + 1+strlen(mensajeAEnviar->pokemon.nombreDelFichero)+1+4;

	} else {
		buffer = malloc(sizeof(header) + sizeof(instruccion_t) + 1);
		nuevoHeader->payload = sizeof(instruccion_t) + 1;
	}
	memcpy(buffer, nuevoHeader, sizeof(header));
	memcpy(buffer + sizeof(header), mensajeAEnviar,
			sizeof(instruccion_t) + sizeof(char));
	int pasaje = sizeof(header)+ sizeof(instruccion_t) + sizeof(char);
	if(mensajeAEnviar->protocolo ==POKEMON){
		memcpy(buffer +pasaje,mensajeAEnviar->pokemon.nombreDelFichero,strlen(mensajeAEnviar->pokemon.nombreDelFichero)+1);
		pasaje +=strlen(mensajeAEnviar->pokemon.nombreDelFichero)+1;
		memcpy(buffer +pasaje,&mensajeAEnviar->pokemon.nivel,4);

	}
	return buffer;
}
char * serializar_MAPA_ENTRENADOR(void * data, header * nuevoHeader) {
	mensaje_MAPA_ENTRENADOR * mensajeAEnviar = (mensaje_MAPA_ENTRENADOR *) data;
	int tamanioString = 0;
	if (mensajeAEnviar->protocolo == POKEMON) {
		tamanioString = strlen(mensajeAEnviar->nombrePokemon);
	}
	char * buffer = malloc(
			sizeof(instruccion_t) + sizeof(posicionMapa) + sizeof(header)
					+ tamanioString);
	nuevoHeader->payload = sizeof(instruccion_t) + sizeof(posicionMapa)
			+ tamanioString;
	memcpy(buffer, nuevoHeader, sizeof(header));
	memcpy(buffer + sizeof(header), mensajeAEnviar,
			sizeof(instruccion_t) + sizeof(posicionMapa));
	if (tamanioString > 0) {
		memcpy(
				buffer + sizeof(header) + sizeof(instruccion_t)
						+ sizeof(posicionMapa), mensajeAEnviar->nombrePokemon,
				tamanioString);
	}
	return buffer;
}

void * (*funcionesDesserializadoras[CLIENTE_SERVIDOR + 1])(char * buffer,
		header header) = {
			deserializarMensaje_ENTRENADOR_MAPA,deserializarMensaje_MAPA_ENTRENADOR,deserializarMensaje_CLIENTE_SERVIDOR_bidireccional,deserializarMensaje_CLIENTE_SERVIDOR_bidireccional
};
void * (*fucionesSerializadoras[CLIENTE_SERVIDOR + 1])(void * data,
		header * nuevoHeader) = {
			serializar_ENTRENADOR_MAPA,serializar_MAPA_ENTRENADOR,serializar_CLIENTE_SERVIDOR_bidireccionl,serializar_CLIENTE_SERVIDOR_bidireccionl
};

int recibir(int socket, char* buffer, int total) {
	int parcial = 0;
	do {
		if ((parcial += recv(socket, buffer + parcial, total - parcial, 0))
				<= 0)
			return -1;
	} while (parcial < total);
	return 1;
}

int enviar(int socket, char* buffer, int total) {
	int parcial = 0;
	do {
		if ((parcial += send(socket, buffer + parcial, total - parcial, 0))
				<= 0)
			return -1;
	} while (parcial < total);
	return 1;
}

void enviarMensaje(mensaje_t mensaje, int socket, void *data) {
	header nuevoHeader;
	nuevoHeader.mensaje = mensaje;
	char * buffer = fucionesSerializadoras[mensaje](data, &nuevoHeader);
	enviar(socket, buffer, sizeof(header) + nuevoHeader.payload);
	free(buffer);
}
void * recibirMensaje(int socket) {
	header unheader;
	void * mensaje;
	if (recibir(socket, (void *) &unheader, sizeof(header)) <= 0) {
		return NULL;
	}
	char * buffer = malloc(unheader.payload);
	if (recibir(socket, buffer, unheader.payload) <= 0) {
		return NULL;
	}
	if (unheader.mensaje == MAPA_ENTRENADOR
			|| unheader.mensaje == ENTRENADOR_MAPA
			|| unheader.mensaje == CLIENTE_SERVIDOR)
		mensaje = funcionesDesserializadoras[unheader.mensaje](buffer,
				unheader);
//free(buffer); Esta de mas, ya hace free cuando desserealiza
	return mensaje;
}
