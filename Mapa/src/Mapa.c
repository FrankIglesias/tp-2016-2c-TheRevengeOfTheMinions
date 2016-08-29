/*
 * Mapa.c

 *
 *  Created on: 27/8/2016
 *      Author: utnso
 */
#include <commons/collections/dictionary.h>
#include <config.h>
#include <log.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sw_sockets.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <tiposDato.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <tad_items.h>
#include <stdlib.h>
#include <curses.h>
#include <commons/collections/list.h>

typedef enum {
	READY, BLOQUEADO, MUERTO, ESPERA
} estado_t;

typedef struct config_t {
	char * nombreDelMapa;
	char * rutaDelMetadata;
	int tiempoDeChequeoDeDeadLock;
	int batalla;
	char * algoritmo;
	int quantum;
	int retardo;
	char ip[10];
	char * puerto;
	t_dictionary * diccionarioDePokeparadas;
	posicionMapa posicionMaxima;
} t_configuracion;
typedef struct entrenadorPokemon_t {
	char simbolo;
	int socket;
	instruccion_t accionARealizar;
	posicionMapa posicion;
	t_dictionary * pokemonesAtrapados;
	int intentos;
	estado_t estado;
} entrenadorPokemon;
typedef int pokenest;

pthread_mutex_t sem_tiempoDeChequeo;
pthread_mutex_t sem_listaDeEntrenadores;
pthread_mutex_t sem_mapas;
t_list * listaDeEntrenadores;
t_list * items;
t_configuracion configuracion;
void cargarPokeNests() {

}
void cargarConfiguracion(void) {
	t_config * config;
	config = config_create("/home/utnso/TP/Mapas/Ciudad Paleta/metadata.txt");
	configuracion.tiempoDeChequeoDeDeadLock = config_get_int_value(config,
			"TiempoChequeoDeadlock");
	configuracion.batalla = config_get_int_value(config, "Batalla");
	configuracion.algoritmo = strdup(
			config_get_string_value(config, "algoritmo"));
	configuracion.quantum = config_get_int_value(config, "quantum");
	configuracion.retardo = config_get_int_value(config, "retardo");
	cargarPokeNests();

	/*
	 IP=127.0.0.1
	 Puerto=5001*/
}

void atenderDeadLock(void) {
	while (1) {
		int aux = configuracion.tiempoDeChequeoDeDeadLock;
		pthread_mutex_lock(&sem_tiempoDeChequeo);
		sleep(aux);
		pthread_mutex_unlock(&sem_tiempoDeChequeo);
		if (configuracion.batalla) {
			// TODO pedir pokemones y hacerlos pelear
		}
	}

}
void atenderClienteEntrenadores(int socket, mensaje_ENTRENADOR_MAPA* mensaje) {
	entrenadorPokemon * unEntrendador;
	mensaje_MAPA_ENTRENADOR mensajeAEnviar;
	if (!mensaje->protocolo == HANDSHAKE) {
		unEntrendador = obtenerEntrenador(socket);
	}
	switch (mensaje->protocolo) {
	case MOVE_DOWN:
	case MOVE_LEFT:
	case MOVE_UP:
	case MOVE_RIGHT:
		unEntrendador->estado = READY;
		unEntrendador->accionARealizar = mensaje->protocolo;
		// sem_post(); TODO
		break;
	case HANDSHAKE:
		nuevoEntrenador(socket, mensaje);
		pthread_mutex_lock(&sem_mapas);
		CrearPersonaje(items, mensaje->id, 1, 1);
		actualizarMapa();
		pthread_mutex_unlock(&sem_mapas);
		mensajeAEnviar.protocolo = POSICION;
		mensajeAEnviar.posicion.posicionx =
				configuracion.posicionMaxima.posicionx;
		mensajeAEnviar.posicion.posiciony =
				configuracion.posicionMaxima.posiciony;
		break;
	case PROXIMAPOKENEST:
		unEntrendador->estado = ESPERA;
		// RESPONDER POSICION DE LA POKENEST TODO
		break;
	case ATRAPAR:
		unEntrendador->estado = BLOQUEADO;
		// sem_post() TODO
		break;
	}
}
void atenderEntrenadores(void) {
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
	socketListen = crearSocketServidor(configuracion.puerto);
	FD_SET(socketListen, &master);
	tamanioMaximoDelFd = socketListen;
	mensaje_ENTRENADOR_MAPA * mensaje;
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
					if ((mensaje = (mensaje_ENTRENADOR_MAPA *) recibirMensaje(i))) {
						printf("Se cayo socket\n");
						close(i);
						FD_CLR(i, &master);
					} else {
						atenderClienteEntrenadores(i, mensaje);
					}
				}
			}
		}
	}
}
void nuevoEntrenador(int socket, mensaje_ENTRENADOR_MAPA * mensajeRecibido) {
	entrenadorPokemon * entrenador = malloc(sizeof(entrenadorPokemon));
	entrenador->socket = socket;
	entrenador->posicion.posicionx = 0;
	entrenador->posicion.posiciony = 0;
	entrenador->pokemonesAtrapados = dictionary_create();
	entrenador->intentos = 0;
	entrenador->estado = ESPERA;
	entrenador->simbolo = mensajeRecibido->id;
	pthread_mutex_lock(&sem_listaDeEntrenadores);
	list_add(listaDeEntrenadores, (void *) entrenador);
//sem_post() TODO
	pthread_mutex_unlock(&sem_listaDeEntrenadores);
}

