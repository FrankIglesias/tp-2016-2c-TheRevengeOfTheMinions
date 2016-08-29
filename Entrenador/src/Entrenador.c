#include <config.h>
#include <tiposDato.h>
#include <signal.h>
#include <log.h>
#include <sw_sockets.c>
#include <collections/list.h>

typedef struct objetivo_t {
	char * nombreDelMapa;
	t_list* pokemones;
} objetivo;
struct config_t {
	char * nombreDelEntrenador;
	char * simbolo;
	t_list * hojaDeViaje;
	int vidas;
	int reintentos;
} config;
t_config* configuracion;
typedef struct nodoMapa_t {
	objetivo* objetivo;

} nodoMapa;
typedef struct nodoPokemon_t
{
	char* nombreDelPokemon;
}nodoPokemon;
typedef struct hojaDeViaje_t {
	char * nombre;
	t_list * listaDePokemones;
	char * ip;
	int puerto;
	bool finalizado;
} t_hojaDeViaje;
pthread_mutex_t mutex_hojaDeViaje;
pthread_mutex_t vidasSem;
posicionMapa posicionDeLaPokeNest;
posicionMapa posicionActual;
t_log * log;

bool movimientoVertical = true; /*
 Mari :para que esto
 Fran: para que altere movimientos verticales y horizontales mi vida*/
instruccion_t moverPosicion(void) {
	if (posicionActual.posicionx == posicionDeLaPokeNest.posicionx
			&& posicionActual.posiciony == posicionDeLaPokeNest.posiciony) {
		return ATRAPAR;
	} else if (posicionDeLaPokeNest.posicionx == posicionActual.posicionx) {
		if (posicionDeLaPokeNest.posiciony > posicionActual.posiciony)
			//posicionActual.posiciony++;
			return MOVE_UP;
		else
			//posicionActual.posiciony--;
			return MOVE_DOWN;
	} else if (posicionDeLaPokeNest.posiciony == posicionActual.posiciony) {
		if (posicionDeLaPokeNest.posicionx > posicionActual.posicionx)
			//posicionActual.posicionx++;
			return MOVE_RIGHT;
		else
			//	posicionActual.posicionx--;
			return MOVE_LEFT;
	} else {
		if (movimientoVertical) {
			movimientoVertical = false;
			if (posicionDeLaPokeNest.posicionx > posicionActual.posicionx)
				//posicionActual.posicionx++;
				return MOVE_RIGHT;
			else
				//	posicionActual.posicionx--;
				return MOVE_LEFT;

		} else {
			movimientoVertical = true;
			if (posicionDeLaPokeNest.posiciony > posicionActual.posiciony)
				//posicionActual.posiciony++;
				return MOVE_UP;
			else
				//posicionActual.posiciony--;
				return MOVE_DOWN;
		}
	}
}

void subirVida(void) {
	config.vidas++;
}
void restarVida(void) {
	config.vidas--;
	// TODO QUE PASA CUANDO PIERDE UNA VIDA

}
int verificarConfiguracion(t_config *configuracion) {
	/*nombre=Red
	 simbolo=@
	 hojaDeViaje=[PuebloPaleta,CiudadVerde,CiudadPlateada]
	 obj[PuebloPaleta]=[P,B,G]
	 obj[CiudadVerde]=[C,Z,C]
	 obj[CiudadPlateada]=[P,M,P,M,S]
	 vidas=5
	 reintentos=0*/
	log_trace(log, "verificarConfiguracion");
	if (!config_has_property(configuracion, "nombre"))
		log_error(log, "No existe NOMBRE en la configuracion");
	if (!config_has_property(configuracion, "simbolo"))
		log_error(log, "No existe SIMBOLO en la configuracion");
	if (!config_has_property(configuracion, "hojaDeViaje"))
		log_error(log, "No existe HOJA_DE_VIAJE en la configuracion");
	if (!config_has_property(configuracion, "vidas"))
		log_error(log, "No existe VIDAS en la configuracion");
	if (!config_has_property(configuracion, "reintentos"))
		log_error(log, "No existe REINTENTOS en la configuracion");
	return 1;
}

