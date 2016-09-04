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
t_log * log;
int pid = 0;
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
typedef struct pokenest_t {
	char * nombreDelPokemon;
	char * tipo;
	posicionMapa posicion;
	int cantidad;
} pokenest;
typedef struct entrenadorPokemon_t {
	char simbolo;
	int socket;
	int pid;
	instruccion_t accionARealizar;
	posicionMapa posicion;
	char* proximoPokemon;
	t_dictionary * pokemonesAtrapados;
	int intentos;
	estado_t estado;
} entrenadorPokemon;

int quantum;
pthread_mutex_t sem_tiempoDeChequeo;
pthread_mutex_t sem_listaDeEntrenadores;
pthread_mutex_t sem_listaDeEntrenadoresBloqueados;
pthread_mutex_t sem_mapas;
pthread_mutex_t sem_config;
t_list * listaDeEntrenadores;
t_list * items;
t_configuracion configuracion;
void cargarPokeNests(t_dictionary * diccionario, char nombre[]) {
	diccionario = dictionary_create();
	t_config * config;
	config =
			config_create(
					string_from_format(
							"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/Mapas/%s/PokeNests/%s/metadata.txt",
							configuracion.nombreDelMapa, nombre));
	pokenest * nuevaPokenest = malloc(sizeof(pokenest));
	nuevaPokenest->cantidad = 1;
	char * string = config_get_string_value(config, "Posicion");
	char ** posiciones = string_split(string, ";");
	nuevaPokenest->posicion.posicionx = atoi(posiciones[0]);
	nuevaPokenest->posicion.posiciony = atoi(posiciones[1]);
	char * letra = strdup(config_get_string_value(config, "Identificador"));
	dictionary_put(diccionario, letra, (void *) nuevaPokenest);
	CrearCaja(items, letra[0], nuevaPokenest->posicion.posicionx,
			nuevaPokenest->posicion.posiciony, nuevaPokenest->cantidad);
	//config_destroy(config);
}
void cargarConfiguracion(void) {
	t_config * config;
	char * rutaDeConfigs =
			string_from_format(
					"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/Mapas/%s/metadata.txt",
					configuracion.nombreDelMapa);
	config = config_create(rutaDeConfigs);
	configuracion.tiempoDeChequeoDeDeadLock = config_get_int_value(config,
			"TiempoChequeoDeadlock");
	configuracion.batalla = config_get_int_value(config, "Batalla");
	configuracion.algoritmo = strdup(
			config_get_string_value(config, "algoritmo"));
	configuracion.quantum = config_get_int_value(config, "quantum");
	configuracion.retardo = config_get_int_value(config, "retardo");
	cargarPokeNests(configuracion.diccionarioDePokeparadas, "Charmandercitos");
	cargarPokeNests(configuracion.diccionarioDePokeparadas, "Pikachu");
	configuracion.puerto = strdup(config_get_string_value(config, "Puerto"));
	/*
	 IP=127.0.0.1*/
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
		//  sem_post(); TODO
		break;
	case HANDSHAKE:
		nuevoEntrenador(socket, mensaje);
		pthread_mutex_lock(&sem_mapas);
		CrearPersonaje(items, mensaje->id, 1, 1);
		actualizarMapa();
		pthread_mutex_unlock(&sem_mapas);

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
	entrenador->pid += pid;
	entrenador->posicion.posicionx = 1;
	entrenador->posicion.posiciony = 1;
	entrenador->pokemonesAtrapados = dictionary_create();
	entrenador->intentos = 0;
	entrenador->estado = ESPERA;
	entrenador->simbolo = mensajeRecibido->id;
	entrenador->proximoPokemon = mensajeRecibido->nombrePokemon;
	pthread_mutex_lock(&sem_mapas);
	CrearPersonaje(items, mensajeRecibido->id, 1, 1);
	actualizarMapa();
	pthread_mutex_unlock(&sem_mapas);
	pthread_mutex_lock(&sem_listaDeEntrenadores);
	list_add(listaDeEntrenadores, (void *) entrenador);
	//sem_post() TODO
	pthread_mutex_unlock(&sem_listaDeEntrenadores);
}
posicionMapa posicionDeUnaPokeNestSegunNombre(char* pokeNest){
	pokenest* pokenestBuscada = (pokenest*) dictionary_get(configuracion.diccionarioDePokeparadas, pokeNest);
	return pokenestBuscada->posicion;
}
int distanciaEntreDosPosiciones(posicionMapa posicion1, posicionMapa posicion2) {
	return abs(posicion1.posicionx - posicion2.posicionx)
			+ abs(posicion1.posiciony - posicion2.posiciony);
}
entrenadorPokemon* planificador(int socket, mensaje_ENTRENADOR_MAPA* mensaje) {
	entrenadorPokemon * entrenador = malloc(sizeof(entrenadorPokemon));
	pthread_mutex_lock(&sem_config);
	quantum = configuracion.quantum;
	int algoritmo = strcmp(configuracion.algoritmo, "RR");
	pthread_mutex_unlock(&sem_config);
	if(algoritmo == 0)
	{
		bool ordenarPorTiempoDeLlegada(void* data, void* data2)
		{
			entrenadorPokemon* entrenador1 = (entrenadorPokemon*) data;
			entrenadorPokemon* entrenador2 = (entrenadorPokemon*) data2;

			if(entrenador1->pid < entrenador2->pid)
				return true;
			return false;
		}
		list_sort(listaDeEntrenadores, ordenarPorTiempoDeLlegada);
		while(quantum != 0)
		{
		entrenador = (entrenadorPokemon*) list_get(listaDeEntrenadores, 0);
		quantum--;
		}
	}
	else
	{
		pthread_mutex_lock(&sem_config);
		algoritmo = strcmp(configuracion.algoritmo, "SRDF");
		pthread_mutex_unlock(&sem_config);
		if(algoritmo == 0)
		{
			bool ordenarPorCercaniaAUnaPokenest(void* data, void* data2)
			{
				entrenadorPokemon* entrenador1 = (entrenadorPokemon*) data;
				entrenadorPokemon* entrenador2 = (entrenadorPokemon*) data2;
				posicionMapa posicionPokemon1 = posicionDeUnaPokeNestSegunNombre(entrenador1->proximoPokemon);
				int distanciaEntrenador1 = distanciaEntreDosPosiciones(entrenador1->posicion, posicionPokemon1);
				posicionMapa posicionPokemon2 = posicionDeUnaPokeNestSegunNombre(entrenador2->proximoPokemon);
				int distanciaEntrenador2 = distanciaEntreDosPosiciones(entrenador2->posicion, posicionPokemon2);
				if(distanciaEntrenador1 > distanciaEntrenador2)
					return true;
				return false;

			}
			while(quantum!=0)
			{
			list_sort(listaDeEntrenadores, ordenarPorCercaniaAUnaPokenest);
			entrenador = (entrenadorPokemon*) list_get(listaDeEntrenadores, 0);
			quantum--;
			}

		}
		}
	return entrenador;
}