void planificador(void) {
	while (1) {
		//	sem_wait();

	}
}
int distanciaEntreDosPosiciones(posicionMapa posicion1, posicionMapa posicion2) {
	/*int x = abs(posicion1.posicionx - posicion2.posicionx);
	 int y = abs(posicion1.posiciony - posicion2.posiciony);
	 return sqrt(pow(x, 2) + pow(y, 2)); TODO*/
}
void actualizarMapa() {
	nivel_gui_dibujar(items, configuracion.nombreDelMapa);
}
void iniciarDatos() {
	listaDeEntrenadores = list_create();
	pthread_mutex_init(&sem_listaDeEntrenadores, NULL);
	pthread_mutex_init(&sem_mapas, NULL);
	pthread_mutex_init(&sem_tiempoDeChequeo, NULL);
	items = list_create();
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&configuracion.posicionMaxima.posicionx,
			&configuracion.posicionMaxima.posiciony);

}
int main(int arc, char * argv[]) {
	configuracion.nombreDelMapa = string_duplicate(argv[1]);
	cargarConfiguracion();
	signal(SIGUSR2, cargarConfiguracion);
	iniciarDatos();
	pthread_t hiloAtencionDeEntrenadores;
	pthread_create(hiloAtencionDeEntrenadores, NULL, atenderEntrenadores,
	NULL);
//sem_init(&finalizarUmc, 0, 0);
	pthread_t hiloPlanificador;
	pthread_create(hiloPlanificador, NULL, planificador, NULL);
	pthread_t hiloDeDeteccionDeDeadLock;
	pthread_create(hiloDeDeteccionDeDeadLock, NULL, atenderDeadLock, NULL);

	int x = 1;
	int y = 1;

	int p = configuracion.posicionMaxima.posiciony;
	int q = configuracion.posicionMaxima.posicionx;

	CrearPersonaje(items, '@', p, q);
	CrearPersonaje(items, '#', x, y);
	CrearCaja(items, 'H', 26, 10, 5);
	CrearCaja(items, 'M', 8, 15, 3);
	CrearCaja(items, 'F', 19, 9, 2);
	nivel_gui_dibujar(items, configuracion.nombreDelMapa);
	MoverPersonaje(items, '@', p, q);
	MoverPersonaje(items, '#', x, y);

	if (((p == 19) && (q == 9)) || ((x == 19) && (y == 9))) {
		restarRecurso(items, 'F');
	}

	if (((p == 8) && (q == 15)) || ((x == 8) && (y == 15))) {
		restarRecurso(items, 'M');
	}

	BorrarItem(items, '#');
	BorrarItem(items, '@');

	BorrarItem(items, '1');
	BorrarItem(items, '2');

	BorrarItem(items, 'H');
	BorrarItem(items, 'M');
	BorrarItem(items, 'F');

	nivel_gui_terminar();

	return 0;
}