void cargarConfiguracion(t_config* configuracion) {
	/*nombre=Red
	 simbolo=@
	 hojaDeViaje=[PuebloPaleta
	 ,CiudadVerde,CiudadPlateada]
	 obj[PuebloPaleta]=[P,B,G]
	 obj[CiudadVerde]=[C,Z,C]
	 obj[CiudadPlateada]=[P,M,P,M,S]
	 vidas=5
	 reintentos=0*/

	log_trace(log, "InicializarConfiguracion");
	config.hojaDeViaje = list_create();
	if (verificarConfiguracion(configuracion)) {
		config.nombreDelEntrenador = strdup(
				config_get_string_value(configuracion, "nombre"));
		config.simbolo = strdup(
				config_get_string_value(configuracion, "simbolo"));
		config.vidas = config_get_int_value(configuracion, "vidas");
		config.reintentos = config_get_int_value(configuracion, "reintentos");

		int i = 0;
		while (&(*config_get_array_value(configuracion, "hojaDeViaje")[i])
				!= NULL) {
			nodoMapa* unMapa = malloc(sizeof(nodoMapa));
			objetivo* objetivoMapa = malloc(sizeof(objetivo));
			unMapa->objetivo = objetivoMapa;
			objetivoMapa->pokemones = list_create();
			objetivoMapa->nombreDelMapa = config_get_array_value(configuracion,
					"hojaDeViaje")[i];
			char* nombre = malloc(sizeof(objetivoMapa->nombreDelMapa));
			nombre = objetivoMapa->nombreDelMapa;
			char* pokemones = string_from_format("obj[%s]", nombre);
			int j=0;
			//while(&(*config_get_array_value(configuracion, pokemones))[j] != NULL) {

				nodoPokemon* unPokemon = malloc(sizeof(nodoPokemon));
				unPokemon->nombreDelPokemon = config_get_array_value(configuracion,
						pokemones)[j];
				pthread_mutex_lock(&mutex_hojaDeViaje);
				list_add(objetivoMapa->pokemones, (void*) unPokemon);
				pthread_mutex_unlock(&mutex_hojaDeViaje);


				j++;

			//}
			int tamanio = list_size(objetivoMapa->pokemones);
			printf("%d\n", tamanio);
			list_add(config.hojaDeViaje, (void*) unMapa);
			i++;
		}

		log_trace(log, "Nombre Entrenador: %s", config.nombreDelEntrenador);
		log_trace(log, "Simbolo: %s", config.simbolo);
		log_trace(log, "Vidas : %d", config.vidas);
		log_trace(log, "Reintentos: %d", config.reintentos);

		void imprimirNombreYPokemonesLista(t_list* lista) {
			int i;
			for (i = 0; i < list_size(lista); i++) {
				nodoMapa* mapa = (objetivo*) list_get(lista, i);
				printf("Mapa: %s\n", mapa->objetivo->nombreDelMapa);
				int j;
				for(j=0;j<list_size(mapa->objetivo->pokemones); j++) {
				nodoPokemon* pokemones = (nodoPokemon*) list_get(mapa->objetivo->pokemones, j);
				printf("Pokemon: %s\n", pokemones->nombreDelPokemon);
				}

			}

		}
		imprimirNombreYPokemonesLista(config.hojaDeViaje);
		printf("\n");
		log_trace(log, "Se cargaron todas las propiedades");
	}

}
/*void crearHojaDeViaje(void) {
 hojaDeViaje = list_create();
 int cantMapas = list_size(config.listaDeMapas);
 void ingresarALaHojaDeViaje(char * buffer) {
 // TO BE CONTINUE... tengo dudas como pensaron las config.
 }

 list_iterate(config.listaDeMapas, ingresarALaHojaDeViaje);
 }*/
