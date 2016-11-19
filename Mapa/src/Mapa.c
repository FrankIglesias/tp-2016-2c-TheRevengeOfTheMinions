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
#include <time.h>
#include <pkmn/factory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
typedef struct pokenest_t {
	char * nombrePokemon;
	char id;
	t_list * listaDePokemones;
	posicionMapa posicion;
} pokenest;
typedef struct estructura_lista_dicc { // EL ENTRENADOR TIENE UN DICCIONARIO CON CLAVE LA INICIAL DEL POKEMON Y VALOR UNA LISTA
	char* nombreDelFichero;             // QUE CONTIENE ESTE TIPO DE ESTRUCTURA
	int nivel;
} pokemon;
typedef struct entrenadorPokemon_t {
	char simbolo;
	int socket;
	clock_t tiempo;
	instruccion_t accionARealizar;
	posicionMapa posicion;
	char proximoPokemon;
	t_dictionary * pokemonesAtrapados;
} entrenadorPokemon;
typedef struct config_t {
	char * nombreDelMapa;
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

pthread_mutex_t sem_listaDeReady;
pthread_mutex_t sem_ejecutandoID;
pthread_mutex_t sem_listaDeEntrenadoresBloqueados;
pthread_mutex_t sem_mapas;
pthread_mutex_t sem_config;
pthread_mutex_t sem_listaDeEntrenadores;

sem_t bloqueados_semaphore;

t_list * listaDeReady;
t_list * listaDeEntrenadoresBloqueados;
t_list * items;
t_list * listaDeEntrenadores;

t_log * log;

sem_t semaphore_listos;

int quantum = 0;
char ID = NULL;

t_configuracion configuracion;
int cantDePokenests = 0;
int indexLetras = 0;
char **letras;
char *punteritoAChar;
int *pokemonesDisponibles = NULL;

void recorrerDirectorios(char *ruta, void (*funcionCarpeta(char * ruta)),
		void (*funcionArchivo(char *ruta))) {
	DIR *dip;
	struct dirent *dit;
	if ((dip = opendir(ruta)) == NULL) {
		perror("opendir");
	}
	while ((dit = readdir(dip)) != NULL) {

		if (!(strcmp(dit->d_name, "..") == 0 || strcmp(dit->d_name, ".") == 0)) {
			char * aux = malloc(strlen(ruta) + 2 + strlen(dit->d_name));
			strcpy(aux, ruta);
			strcat(aux, dit->d_name);
			if (aux[strlen(aux) - 4] != '.') {
				strcat(aux, "/");
				funcionCarpeta(aux);
				recorrerDirectorios(aux, funcionCarpeta, funcionArchivo);
			} else {
				funcionArchivo(aux);
			}
			aux = malloc(2);
			free(aux);
		}

	}
	if (closedir(dip) == -1) {
		perror("closedir");
	}
}
char * obtenerNombreDelPokemon(char * ruta) {
	char ** separados = string_split(ruta, "/");
	int i;
	while (separados[i + 1]) {
		i++;
	}
	return separados[i];
}
char * charToString(char letra) {
	char * string = malloc(2);
	string[0] = letra;
	string[1] = '\0';
	return string;
}
bool tienePokemonesAsignados(entrenadorPokemon * unEntrenador) {
	return (dictionary_size(unEntrenador->pokemonesAtrapados) > 0);
}

void funcionArchivosPokenest(char * ruta) {
	char **rutaParseada = string_split(ruta, "/");
	int i = 0;
	while (rutaParseada[i + 1]) {
		i++;
	}
	if (string_ends_with(rutaParseada[i], ".dat")) {
		t_config * config = config_create(ruta);
		pokemon * nuevoPokemon = malloc(sizeof(pokemon));
		nuevoPokemon->nivel = config_get_int_value(config, "Nivel");
		nuevoPokemon->nombreDelFichero = string_duplicate(rutaParseada[i]);
		char * letra_a_buscar;
		void obtenerPokeparada(char * key, void * data) {
			pokenest * aux = (pokenest *) data;
			if (strcmp(aux->nombrePokemon, rutaParseada[i - 1]) == 0) {
				letra_a_buscar = key;
			}
		}
		dictionary_iterator(configuracion.diccionarioDePokeparadas,
				obtenerPokeparada);
		pokenest * unaPokenest = (pokenest *) dictionary_get(
				configuracion.diccionarioDePokeparadas, letra_a_buscar);
		list_add_in_index(unaPokenest->listaDePokemones, 0, nuevoPokemon);
		log_trace(log, "Pokemon nombre %s, nivel: %d",
				nuevoPokemon->nombreDelFichero, nuevoPokemon->nivel);
		sumarRecurso(items, letra_a_buscar[0]);
	}

}
void funcionDirectoriosPokenest(char * ruta) {

	char * rutaAux = malloc(strlen(ruta) + strlen("metadata.txt") + 1);
	strcpy(rutaAux, ruta);
	strcat(rutaAux, "metadata.txt");
	t_config * config = config_create(rutaAux);
	pokenest * nuevaPokenest = malloc(sizeof(pokenest));
	char * string = config_get_string_value(config, "Posicion");
	char ** posiciones = string_split(string, ";");
	char ** nombrePokenest = string_split(rutaAux, "/");
	int i = 0;
	while (nombrePokenest[i + 1]) {
		i++;
	}
	nuevaPokenest->nombrePokemon = string_duplicate(nombrePokenest[i - 1]);
	log_trace(log, "Nombre de la nueva pokenest %s",
			nuevaPokenest->nombrePokemon);
	nuevaPokenest->posicion.posicionx = atoi(posiciones[0]);
	nuevaPokenest->posicion.posiciony = atoi(posiciones[1]);
	nuevaPokenest->listaDePokemones = list_create();
	nuevaPokenest->id =
			strdup(config_get_string_value(config, "Identificador"))[0];
	letras[indexLetras] = config_get_string_value(config, "Identificador");
	cantDePokenests++;
	indexLetras++;
	pokemonesDisponibles = (int *) realloc(pokemonesDisponibles,
			sizeof(int) * cantDePokenests);
	dictionary_put(configuracion.diccionarioDePokeparadas,
			charToString(nuevaPokenest->id), nuevaPokenest);
	CrearCaja(items, nuevaPokenest->id, nuevaPokenest->posicion.posicionx,
			nuevaPokenest->posicion.posiciony, 0);
}
void cargarPokeNests(void) {
	configuracion.diccionarioDePokeparadas = dictionary_create();
	recorrerDirectorios(
			string_from_format("/home/utnso/montaje/Mapas/%s/PokeNests/",
					configuracion.nombreDelMapa), funcionDirectoriosPokenest,
			funcionArchivosPokenest);
}
void cargarMatrizDisponibles() {
	int j;
	for (j = 0; j < cantDePokenests; j++) {

		if (dictionary_has_key(configuracion.diccionarioDePokeparadas,
				letras[j])) {
			pokenest * unaPokenest;
			pthread_mutex_lock(&sem_config);
			unaPokenest = (pokenest*) dictionary_get(
					configuracion.diccionarioDePokeparadas, letras[j]);
			pthread_mutex_unlock(&sem_config);
			pokemonesDisponibles[j] = list_size(unaPokenest->listaDePokemones);
		} else {
			pokemonesDisponibles[j] = 0;
		}
	}
}
void imprimirMatrizDisponibles(int disponibles[cantDePokenests]) {
	int j;
	//log_trace(log, "MATRIZ DE DISPONIBLES");
	for (j = 0; j < cantDePokenests; j++) {
		printf("%d \t", disponibles[j]);
	}
	printf("\n");
}
void cargarConfiguracion(void) {
	t_config * config;
	char * rutaDeConfigs = string_from_format(
			"/home/utnso/montaje/Mapas/%s/metadata.txt",
			configuracion.nombreDelMapa);
	//log_trace(log, "Nombre del mapa: %s", configuracion.nombreDelMapa);
	config = config_create(rutaDeConfigs);
	configuracion.tiempoDeChequeoDeDeadLock = config_get_int_value(config,
			"TiempoChequeoDeadlock");
	//log_trace(log, "Tiempo de checkeo de Deadlock: %d",
//			configuracion.tiempoDeChequeoDeDeadLock);
	configuracion.batalla = config_get_int_value(config, "Batalla");
//	if (configuracion.batalla)
	//log_trace(log, "Algoritmo  de batalla habilitado");
//	else
	//log_trace(log, "Algoritmo  de batalla deshabilitado");
	configuracion.algoritmo = strdup(
			config_get_string_value(config, "algoritmo"));
	//log_trace(log, "Algoritmo: %s", configuracion.algoritmo);
	configuracion.quantum = config_get_int_value(config, "quantum");
	//log_trace(log, "Quantum: %d", configuracion.quantum);

	configuracion.retardo = config_get_int_value(config, "retardo");
	//log_trace(log, "Retardo de planificacion %d", configuracion.retardo);

	configuracion.puerto = strdup(config_get_string_value(config, "Puerto"));
	//log_trace(log, "Puerto: %s", configuracion.puerto);

}
void detectarDeadLock() {
	entrenadorPokemon* entrenadorPerdedor;
	entrenadorPokemon* segundoEntrenador;
	t_pokemon* pokemonGanador;
//	pthread_mutex_lock(&sem_listaDeEntrenadores);
	int cantEntrenadores = list_size(listaDeEntrenadores);
//	pthread_mutex_unlock(&sem_listaDeEntrenadores);
	int pokemonesPorEntrenador[cantEntrenadores][cantDePokenests];
	int pokemonAAtraparPorEntrenador[cantEntrenadores][cantDePokenests + 1];

	int i;
	int j = 0;
	for (i = 0; i < cantEntrenadores; i++) {
		entrenadorPokemon* unEntrenador;
		//	pthread_mutex_lock(&sem_listaDeEntrenadores);
		unEntrenador = (entrenadorPokemon*) list_get(listaDeEntrenadores, i);
		//	pthread_mutex_unlock(&sem_listaDeEntrenadores);
		t_list* valor;
		for (j = 0; j < cantDePokenests; j++) {
			if (dictionary_has_key(unEntrenador->pokemonesAtrapados,
					letras[j])) {
				valor = (t_list*) dictionary_get(
						unEntrenador->pokemonesAtrapados, letras[j]);
				log_trace(log, "EL TAMANIO DE LA LITA ES %d", list_size(valor));
				pokemonesPorEntrenador[i][j] = list_size(valor);
			} else {
				pokemonesPorEntrenador[i][j] = 0;

			}
			if (unEntrenador->accionARealizar != ATRAPAR) {
				pokemonAAtraparPorEntrenador[i][j] = 0; // FALTA CHEQUEAR POR QUE NO LO TOMA
			} else {
				if (strcmp(charToString(unEntrenador->proximoPokemon),
						letras[j]) == 0)
					pokemonAAtraparPorEntrenador[i][j] = 1;
				else
					pokemonAAtraparPorEntrenador[i][j] = 0;
			}
		}

	}

	cargarMatrizDisponibles();

	bool puedoRestar(int disponibles[cantDePokenests],
			int necesidad[cantDePokenests]) {
		int resultado[cantDePokenests];
		for (j = 0; j < cantDePokenests; j++) {
			if (disponibles[j] >= necesidad[j])
				resultado[i] = disponibles[j] - necesidad[j];
			else {
				return false;
			}
		}
		return true;

	}
	void ordenarListaSegunNivelDePokemon(t_list* unaLista) {
		bool tieneMayorNivel(void* data1, void* data2) {
			pokemon* pokemon1 = data1;
			pokemon* pokemon2 = data2;
			return (pokemon1->nivel > pokemon2->nivel);
		}
		list_sort(unaLista, tieneMayorNivel);
		return;
	}
	t_pokemon* obtenerPokemonMasFuerte(entrenadorPokemon* unEntrenador) {
		int mayorNivel;
		int nivel1;
		int nivel2;
		pokemon* valorE;
		pokemon* valorE2;
		t_list* lista1;
		t_list* lista2;
		if (dictionary_has_key(unEntrenador->pokemonesAtrapados, letras[0])) {
			lista1 = (t_list*) dictionary_get(unEntrenador->pokemonesAtrapados,
					letras[0]);
			ordenarListaSegunNivelDePokemon(lista1);
			valorE = (pokemon*) list_get(lista1, 0);
			nivel1 = valorE->nivel;
		} else
			nivel1 = -1;
		char* letraMayorNivel;
		mayorNivel = nivel1;
		for (i = 1; i < cantDePokenests; i++) {
			if (dictionary_has_key(unEntrenador->pokemonesAtrapados,
					letras[i])) {
				lista2 = (t_list*) dictionary_get(
						unEntrenador->pokemonesAtrapados, letras[i]);
				ordenarListaSegunNivelDePokemon(lista2);
				valorE2 = list_get(lista2, 0);
				nivel2 = valorE2->nivel;
			} else
				nivel2 = -1;
			if (mayorNivel > nivel2) {
				letraMayorNivel = letras[i - 1];
				mayorNivel = nivel1;
			} else {
				letraMayorNivel = letras[i];
				mayorNivel = nivel2;
			}
		}
		pokenest * nuevaPokenest;
		pthread_mutex_lock(&sem_config);
		nuevaPokenest = (pokenest*) dictionary_get(
				configuracion.diccionarioDePokeparadas, letraMayorNivel);
		pthread_mutex_unlock(&sem_config);
		t_pkmn_factory* fabricaPokemon = malloc(sizeof(t_pkmn_factory));
		fabricaPokemon = create_pkmn_factory();
		return (create_pokemon(fabricaPokemon, nuevaPokenest->nombrePokemon,
				mayorNivel));
	}
	bool unEntrenadorTienePokemon(entrenadorPokemon* unEntrenador,
			t_pokemon* unPokemon) {
		char * nombre = unPokemon->species;
		char ** posiciones = string_split(nombre, "");
		char* letraInicial = posiciones[0];
		if (dictionary_has_key(unEntrenador->pokemonesAtrapados, letraInicial))
			return true;
		return false;
	}

	entrenadorPokemon* lucharEntreDosEntrenadoresYObtenerPerdedor(
			entrenadorPokemon* unEntrenador, entrenadorPokemon* otroEntrenador) {

		t_pokemon* pokemonGanador;
		t_pokemon* pokemonAPelear1;
		pokemonAPelear1 = (t_pokemon*) obtenerPokemonMasFuerte(unEntrenador);
		t_pokemon* pokemonAPelear2;
		pokemonAPelear2 = (t_pokemon*) obtenerPokemonMasFuerte(otroEntrenador);
		pokemonGanador = (t_pokemon*) pkmn_battle(pokemonAPelear1,
				pokemonAPelear2);
		if (unEntrenadorTienePokemon(unEntrenador, pokemonGanador))
			return unEntrenador;
		else
			return otroEntrenador;

	}

	entrenadorPokemon* recorrerListaDeEntrenadoresYPelear(int cantBloqueados) {
		int i = -1;
		do {
			i++;
		} while (pokemonAAtraparPorEntrenador[i][cantDePokenests] != -1
				&& i <= cantEntrenadores);
		//pthread_mutex_lock(&sem_listaDeEntrenadores);
		entrenadorPerdedor = (entrenadorPokemon*) list_get(listaDeEntrenadores,
				i);
		//pthread_mutex_unlock(&sem_listaDeEntrenadores);
		int l;
		for (l = i + 1; l < cantBloqueados; l++) {
			if (pokemonAAtraparPorEntrenador[l][cantDePokenests] == -1) {
				//	pthread_mutex_lock(&sem_listaDeEntrenadores);
				segundoEntrenador = (entrenadorPokemon*) list_get(
						listaDeEntrenadores, l);
				//	pthread_mutex_unlock(&sem_listaDeEntrenadores);
				entrenadorPerdedor = lucharEntreDosEntrenadoresYObtenerPerdedor(
						entrenadorPerdedor, segundoEntrenador);
			}
		}
		//log_trace(log, "EL ENTRENADOR QUE PERDIO LAS BATALLAS FUE %c. MORIRA",
		//entrenadorPerdedor->simbolo
		//);
		return entrenadorPerdedor;

	}

	bool noDeseaAgarrarUnPokemon(int filaDeNecesidad[cantDePokenests]) {
		int contador = 0;
		int i;
		for (i = 0; i < cantDePokenests; i++) {
			if (filaDeNecesidad[i] == 0)
				contador++;
		}
		return (contador == cantDePokenests);
	}

	int k;
	int noPudoAnalizar = 0;
	int pudoAnalizar = 0;
	int noDeseaAtraparPokemones = 0;
	int w;
	int o;
	int fuisteVos = 0;
	int total = 0;
	int filaDeNecesidad[cantDePokenests];
	int filaDeAsignados[cantDePokenests];
	for (i = 0; i < cantEntrenadores; i++) {
		pokemonAAtraparPorEntrenador[i][cantDePokenests] = -1;
	}

	for (i = 0; i < cantEntrenadores; i++) {
		if (pokemonAAtraparPorEntrenador[i][cantDePokenests] == -1) {
			for (w = 0; w < cantDePokenests; w++) {
				filaDeNecesidad[w] = pokemonAAtraparPorEntrenador[i][w];
				filaDeAsignados[w] = pokemonesPorEntrenador[i][w];
			}
			if (noDeseaAgarrarUnPokemon(filaDeNecesidad)) {
				pokemonAAtraparPorEntrenador[i][cantDePokenests] = 0;
				for (j = 0; j < cantDePokenests; j++) {
					pokemonesDisponibles[j] += filaDeAsignados[j];
				}
				i = 0;
				noPudoAnalizar = 0;
				noDeseaAtraparPokemones++;
			} else {
				if (puedoRestar(pokemonesDisponibles, filaDeNecesidad)) {
					pudoAnalizar++;
					for (j = 0; j < cantDePokenests; j++) {
						pokemonesDisponibles[j] += filaDeAsignados[j];
					}
					pokemonAAtraparPorEntrenador[i][cantDePokenests] = 0;
					i = 0;
					noPudoAnalizar = 0;
				} else {
					pokemonAAtraparPorEntrenador[i][cantDePokenests] = -1;
					noPudoAnalizar++;
					fuisteVos = i;
				}
			}
			total++;
		}
	}
	if (noPudoAnalizar == 1 && cantEntrenadores > 1) {
		log_trace(log, "FELICITACIONES, NO HUBO DEADLOCK");
		return;
	} else if (noPudoAnalizar == 1 && cantEntrenadores == 1) {
		entrenadorPokemon* fuisteTu = (entrenadorPokemon*) list_get(
				listaDeEntrenadoresBloqueados, fuisteVos);
		abastecerEntrenador(fuisteTu);
		list_remove(listaDeEntrenadoresBloqueados, fuisteVos);
		BorrarItem(items, fuisteTu->simbolo);
		actualizarMapa();
		cargarMatrizDisponibles();

		return;
	}
	if ((noPudoAnalizar <= cantEntrenadores - noDeseaAtraparPokemones)
			&& noPudoAnalizar != 0) {

		log_trace(log,
				"SE DETECTO DEADLOCK, LOS ENTRENADORES INVOLUCRADOS EN EL INTERBLOQUEO SON");
		entrenadorPokemon* unEntrenador;
		for (i = 0; i < cantEntrenadores; i++) {
			//		pthread_mutex_lock(&sem_listaDeEntrenadores);
			unEntrenador = (entrenadorPokemon*) list_get(listaDeEntrenadores,
					i);
			//	pthread_mutex_unlock(&sem_listaDeEntrenadores);
			log_trace(log, "ENTRENADOR %c", unEntrenador->simbolo);

		}
		entrenadorPokemon* entrenadorPerdedor =
				(entrenadorPokemon*) recorrerListaDeEntrenadoresYPelear(
						cantEntrenadores);

		mensaje_MAPA_ENTRENADOR mensaje;
		mensaje.protocolo = MORIR;
		enviarMensaje(MAPA_ENTRENADOR, entrenadorPerdedor->socket,
				(void *) &mensaje);
		return;
	} else {
		if ((pudoAnalizar == cantEntrenadores - noDeseaAtraparPokemones)
				|| (cantEntrenadores == noDeseaAtraparPokemones)) {
			log_trace(log, "FELICITACIONES, NO HUBO DEADLOCK");
			return;
		}

	}
}

void atenderDeadLock(void) {
	while (1) {
		sem_wait(&bloqueados_semaphore);
		pthread_mutex_lock(&sem_config);
		int aux = configuracion.tiempoDeChequeoDeDeadLock;
		pthread_mutex_unlock(&sem_config);
		usleep(aux);
		pthread_mutex_lock(&sem_listaDeEntrenadoresBloqueados);
		detectarDeadLock();
		if (list_size(listaDeEntrenadoresBloqueados) > 0)
			sem_post(&bloqueados_semaphore);
		pthread_mutex_unlock(&sem_listaDeEntrenadoresBloqueados);
	}
}
posicionMapa posicionDeUnaPokeNestSegunNombre(char pokeNest) {
	pokenest* pokenestBuscada = (pokenest*) dictionary_get(
			configuracion.diccionarioDePokeparadas, charToString(pokeNest));
	return pokenestBuscada->posicion;
}
int distanciaEntreDosPosiciones(posicionMapa posicion1, posicionMapa posicion2) {
	return abs(posicion1.posicionx - posicion2.posicionx)
			+ abs(posicion1.posiciony - posicion2.posiciony);
}
void abastecerEntrenador(entrenadorPokemon* unEntrenador) {
	mensaje_MAPA_ENTRENADOR mensaje;
	pokenest* unaPoke = (pokenest*) dictionary_get(
			configuracion.diccionarioDePokeparadas,
			charToString(unEntrenador->proximoPokemon));
	if (list_size(unaPoke->listaDePokemones) > 0) {
		pokemon* unPoke = (pokemon*) list_get(unaPoke->listaDePokemones, 0);
		if (dictionary_has_key(unEntrenador->pokemonesAtrapados,
				charToString(unEntrenador->proximoPokemon))) {
			t_list* unaLista = (t_list*) dictionary_get(
					unEntrenador->pokemonesAtrapados,
					charToString(unEntrenador->proximoPokemon));

			list_add(unaLista, (void*) unPoke);

		} else {
			t_list* nuevaLista = list_create();
			list_add(nuevaLista, (void*) unPoke);
			dictionary_put(unEntrenador->pokemonesAtrapados,
					charToString(unEntrenador->proximoPokemon),
					(void*) nuevaLista);
		}
		pthread_mutex_lock(&sem_config);
		list_remove(unaPoke->listaDePokemones, 0);
		pthread_mutex_unlock(&sem_config);
		pthread_mutex_lock(&sem_mapas);
		restarRecurso(items, unaPoke->id);
		pthread_mutex_unlock(&sem_mapas);
		mensaje.protocolo = POKEMON;
		mensaje.nombrePokemon = malloc(strlen(unPoke->nombreDelFichero));
		strcpy(mensaje.nombrePokemon, unPoke->nombreDelFichero);
		enviarMensaje(MAPA_ENTRENADOR, unEntrenador->socket, (void *) &mensaje);

	} else {
		//log_trace(log,
//		"EL SISTEMA NO TIENE EL RECURSO PEDIDO POR EL ENTRENADOR %c", unEntrenador->simbolo
		//	);
		//log_trace(log, "SE AGREGA AL ENTRENADOR %c A LA LISTA DE BLOQUEADOS",
		//	unEntrenador->simbolo
		//	);
		pthread_mutex_lock(&sem_listaDeEntrenadoresBloqueados);
		int flag = 0;
		void buscarEntrenador(entrenadorPokemon * entrenador) {
			if (entrenador->simbolo == unEntrenador->simbolo)
				flag = 1;
		}

		list_iterate(listaDeEntrenadoresBloqueados, buscarEntrenador);
		if (!flag)
			list_add(listaDeEntrenadoresBloqueados, unEntrenador);
		pthread_mutex_unlock(&sem_listaDeEntrenadoresBloqueados);
		if (list_size(listaDeEntrenadoresBloqueados) > 1)
			sem_post(&bloqueados_semaphore);
	}

}
void abastecerAEntrenadoresBloqueados() {
	list_iterate(listaDeEntrenadoresBloqueados, abastecerEntrenador);
}
void realizarAccion(entrenadorPokemon * unEntrenador) {
	mensaje_MAPA_ENTRENADOR mensaje;
	pokenest* pokenestASolicitar;
//log_trace(log, "La accion a realizar es: %s",
//	mostrarProtocolo(unEntrenador->accionARealizar)
//	);
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
	case ATRAPAR:
		pthread_mutex_lock(&sem_listaDeEntrenadores);
		abastecerEntrenador(unEntrenador);
		pthread_mutex_unlock(&sem_listaDeEntrenadores);
		bool tieneElMismoSocket(void *data) {
			entrenadorPokemon * unE = (entrenadorPokemon*) data;
			return unE->socket == unEntrenador->socket;
		}
		list_remove_by_condition(listaDeReady, tieneElMismoSocket);
		quantum = 0;

		break;
	}
	pthread_mutex_lock(&sem_mapas);
	MoverPersonaje(items, unEntrenador->simbolo,
			unEntrenador->posicion.posicionx, unEntrenador->posicion.posiciony);
	actualizarMapa();
	pthread_mutex_unlock(&sem_mapas);
	quantum--;
}
void replanificar() {
	entrenadorPokemon * entrenador;
	pthread_mutex_lock(&sem_config);
	int algoritmo = strcmp(configuracion.algoritmo, "RR");
	pthread_mutex_unlock(&sem_config);
	if (algoritmo == 0) {
		bool ordenarPorTiempo(void* data, void* data2) {
			entrenadorPokemon* entrenador1 = (entrenadorPokemon*) data;
			entrenadorPokemon* entrenador2 = (entrenadorPokemon*) data2;
			return (entrenador1->tiempo < entrenador2->tiempo);
		}
		list_sort(listaDeReady, ordenarPorTiempo);
	} else {
		bool ordenarPorCercaniaAUnaPokenest(void* data, void* data2) {
			entrenadorPokemon* entrenador1 = (entrenadorPokemon*) data;
			entrenadorPokemon* entrenador2 = (entrenadorPokemon*) data2;
			posicionMapa posicionPokemon1 = posicionDeUnaPokeNestSegunNombre(
					entrenador1->proximoPokemon);
			int distanciaEntrenador1 = distanciaEntreDosPosiciones(
					entrenador1->posicion, posicionPokemon1);
			posicionMapa posicionPokemon2 = posicionDeUnaPokeNestSegunNombre(
					entrenador2->proximoPokemon);
			int distanciaEntrenador2 = distanciaEntreDosPosiciones(
					entrenador2->posicion, posicionPokemon2);
			return (distanciaEntrenador1 < distanciaEntrenador2);
		}
		list_sort(listaDeReady, ordenarPorCercaniaAUnaPokenest);

	}
	entrenador = (entrenadorPokemon*) list_get(listaDeReady, 0);
	if (algoritmo == 0) {
		pthread_mutex_lock(&sem_config);
		quantum = configuracion.quantum;
		pthread_mutex_unlock(&sem_config);
	} else {
		if (entrenador != NULL) {
			posicionMapa posix = posicionDeUnaPokeNestSegunNombre(
					entrenador->proximoPokemon);
			quantum = distanciaEntreDosPosiciones(entrenador->posicion, posix);
		}
	}

	if (entrenador == NULL)
		ID = NULL;
	else {
		//log_trace(log,
		//	"Se obtuvo de la lista de entrenadores el entrenador: %c", entrenador->simbolo
		//	);
		ID = entrenador->simbolo;
	}
}

