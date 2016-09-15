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

typedef struct pokenest_t {
	char * nombreDelPokemon;
	char * tipo;
	posicionMapa posicion;
	int cantidad;
} pokenest;
typedef struct entrenadorPokemon_t {
	char simbolo;
	int socket;
	instruccion_t accionARealizar;
	posicionMapa posicion;
	char* proximoPokemon;
	t_dictionary * pokemonesAtrapados;
} entrenadorPokemon;
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

t_log * log;
int quantum = 0;
char ejecutandoId = NULL;
pthread_mutex_t sem_ejecutandoID;
t_list * listaDeReady;
pthread_mutex_t sem_listaDeReady;
sem_t bloqueados_semaphore;
t_list * listaDeEntrenadoresBloqueados;
pthread_mutex_t sem_listaDeEntrenadoresBloqueados;
t_list * items;
pthread_mutex_t sem_mapas;
t_configuracion configuracion;
pthread_mutex_t sem_config;
sem_t semaphore_listos;
pthread_mutex_t sem_listaDeEntrenadores;
t_list * listaDeEntrenadores;
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
	log_trace(log, "Nombre del mapa: %s", configuracion.nombreDelMapa);
	config = config_create(rutaDeConfigs);
	configuracion.tiempoDeChequeoDeDeadLock = config_get_int_value(config,
			"TiempoChequeoDeadlock");
	configuracion.batalla = config_get_int_value(config, "Batalla");
	configuracion.algoritmo = strdup(
			config_get_string_value(config, "algoritmo"));
	configuracion.quantum = config_get_int_value(config, "quantum");
	configuracion.retardo = config_get_int_value(config, "retardo");
	cargarPokeNests(configuracion.diccionarioDePokeparadas, "Charmandercitos"); // HARCODEADO TODO
	cargarPokeNests(configuracion.diccionarioDePokeparadas, "Pikachu"); // HARCODEADO TODO
	configuracion.puerto = strdup(config_get_string_value(config, "Puerto"));
	log_trace(log, "Algoritmo: %s", configuracion.algoritmo);
	log_trace(log, "Tiempo de checkeo de Deadlock: %d",
			configuracion.tiempoDeChequeoDeDeadLock);
	if (configuracion.batalla)
		log_trace(log, "Algoritmo  de batalla habilitado");
	else
		log_trace(log, "Algoritmo  de batalla deshabilitado");
	log_trace(log, "Quantum: %d", configuracion.quantum);
	log_trace(log, "Retardo de planificacion %d", configuracion.retardo);
	quantum = configuracion.quantum;

}