void iniciarDatos() {
	pthread_mutex_init(&mutex_hojaDeViaje, NULL);
	log = log_create("Log", "Nucleo", 1, 0);
	configuracion = config_create(
			"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/Entrenadores/Ash/metadata.txt");
}
/*int contarPokemones(char ** pokemones) {
	int i = 0;
	while (pokemones != NULL) {
		i++;
	}
	return i;
}*/
void jugar(void) {
	mensaje_ENTRENADOR_MAPA mensajeAEnviar;
	mensaje_MAPA_ENTRENADOR * mensajeARecibir;
	int cantidadDeObjetivos = list_size(config.hojaDeViaje);
	int i, j;
	for (i = 0; i < cantidadDeObjetivos; i++) {
		objetivo * unObjetivo = list_get(config.hojaDeViaje, i);
		char * rutaDelMetadataDelMapa = string_from_format(
				"/home/utnso/TP/Mapas/%s/metadata.txt",
				unObjetivo->nombreDelMapa);
		t_config * configAux = config_create(rutaDelMetadataDelMapa);
		char * ipMapa = strdup(config_get_string_value(configAux, "IP"));
		char * puertoMapa = strdup(
				config_get_string_value(configAux, "puerto"));
		config_destroy(configAux);
		int socketCliente = crearSocketCliente(ipMapa, puertoMapa);
		free(ipMapa);
		free(puertoMapa);
		mensajeAEnviar.protocolo = HANDSHAKE;
		mensajeAEnviar.id = config.simbolo[0];
		posicionActual.posicionx = 1;
		posicionActual.posiciony = 1;
		// ENVIAR MENSAJE TODO
		mensajeARecibir = (mensaje_MAPA_ENTRENADOR *) recibirMensaje(
				socketCliente);
		free(mensajeARecibir);
		//int cantidadDePokemones = contarPokemones(unObjetivo->pokemones);
		for (j = 0; j < list_size(unObjetivo->pokemones); j++) {
			mensajeAEnviar.protocolo = PROXIMAPOKENEST;
			//mensajeAEnviar.id = unObjetivo->pokemones[j][0];
			// ENVIAR MENSAJE TODO
			mensajeARecibir = (mensaje_MAPA_ENTRENADOR *) recibirMensaje(
					socketCliente);
			posicionDeLaPokeNest.posicionx =
					mensajeARecibir->posicion.posicionx;
			posicionDeLaPokeNest.posiciony =
					mensajeARecibir->posicion.posiciony;
			free(mensajeARecibir);
			bool pokemonNoAtrapado = true;
			while (pokemonNoAtrapado) {
				mensajeAEnviar.protocolo = moverPosicion();
				// ENVIAR MENSAJE TODO

				mensajeARecibir = (mensaje_MAPA_ENTRENADOR *) recibirMensaje(
						socketCliente);
				if (mensajeAEnviar.protocolo == ATRAPAR) { // o mensajeARecibir->protocolo==POKEMON
									pokemonNoAtrapado = false;
								}
				if (mensajeARecibir->protocolo == OK) {
					switch (mensajeAEnviar.protocolo) {
					case MOVE_DOWN:
						posicionActual.posiciony--;
						break;
					case MOVE_UP:
						posicionActual.posiciony++;
						break;
					case MOVE_LEFT:
						posicionActual.posicionx--;
						break;
					case MOVE_RIGHT:
						posicionActual.posicionx++;
						break;
					default:
						log_trace(log, "La estamos pifiando");
						break;
					}
				}
				free(mensajeARecibir);
			}
		}

	}

}

int main(int argc, char * argv[]) {
	iniciarDatos();
	cargarConfiguracion(configuracion);
	//signal(SIGUSR1, subirVida);
	//signal(SIGTERM, restarVida);
	//jugar();
	return 0;
}