void planificador() {
	while (1) {
		pthread_mutex_lock(&sem_config);
		int aux = configuracion.retardo;
		pthread_mutex_unlock(&sem_config);
		usleep(aux * 1000);
		sem_wait(&semaphore_listos);
		pthread_mutex_lock(&sem_listaDeReady);
		//log_trace(log, "Se ha activado el planificador");

		entrenadorPokemon * entrenador;
		if (ID == NULL) {
			replanificar();
		}
		if (ID != NULL) {
			bool tieneMismoId(void * data) {
				entrenadorPokemon * unEntrenador = (entrenadorPokemon *) data;
				return unEntrenador->simbolo == ID;
			}

			entrenador = (entrenadorPokemon *) list_find(listaDeReady,
					tieneMismoId);
			//log_trace(log, "Entrenador a atender: %c", entrenador->simbolo);
			realizarAccion(entrenador);
			if (quantum <= 0) {
				entrenador->tiempo = time(NULL);
				//log_trace(log, "Fin de quantum");

				replanificar();
				if (ID != NULL)
					sem_post(&semaphore_listos);

			}
		}
		pthread_mutex_unlock(&sem_listaDeReady);
	}
}
void atenderClienteEntrenadores(int socket, mensaje_ENTRENADOR_MAPA* mensaje) {
	entrenadorPokemon * unEntrenador;
	bool obtenerSegunSocket(void * data) {
		entrenadorPokemon * entrenador = (entrenadorPokemon *) data;
		return entrenador->socket == socket;
	}
	unEntrenador = (entrenadorPokemon *) list_find(listaDeEntrenadores,
			obtenerSegunSocket);
	if (unEntrenador == NULL) {
		nuevoEntrenador(socket, mensaje);
	} else {
		//log_trace(log, "Mensaje %s recibido de entrenador %c",
		//mostrarProtocolo(mensaje->protocolo), unEntrenador->simbolo
		//);
		if (mensaje->protocolo == PROXIMAPOKENEST) {
			unEntrenador->proximoPokemon = mensaje->simbolo;
		}
		unEntrenador->accionARealizar = mensaje->protocolo;

		if (unEntrenador->simbolo == ID) {
			//log_trace(log,
			//		"El entrenador %c esta siendo actualmente planificado", unEntrenador->simbolo
			//		);
			sem_post(&semaphore_listos);
		} else {
			unEntrenador->tiempo = time(NULL);
			pthread_mutex_lock(&sem_listaDeReady);
			list_add(listaDeReady, unEntrenador);
			pthread_mutex_unlock(&sem_listaDeReady);
			//log_trace(log, "Entrenador  %c agregado a la lista de ready",
			//	unEntrenador->simbolo
			//	);
			if (ID == NULL) {
				sem_post(&semaphore_listos);
			}
		}
		free(mensaje);
	}
}
void librerarPokemonesAtrapadosAlMorirOTerminarMapa(int socket) {
	bool tieneElMismoSocket(void* data) {
		entrenadorPokemon* unEntrenador = (entrenadorPokemon*) data;
		return (unEntrenador->socket == socket);
	}
	pthread_mutex_lock(&sem_listaDeEntrenadores);
	entrenadorPokemon * entrenador = list_find(listaDeEntrenadores,
			tieneElMismoSocket);
	pthread_mutex_unlock(&sem_listaDeEntrenadores);
	int i;
	for (i = 0; i < cantDePokenests; i++) {
		if (dictionary_has_key(entrenador->pokemonesAtrapados, letras[i])) {
			t_list* listaPokemon = (t_list*) dictionary_get(
					entrenador->pokemonesAtrapados, letras[i]);
			pthread_mutex_lock(&sem_config);
			pokenest* unaPoke = (pokenest*) dictionary_get(
					configuracion.diccionarioDePokeparadas, letras[i]);
			list_add_all(unaPoke->listaDePokemones, listaPokemon);
			pthread_mutex_unlock(&sem_config);
			sumarRecurso(items, letras[i][0]);
		}
	}

//log_trace(log, "LOS POKEMONES DEL ENTRENADOR %c FUERON LIBERADOS",
//entrenador->simbolo
//);
//log_trace(log, "LOS RECURSOS DISPONIBLES DEL SISTEMA QUEDARON");
	cargarMatrizDisponibles();

}
void removerEntrenadoresPorSocket(int socket) {
	char simbolo;
	bool tieneElMismoSocket(void* data) {
		entrenadorPokemon* unEntrenador = (entrenadorPokemon*) data;
		if (unEntrenador->socket == socket) {
			simbolo = unEntrenador->simbolo;
			return true;
		} else
			return false;
	}
	pthread_mutex_lock(&sem_listaDeEntrenadoresBloqueados);
	list_remove_all_by_condition(listaDeEntrenadoresBloqueados,
			tieneElMismoSocket);
	pthread_mutex_unlock(&sem_listaDeEntrenadoresBloqueados);
	pthread_mutex_lock(&sem_listaDeEntrenadores);
	abastecerAEntrenadoresBloqueados();
	list_remove_all_by_condition(listaDeEntrenadores, tieneElMismoSocket);
	pthread_mutex_unlock(&sem_listaDeEntrenadores);

	pthread_mutex_lock(&sem_listaDeReady);
	list_remove_all_by_condition(listaDeReady, tieneElMismoSocket);
	pthread_mutex_unlock(&sem_listaDeReady);

	pthread_mutex_lock(&sem_mapas);
	BorrarItem(items, simbolo);
	if (ID == simbolo) {
		ID = NULL;
	}
	actualizarMapa();
	pthread_mutex_unlock(&sem_mapas);
}
int recibirMensajesEntrenadores(int socket) {
	mensaje_ENTRENADOR_MAPA * mensaje;
	if ((mensaje = (mensaje_ENTRENADOR_MAPA *) recibirMensaje(socket)) == NULL) {
		librerarPokemonesAtrapadosAlMorirOTerminarMapa(socket);
		//log_trace(log, "SE LE ENTREGO A LOS ENTRENADORES EL POKEMON PEDIDO");
		//log_trace(log, "LOS POKEMONES DISPONIBLES QUEDARON");
		cargarMatrizDisponibles();
		removerEntrenadoresPorSocket(socket);

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
	entrenador->simbolo = mensajeRecibido->simbolo;
	pthread_mutex_lock(&sem_mapas);
	CrearPersonaje(items, mensajeRecibido->simbolo, 1, 1);
	actualizarMapa();
	pthread_mutex_unlock(&sem_mapas);
	pthread_mutex_lock(&sem_listaDeEntrenadores);
	list_add(listaDeEntrenadores, entrenador);
	pthread_mutex_unlock(&sem_listaDeEntrenadores);
//log_trace(log, "Entrenador %c agregado a la lista de entrenadores",
//entrenador->simbolo
//);
}
void actualizarMapa() {
	nivel_gui_dibujar(items, configuracion.nombreDelMapa);
}
void iniciarMapa() {
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&configuracion.posicionMaxima.posicionx,
			&configuracion.posicionMaxima.posiciony);
}
void iniciarDatos() {
	log = log_create("Log", "Mapa", 0, 0);
	listaDeEntrenadores = list_create();
	sem_init(&bloqueados_semaphore, 0, 0);
	sem_init(&semaphore_listos, 0, 0);
	listaDeEntrenadoresBloqueados = list_create();
	configuracion.diccionarioDePokeparadas = dictionary_create();
	pthread_mutex_init(&sem_listaDeEntrenadores, NULL);
	pthread_mutex_init(&sem_listaDeEntrenadoresBloqueados, NULL);
	pthread_mutex_init(&sem_mapas, NULL);
	pthread_mutex_init(&sem_listaDeReady, NULL);
	pthread_mutex_init(&sem_config, NULL);
	listaDeReady = list_create();
	items = list_create();
	letras = realloc((void*) punteritoAChar, sizeof(char*));
	iniciarMapa();
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

pthread_t hiloDeDeteccionDeDeadLock;
pthread_t hiloAtencionDeEntrenadores;
pthread_t hiloPlanificador;

void crearHiloParaDeadlock() {
	pthread_create(&hiloDeDeteccionDeDeadLock, NULL, atenderDeadLock,
	NULL);
}
void crearHiloAtenderEntrenadores() {
	pthread_create(&hiloAtencionDeEntrenadores, NULL, atenderEntrenadores,
	NULL);
}
void crearHiloPlanificador() {
	pthread_create(&hiloPlanificador, NULL, planificador,
	NULL);
}

int main(int arc, char * argv[]) {

	configuracion.nombreDelMapa = string_duplicate(argv[1]);
	iniciarDatos();
	cargarConfiguracion();
	cargarPokeNests();
	signal(SIGPIPE, funcionNULL);
	signal(SIGUSR2, cargarConfiguracion);
	log_trace(log, "Se crea hilo para Entrenadores");
	crearHiloAtenderEntrenadores();
	log_trace(log, "Se crea hilo de Deadlock");
	crearHiloParaDeadlock();
	log_trace(log, "Se crea hilo planificador");
	crearHiloPlanificador();
	actualizarMapa();
	pthread_join(hiloPlanificador, NULL);
	pthread_join(hiloAtencionDeEntrenadores, NULL);
	pthread_join(hiloDeDeteccionDeDeadLock, NULL);
	liberarDatos();
	return 0;
}
