#include <config.h>
#include <tiposDato.h>
#include <signal.h>
#include <log.h>
#include <sw_sockets.c>
#include <collections/list.h>
#include <dirent.h>
#include <stdio.h>

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

posicionMapa posicionDeLaPokeNest;
posicionMapa posicionActual;
t_log * log;

int socketCliente;
int pokemonesAtrapados;

bool movimientoVertical = true;
instruccion_t moverPosicion(void) {
	if (posicionActual.posicionx == posicionDeLaPokeNest.posicionx
			&& posicionActual.posiciony == posicionDeLaPokeNest.posiciony) {
		return ATRAPAR;
	} else if (posicionDeLaPokeNest.posicionx == posicionActual.posicionx) {
		if (posicionDeLaPokeNest.posiciony > posicionActual.posiciony)
			return MOVE_UP;
		else
			return MOVE_DOWN;
	} else if (posicionDeLaPokeNest.posiciony == posicionActual.posiciony) {
		if (posicionDeLaPokeNest.posicionx > posicionActual.posicionx)
			return MOVE_RIGHT;
		else
			return MOVE_LEFT;
	} else {
		if (movimientoVertical) {
			movimientoVertical = false;
			if (posicionDeLaPokeNest.posicionx > posicionActual.posicionx)
				return MOVE_RIGHT;
			else
				return MOVE_LEFT;

		} else {
			movimientoVertical = true;
			if (posicionDeLaPokeNest.posiciony > posicionActual.posiciony)
				return MOVE_UP;
			else
				return MOVE_DOWN;
		}
	}
}

void subirVida(void) {
	config.vidas++;
}
void restarVida(char* motivo) {
	config.vidas--;
	close(socketCliente);
	log_trace(log, "Motivo de muerte: %s ", motivo);

	char* rutaDeLosPokemones = malloc(256);
	sprintf(rutaDeLosPokemones,
			"/home/utnso/montaje/Entrenadores/%s/Dir\ de\ Bill",
			config.nombreDelEntrenador);
	borrarArchivosDeUnDirectorio(rutaDeLosPokemones);
	free(rutaDeLosPokemones);

	char* rutaDeLasMedallas = malloc(256);
		sprintf(rutaDeLasMedallas,
				"/home/utnso/montaje/Entrenadores/%s/Medallas",
				config.nombreDelEntrenador);
		borrarArchivosDeUnDirectorio(rutaDeLasMedallas);
	free(rutaDeLasMedallas);


	if (config.vidas < 1) {
		log_trace(log, "Desea reintentar?\n");
		log_trace(log, "Cantidad de intentos:  %d \n", config.reintentos);
		log_trace(log, "0-No \n 1-Si \n");

		int opc;
		scanf("%d", &opc);

		if (opc) {

			reintentar();
		}
	} else if (config.vidas >= 1) {
		jugar();
	}

}

void restarVidaPorSignal(void) {
	restarVida("Muerte por Signal");
}

void restarVidaPorDeadlock(void) {
	restarVida("Muerte por Deadlock");
}

void restarVidaPorComando(void) {
	restarVida("Muerte por comando kill");
}

void reintentar() {

	config.reintentos++;
	levantarHojaDeViaje();
	jugar();

}
void borrarArchivosDeUnDirectorio(char* ruta) {

	char* comando = malloc(256);
	sprintf(comando,
			"exec rm -rf %s/*",
			ruta);

	system(comando);

}