void atenderDeadLock(void) {
	while (1) {
		sem_wait(&bloqueados_semaphore);
		pthread_mutex_lock(&sem_config);
		int aux = configuracion.tiempoDeChequeoDeDeadLock;
		pthread_mutex_unlock(&sem_config);
		sleep(aux);
		// TODO checkeo de deadlock
		if (configuracion.batalla) {
			//  TODO pedir pokemones y hacerlos pelear
		}
		// Si no esta habilitado el algoritmo de batalla queda frizado
	}
}
posicionMapa posicionDeUnaPokeNestSegunNombre(char* pokeNest) {
	pokenest* pokenestBuscada = (pokenest*) dictionary_get(
			configuracion.diccionarioDePokeparadas, pokeNest);
	return pokenestBuscada->posicion;
}
int distanciaEntreDosPosiciones(posicionMapa posicion1, posicionMapa posicion2) {
	return abs(posicion1.posicionx - posicion2.posicionx)
			+ abs(posicion1.posiciony - posicion2.posiciony);
}
void realizarAccion(entrenadorPokemon * unEntrenador) {
	mensaje_MAPA_ENTRENADOR mensaje;
	switch (unEntrenador->accionARealizar) {
	case MOVE_DOWN:
		unEntrenador->posicion.posiciony--;
		mensaje.protocolo = OK;
		enviarMensaje(MAPA_ENTRENADOR, unEntrenador->socket, (void *) &mensaje);
		break;
	case MOVE_UP:
		unEntrenador->posicion.posiciony++;
		mensaje.protocolo = OK;
		enviarMensaje(MAPA_ENTRENADOR, unEntrenador->socket, (void *) &mensaje);
		break;
	case MOVE_LEFT:
		unEntrenador->posicion.posicionx--;
		mensaje.protocolo = OK;
		enviarMensaje(MAPA_ENTRENADOR, unEntrenador->socket, (void *) &mensaje);
		break;
	case MOVE_RIGHT:
		unEntrenador->posicion.posicionx++;
		mensaje.protocolo = OK;
		enviarMensaje(MAPA_ENTRENADOR, unEntrenador->socket, (void *) &mensaje);

		break;
	case PROXIMAPOKENEST:
		mensaje.posicion = posicionDeUnaPokeNestSegunNombre(
				unEntrenador->proximoPokemon);
		mensaje.protocolo = PROXIMAPOKENEST;
		enviarMensaje(MAPA_ENTRENADOR, unEntrenador->socket, (void *) &mensaje);
		break;
	}
	pthread_mutex_lock(&sem_mapas);
	MoverPersonaje(items, unEntrenador->simbolo,
			unEntrenador->posicion.posicionx, unEntrenador->posicion.posiciony);
	actualizarMapa();
	pthread_mutex_unlock(&sem_mapas);
}
void planificador() {
	sem_wait(&semaphore_listos);
	entrenadorPokemon * entrenador;
	pthread_mutex_lock(&sem_config);
	int algoritmo = strcmp(configuracion.algoritmo, "RR");
	pthread_mutex_unlock(&sem_config);
	pthread_mutex_lock(&sem_listaDeReady);
	bool tieneMismoId(void * data) {
		entrenadorPokemon * unEntrenador = (entrenadorPokemon *) data;
		return unEntrenador->simbolo == ejecutandoId;
	}
	entrenador = (entrenadorPokemon *) list_find(listaDeReady, tieneMismoId);
	if (algoritmo == 0) {
		if (quantum > 0) {
			if (entrenador->accionARealizar == ATRAPAR) {
				pthread_mutex_lock(&sem_listaDeEntrenadoresBloqueados);
				list_add(listaDeEntrenadoresBloqueados, (void *) entrenador);
				pthread_mutex_unlock(&sem_listaDeEntrenadoresBloqueados);
				list_remove_by_condition(listaDeEntrenadores, tieneMismoId);
				entrenador = (entrenadorPokemon *) list_get(listaDeReady, 0);
				quantum = configuracion.quantum - 1;
			} else {
				quantum--;
			}
		} else {
			log_trace(log, "FIN DE QUANTUM ---- ENTRENADOR: %c",
					entrenador->simbolo);
			list_add(listaDeReady, (void *) entrenador);
			list_remove_by_condition(listaDeReady, tieneMismoId);
			entrenador = list_get(listaDeReady, 0);
			pthread_mutex_lock(&sem_config);
			quantum = configuracion.quantum - 1;
			pthread_mutex_unlock(&sem_config);
		}
	} else {
		if (entrenador->accionARealizar == ATRAPAR) {
			pthread_mutex_lock(&sem_listaDeEntrenadoresBloqueados);
			list_add(listaDeReady, (void *) entrenador);
			pthread_mutex_unlock(&sem_listaDeEntrenadoresBloqueados);
			list_remove_by_condition(listaDeReady, tieneMismoId);
			bool ordenarPorCercaniaAUnaPokenest(void* data, void* data2) {
				entrenadorPokemon* entrenador1 = (entrenadorPokemon*) data;
				entrenadorPokemon* entrenador2 = (entrenadorPokemon*) data2;
				posicionMapa posicionPokemon1 =
						posicionDeUnaPokeNestSegunNombre(
								entrenador1->proximoPokemon);
				int distanciaEntrenador1 = distanciaEntreDosPosiciones(
						entrenador1->posicion, posicionPokemon1);
				posicionMapa posicionPokemon2 =
						posicionDeUnaPokeNestSegunNombre(
								entrenador2->proximoPokemon);
				int distanciaEntrenador2 = distanciaEntreDosPosiciones(
						entrenador2->posicion, posicionPokemon2);
				return (distanciaEntrenador1 < distanciaEntrenador2);
			}
			list_sort(listaDeReady, ordenarPorCercaniaAUnaPokenest);
			entrenador = (entrenadorPokemon*) list_get(listaDeReady, 0);
			log_trace(log,
					"SE OBTUVO DE LA LISTA DE ENTRENADORES AL ENTRENADOR %c",
					entrenador->simbolo);
			ejecutandoId = entrenador->simbolo;
		}
	}
	list_remove_by_condition(listaDeReady, tieneMismoId);
	realizarAccion(entrenador);
	pthread_mutex_unlock(&sem_listaDeReady);
}
void atenderClienteEntrenadores(int socket, mensaje_ENTRENADOR_MAPA* mensaje) {
	entrenadorPokemon * unEntrenador;
	if (mensaje->protocolo != HANDSHAKE) {
		pthread_mutex_lock(&sem_listaDeReady);
		bool obtenerSegunSocket(void * data) {
			entrenadorPokemon * entrenador = (entrenadorPokemon *) data;
			return entrenador->socket == socket;
		}
		unEntrenador = (entrenadorPokemon *) list_find(listaDeEntrenadores,
				obtenerSegunSocket);
		if (mensaje->protocolo == POKEMON) {
			/*unEntrenador-> mejorPOkemon malloc ....... saraza}
			 * sem_post entrenador
			 */
		} else {
			if (unEntrenador->accionARealizar == PROXIMAPOKENEST) {
				unEntrenador->proximoPokemon = malloc(2);
			strcpy(unEntrenador->proximoPokemon,mensaje->nombrePokemon);
				}
				unEntrenador->accionARealizar = mensaje->protocolo;
				list_add(listaDeReady, (void *) unEntrenador);
				if (ejecutandoId == unEntrenador->simbolo)
					sem_post(&semaphore_listos);
				if (ejecutandoId == NULL) {
					ejecutandoId = unEntrenador->simbolo;
					sem_post(&semaphore_listos);
				}
			}
			pthread_mutex_unlock(&sem_listaDeReady);
		} else {
			nuevoEntrenador(socket, mensaje);
			log_trace(log, "Se conectó un nuevo entrenador: %c", mensaje->id);
			pthread_mutex_lock(&sem_mapas);
			CrearPersonaje(items, mensaje->id, 1, 1);
			actualizarMapa();
			pthread_mutex_unlock(&sem_mapas);
		}

	}
	int recibirMensajesEntrenadores(int socket) {
		mensaje_ENTRENADOR_MAPA * mensaje;
		if ((mensaje = (mensaje_ENTRENADOR_MAPA *) recibirMensaje(socket))
				== NULL) {
			pthread_mutex_lock(&sem_listaDeEntrenadoresBloqueados);
			// list_remove_by_condition(listaDeEntrenadoresBloqueados,/*entrenador->socket == socket*/); TODO
			pthread_mutex_unlock(&sem_listaDeEntrenadoresBloqueados);
			pthread_mutex_lock(&sem_listaDeEntrenadores);
			// LIST REMOVE AND DESTROY BY CONDITION TODO
			// IF EJECUTANDOID == entrenador->simbolo TODO
			pthread_mutex_unlock(&sem_listaDeEntrenadores);
			return -1;
		} else {
			atenderClienteEntrenadores(socket, mensaje);
		}
		return 1;
	}
	void funcionNULL(int n) {
	}
	void atenderEntrenadores(void) {
		theMinionsRevengeSelect(configuracion.puerto, funcionNULL,
				recibirMensajesEntrenadores);
	}

	void nuevoEntrenador(int socket, mensaje_ENTRENADOR_MAPA * mensajeRecibido) {
		entrenadorPokemon * entrenador = malloc(sizeof(entrenadorPokemon));
		entrenador->socket = socket;
		entrenador->posicion.posicionx = 1;
		entrenador->posicion.posiciony = 1;
		entrenador->pokemonesAtrapados = dictionary_create();
		entrenador->simbolo = mensajeRecibido->id;
		pthread_mutex_lock(&sem_mapas);
		CrearPersonaje(items, mensajeRecibido->id, 1, 1);
		actualizarMapa();
		pthread_mutex_unlock(&sem_mapas);
		list_add(listaDeEntrenadores, (void *) entrenador);
	}
	void actualizarMapa() {
	//	nivel_gui_dibujar(items, configuracion.nombreDelMapa);
	}
	void iniciarDatos() {
		log = log_create("Log", "Mapa", 0, 0);
		listaDeEntrenadores = list_create();
		sem_init(&bloqueados_semaphore, 0, 0);
		sem_init(&semaphore_listos, 0, 0);
		listaDeEntrenadoresBloqueados = list_create();
		pthread_mutex_init(&sem_listaDeEntrenadores, NULL);
		pthread_mutex_init(&sem_listaDeEntrenadoresBloqueados, NULL);
		pthread_mutex_init(&sem_mapas, NULL);
		pthread_mutex_init(&sem_listaDeReady, NULL);
		pthread_mutex_init(&sem_config, NULL);
		listaDeReady = list_create();
		items = list_create();
//		nivel_gui_inicializar();
//		nivel_gui_get_area_nivel(&configuracion.posicionMaxima.posicionx,
//				&configuracion.posicionMaxima.posiciony);
//
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
	void crearHiloPlanificador() {
		pthread_t hiloPlanificador;
		pthread_create(hiloPlanificador, NULL, planificador,
		NULL);
	}

	int main(int arc, char * argv[]) {
		configuracion.nombreDelMapa = string_duplicate(argv[1]);
		iniciarDatos();
		cargarConfiguracion();
		signal(SIGUSR2, cargarConfiguracion);
		crearHiloAtenderEntrenadores();
		crearHiloParaDeadlock();
		crearHiloPlanificador();
		actualizarMapa();
		sleep(60000000);
		/* restarRecurso(items, 'F');
		 */
//liberarDatos();
		return 0;
	}
