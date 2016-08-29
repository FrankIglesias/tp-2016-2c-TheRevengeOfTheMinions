#include <config.h>
#include <tiposDato.h>
#include <signal.h>
#include <log.h>
#include <sw_sockets.c>
#include <collections/list.h>

struct config_t {
	char * nombreDelEntrenador;
	char * simbolo;
	t_list * hojaDeViaje;
	int vidas;
	int reintentos;
} config;
t_config* configuracion;

typedef struct objetivo_t {
	char * nombreDelMapa;
	t_list* pokemones;
} objetivo;
typedef struct nodoPokemon_t {
	char nombreDelPokemon;
} nodoPokemon;
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

			objetivo* unMapa = malloc(sizeof(objetivo));
			unMapa->pokemones = list_create();
			unMapa->nombreDelMapa = strdup(
					config_get_array_value(configuracion, "hojaDeViaje")[i]);

			char* pokemones = string_from_format("obj[%s]",
					unMapa->nombreDelMapa);

			int j = 0;
			while (&(*config_get_array_value(configuracion, pokemones)[j])
					!= NULL) {
				nodoPokemon* unPokemon = malloc(sizeof(nodoPokemon));
				unPokemon->nombreDelPokemon = strdup(
						config_get_array_value(configuracion, pokemones)[j])[0];
				list_add(unMapa->pokemones, (void*) unPokemon);
				j++;

			}
			log_trace(log, "hola");
			list_add(config.hojaDeViaje, (void*) unMapa);
			i++;
		}

		log_trace(log, "Nombre Entrenador: %s", config.nombreDelEntrenador);
		log_trace(log, "Simbolo: %c", config.simbolo);
		log_trace(log, "Vidas : %d", config.vidas);
		log_trace(log, "Reintentos: %d", config.reintentos);

		void imprimirNombreYPokemonesLista(void * data) {
			objetivo * unMapa = (objetivo *) data;
			printf("Nombre del mapa %s\n", unMapa->nombreDelMapa);
			void imprimirPokemones(void *data) {
				printf("\t%c", (char) data);
			}
			printf("\n");
			list_iterate(unMapa->pokemones, imprimirPokemones);
		}
		list_iterate(config.hojaDeViaje, imprimirNombreYPokemonesLista);
		log_trace(log, "Se cargaron todas las propiedades");
	}

}

void iniciarDatos() {
	log = log_create("Log", "Nucleo", 1, 0);
	configuracion =
			config_create(
					"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/Entrenadores/Ash/metadata.txt");
}
void jugar(void) {
	mensaje_ENTRENADOR_MAPA mensajeAEnviar;
	mensaje_MAPA_ENTRENADOR * mensajeARecibir;
	int cantidadDeObjetivos = list_size(config.hojaDeViaje);
	int i, j;
	for (i = 0; i < cantidadDeObjetivos; i++) {
		objetivo * unObjetivo = list_get(config.hojaDeViaje, i);
		char * rutaDelMetadataDelMapa =
				string_from_format(
						"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/Mapas/%s/metadata.txt",
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
		int cantidadDePokemones = list_size(unObjetivo->pokemones);
		for (j = 0; j < cantidadDePokemones; j++) {
			mensajeAEnviar.protocolo = PROXIMAPOKENEST;

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
	signal(SIGUSR1, subirVida);
	signal(SIGTERM, restarVida);
	jugar();
	return 0;
}