int verificarConfiguracion(t_config *configuracion) {
	/*nombre=Red
	 simbolo=@
	 hojaDeViaje=[PuebloPaleta,CiudadVerde,CiudadPlateada]
	 obj[P-uebloPaleta]=[P,B,G]
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
	log_trace(log, "InicializarConfiguracion");
	config.hojaDeViaje = list_create();
	if (verificarConfiguracion(configuracion)) {
		config.nombreDelEntrenador = strdup(
				config_get_string_value(configuracion, "nombre"));
		config.simbolo = strdup(
				config_get_string_value(configuracion, "simbolo"));
		config.vidas = config_get_int_value(configuracion, "vidas");
		config.reintentos = config_get_int_value(configuracion, "reintentos");
		levantarHojaDeViaje();
		log_trace(log, "Nombre Entrenador: %s", config.nombreDelEntrenador);
		log_trace(log, "Simbolo: %c", config.simbolo);
		log_trace(log, "Vidas : %d", config.vidas);
		log_trace(log, "Reintentos: %d", config.reintentos);

		void imprimirNombreYPokemonesLista(void * data) {
			objetivo * unMapa = (objetivo *) data;
			printf("\nNombre del mapa %s\n", unMapa->nombreDelMapa);
			void imprimirPokemones(void *data) {
				char * caracter = (char) data;
				printf("\t%c", caracter);
			}
			list_iterate(unMapa->pokemones, imprimirPokemones);
		}
		list_iterate(config.hojaDeViaje, imprimirNombreYPokemonesLista);
		printf("\n");
		log_trace(log, "Se cargaron todas las propiedades");
	}

}

void levantarHojaDeViaje() {

	int i = 0;

	while (&(*config_get_array_value(configuracion, "hojaDeViaje")[i]) != NULL) {

		objetivo* unMapa = malloc(sizeof(objetivo));
		unMapa->pokemones = list_create();
		unMapa->nombreDelMapa = strdup(
				config_get_array_value(configuracion, "hojaDeViaje")[i]);

		char* pokemones = string_from_format("obj[%s]", unMapa->nombreDelMapa);

		int j = 0;
		while (&(*config_get_array_value(configuracion, pokemones)[j]) != NULL) {
			char * nombre = malloc(sizeof(char));
			nombre =
					strdup(config_get_array_value(configuracion, pokemones)[j])[0];
			list_add(unMapa->pokemones, (void *) nombre);
			j++;

		}
		list_add(config.hojaDeViaje, (void*) unMapa);
		i++;
	}
}

void iniciarDatos(char * nombreEntrenador) {
	log = log_create("Log", nombreEntrenador, 1, 0);
	configuracion =
			config_create(
					string_from_format(
							"/home/utnso/montaje/Entrenadores/%s/metadata.txt",
							nombreEntrenador));
}

void actualizarPosicion(instruccion_t protocolo) {
	switch (protocolo) {
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
void finDeJuego(void) {
	log_trace(log, "Juego finalizado");

	char* rutaDeLosPokemones = malloc(256);
	sprintf(rutaDeLosPokemones,
				"/home/utnso/montaje/Entrenadores/%s/Dir\ de\ Bill",
				config.nombreDelEntrenador);
	borrarArchivosDeUnDirectorio(rutaDeLosPokemones);
	free(rutaDeLosPokemones);

	char* rutaDeLasMedallas = malloc(256);
	sprintf(rutaDeLasMedallas,
			"/home/utnso/montaje/Entrenadores/%s/Medallas",
			config.nombreDelEntrenador);
	borrarArchivosDeUnDirectorio(rutaDeLasMedallas);
	free(rutaDeLasMedallas);
	exit(1);
}
char * obtenerNombreDelPokemonRecibido(char * archivo) {
	return string_substring(archivo, 0, strlen(archivo) - 7);
}
void jugar(void) {
	pokemonesAtrapados = 0;
	mensaje_ENTRENADOR_MAPA mensajeAEnviar;
	mensaje_MAPA_ENTRENADOR * mensajeARecibir;
	int cantidadDeObjetivos = list_size(config.hojaDeViaje);
	int i;
	for (i = 0; i < cantidadDeObjetivos; i++) {
		objetivo * unObjetivo = list_get(config.hojaDeViaje, i);
		char * rutaDelMetadataDelMapa =
				string_from_format(
						"/home/utnso/montaje/Mapas/%s/metadata.txt",
						unObjetivo->nombreDelMapa);
		t_config * configAux = config_create(rutaDelMetadataDelMapa);

		char * ipMapa = strdup(config_get_string_value(configAux, "IP"));
		int puertoMapa = config_get_int_value(configAux, "Puerto");
		config_destroy(configAux);
		log_trace(log, " Se va a conectar al PUERTO:  %d y al IP: %s",
				puertoMapa, ipMapa);
		socketCliente = crearSocketCliente(ipMapa, puertoMapa);
		free(ipMapa);
		mensajeAEnviar.protocolo = HANDSHAKE;
		mensajeAEnviar.simbolo = config.simbolo[0];
		posicionActual.posicionx = 1;
		posicionActual.posiciony = 1;
		enviarMensaje(ENTRENADOR_MAPA, socketCliente, (void *) &mensajeAEnviar);
		int cantidadDePokemones = list_size(unObjetivo->pokemones);

		for (pokemonesAtrapados = 0; pokemonesAtrapados < cantidadDePokemones;
				pokemonesAtrapados++) {
			mensajeAEnviar.protocolo = PROXIMAPOKENEST;
			mensajeAEnviar.simbolo = (char) list_get(unObjetivo->pokemones,
					pokemonesAtrapados);
			log_trace(log, "MAPA: %s POKEMON: %c", unObjetivo->nombreDelMapa,
					mensajeAEnviar.simbolo);
			enviarMensaje(ENTRENADOR_MAPA, socketCliente,
					(void *) &mensajeAEnviar);
			if ((mensajeARecibir = (mensaje_MAPA_ENTRENADOR *) recibirMensaje(
					socketCliente)) == NULL) {
				log_trace(log, "Socket de mapa caido");
				finDeJuego();
			}
			posicionDeLaPokeNest.posicionx =
					mensajeARecibir->posicion.posicionx;
			posicionDeLaPokeNest.posiciony =
					mensajeARecibir->posicion.posiciony;
			free(mensajeARecibir);
			log_trace(log, "La posicion del pokemon a atrapar es: %d || %d",
					posicionDeLaPokeNest.posicionx,
					posicionDeLaPokeNest.posiciony);
			bool pokemonNoAtrapado = true;
			while (pokemonNoAtrapado) {
				mensajeAEnviar.protocolo = moverPosicion();
				log_trace(log, "Se va a mover hacia %s",
						mostrarProtocolo(mensajeAEnviar.protocolo));
				enviarMensaje(ENTRENADOR_MAPA, socketCliente,
						(void *) &mensajeAEnviar);
				if ((mensajeARecibir =
						(mensaje_MAPA_ENTRENADOR *) recibirMensaje(
								socketCliente)) == NULL) {
					log_trace(log, "Socket de mapa caido");
					finDeJuego();
				}
				if (mensajeARecibir->protocolo == POKEMON) {
					char * comando =
							string_from_format(
									"cp \"/home/utnso/montaje/Mapas/%s/PokeNests/%s/%s\" \"/home/utnso/montaje/Entrenadores/%s/Dir\ de\ Bill\"",
									unObjetivo->nombreDelMapa,
									obtenerNombreDelPokemonRecibido(
											mensajeARecibir->nombrePokemon),
									mensajeARecibir->nombrePokemon,
									config.nombreDelEntrenador);
					system(comando);

					char * comando2 =
												string_from_format(
														"cp \"/home/utnso/montaje/Mapas/%s/medalla-%s\" \"/home/utnso/montaje/Entrenadores/%s/Medallas\"",
														unObjetivo->nombreDelMapa,
														unObjetivo->nombreDelMapa,
														config.nombreDelEntrenador);

					system(comando2);
				}
				if (mensajeAEnviar.protocolo == ATRAPAR) { // o mensajeARecibir->protocolo==POKEMON
					pokemonNoAtrapado = false;
				}
				if (mensajeARecibir->protocolo == OK) {
					actualizarPosicion(mensajeAEnviar.protocolo);
				}
				if (mensajeARecibir->protocolo == MORIR) {
					log_trace(log, "Acabo de morir en batalla");
					close(socketCliente);
					restarVida("Muerte por deadlock");
				}
				if (mensajeARecibir->protocolo == POKEMON) {
					if (pokemonesAtrapados == cantidadDePokemones - 1)
						close(socketCliente);
				}
				free(mensajeARecibir);
			}
		}

	}
	finDeJuego();
}

int main(int argc, char * argv[]) {
	iniciarDatos(argv[1]);
	cargarConfiguracion(configuracion);
	signal(SIGUSR1, subirVida);
	signal(SIGTERM, restarVidaPorSignal);
	signal(SIGINT, restarVidaPorSignal);
	jugar();
	return 0;
}