void actualizarMapa() {
	nivel_gui_dibujar(items, configuracion.nombreDelMapa);
}
void iniciarDatos() {
	log = log_create("Log", "Mapa", 0, 0);
	listaDeEntrenadores = list_create();
	pthread_mutex_init(&sem_listaDeEntrenadores, NULL);
	pthread_mutex_init(&sem_mapas, NULL);
	pthread_mutex_init(&sem_tiempoDeChequeo, NULL);
	pthread_mutex_init(&sem_config, NULL);
	items = list_create();
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&configuracion.posicionMaxima.posicionx,
			&configuracion.posicionMaxima.posiciony);
	//sem_init(&finalizarUmc, 0, 0);
}
void liberarDatos() {
	/*	BorrarItem(items, '#');
	 BorrarItem(items, '@');

	 BorrarItem(items, '1');
	 BorrarItem(items, '2');

	 BorrarItem(items, 'H');
	 BorrarItem(items, 'M');
	 BorrarItem(items, 'F');
	 */

	nivel_gui_terminar();
}
void crearHiloParaPlanificador() {
	pthread_t hiloPlanificador;
	pthread_create(hiloPlanificador, NULL, planificador, NULL);
}
void crearHiloParaDeadlock() {
	pthread_t hiloDeDeteccionDeDeadLock;
	pthread_create(hiloDeDeteccionDeDeadLock, NULL, atenderDeadLock,
	NULL);
}
void crearHiloAtenderEntrenadores() {
	pthread_t hiloAtencionDeEntrenadores;
	pthread_create(hiloAtencionDeEntrenadores, NULL, atenderEntrenadores,
	NULL);
}
int main(int arc, char * argv[]) {
	configuracion.nombreDelMapa = string_duplicate(argv[1]);
	iniciarDatos();
	cargarConfiguracion();
	signal(SIGUSR2, cargarConfiguracion);
	crearHiloAtenderEntrenadores();

	crearHiloParaPlanificador();
	crearHiloParaDeadlock();
	nivel_gui_dibujar(items, configuracion.nombreDelMapa);
	sleep(60000000);
	/*
	 CrearPersonaje(items, '@', p, q);
	 CrearPersonaje(items, '#', x, y);
	 CrearCaja(items, 'H', 26, 10, 5);
	 CrearCaja(items, 'M', 8, 15, 3);
	 CrearCaja(items, 'F', 19, 9, 2);
	 MoverPersonaje(items, '@', p, q);
	 MoverPersonaje(items, '#', x, y);

	 if (((p == 19) && (q == 9)) || ((x == 19) && (y == 9))) {
	 restarRecurso(items, 'F');
	 }

	 if (((p == 8) && (q == 15)) || ((x == 8) && (y == 15))) {
	 restarRecurso(items, 'M');
	 }
	 */
	//liberarDatos();
	return 0;
}
