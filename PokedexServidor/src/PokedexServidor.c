#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <tiposDato.h>
#include <sw_sockets.h>
#include <log.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <bitarray.h>
#include <collections/list.h>
#include <time.h>
#include <pthread.h>

#define BLOCK_SIZE 64
#define DIRMONTAJE 65535
pthread_mutex_t sem_tablaDeArchivos;
pthread_mutex_t sem_bitarray;
pthread_mutex_t sem_memory;
typedef enum
	__attribute__((packed)) {
		BORRADO = '\0', ARCHIVO = '\1', DIRECTORIO = '\2',
} osada_file_state;

typedef struct OSADA_HEADER {
	unsigned char identificador[7];
	uint8_t version;
	uint32_t fs_blocks; //en bloques
	uint32_t bitmap_blocks; //en bloques
	uint32_t inicioTablaAsignaciones; //bloque de inicio tabla de asignaciones
	uint32_t data_blocks; // cantidad de bloques asignadas para datos
	char relleno[40];
} osadaHeader;

typedef struct osadaFile {
	osada_file_state estado; //0 borrado, 1 ocupado, 2 directorio
	unsigned char nombreArchivo[17];
	uint16_t bloquePadre;
	uint32_t tamanioArchivo;
	uint32_t fechaUltimaModif; //como hago fechas?
	uint32_t bloqueInicial;
} archivos_t;

osadaHeader fileHeader;
archivos_t * tablaDeArchivos;
int osadaFile;
t_bitarray* bitmap;
int * tablaDeAsignaciones;
char * bloquesDeDatos;
t_log * log;
char * data;

void limpiarNombre(int i) {
	memset(tablaDeArchivos[i].nombreArchivo, 0, 17);
}
int tienenElMismoNombre(char * nombre1, char * nombre2) {
	char * aux = malloc(18);
	memcpy(aux, nombre1, 17);
	aux[17] = '\0';
	int auxRes = strcmp(aux, nombre2);
	free(aux);
	return auxRes;
}
uint16_t buscarIndiceArchivo(uint16_t padre, char * nombre,
		osada_file_state tipo) {
	uint16_t i;
	for (i = 0; i < 2048; i++) {
		if ((padre == tablaDeArchivos[i].bloquePadre)
				&& (tablaDeArchivos[i].estado == tipo)
				&& (tienenElMismoNombre(tablaDeArchivos[i].nombreArchivo,
						nombre) == 0)) {
			return i;
		}
	}
	return -1;
}
char * tipoArchivo(int i) {
	switch (tablaDeArchivos[i].estado) {
	case BORRADO:
		return "BORRADO";
		break;
	case ARCHIVO:
		return "ARCHIVO";
		break;
	case DIRECTORIO:
		return "DIRECTORIO";
		break;
	default:
		return "SARASA";
	}
}
uint32_t estadoEnum(uint16_t i) {
	int aux = i;
	switch (tablaDeArchivos[aux].estado) {
	case BORRADO:
		return 0;
		break;
	case ARCHIVO:
		return 1;
		break;
	case DIRECTORIO:
		return 2;
		break;
	default:
		return -1;
	}
}
int buscarBloqueLibre() {
	int i;
	for (i = 0; i < fileHeader.data_blocks; ++i) {
		if (bitarray_test_bit(bitmap, i) == 0) {
			bitarray_set_bit(bitmap, i);
			tablaDeAsignaciones[i] = -1;
			return i;
		}
	}
	log_error(log, "No hay memoria suficiente");
	return -1;
}
int asignarBloqueLibre(int bloqueAnterior) {
	int bloqueLibre = buscarBloqueLibre();
	if (bloqueLibre == -1)
		return -1;
	tablaDeAsignaciones[bloqueAnterior] = bloqueLibre;
	return 0;
}
int verificarBloqueSiguiente(int *actual, int *siguiente) {
	if (tablaDeAsignaciones[*siguiente] == 0) {
		log_trace(log, "No hay un bloque siguiente");
		if (asignarBloqueLibre(actual) == -1)
			return -1;
	}
	*actual = tablaDeAsignaciones[*actual];
	*siguiente = tablaDeAsignaciones[*actual];
	return 0;
}
uint16_t buscarAlPadre(char *path) { // Del ultimo directorio sirve Directorios
	int i = 0;
	int j = 0;
	uint16_t padre = DIRMONTAJE;
	char ** ruta = string_split(path, "/");
	for (j = 0; j < 2048 && ruta[i + 1]; j++) {
		if (padre == tablaDeArchivos[j].bloquePadre
				&& (tienenElMismoNombre(tablaDeArchivos[j].nombreArchivo,
						ruta[i]) == 0)
				&& tablaDeArchivos[j].estado == DIRECTORIO) {
			padre = j;
			i++;
			j = 0;
		}
	}
	if (ruta[i + 1]) {
		log_error(log, "No existe el path");
		return -2; // si es -1 es DIRMONTAJE
	}
	return padre;
}
int verificarSiExiste(char * path, osada_file_state tipo) {
	char ** ruta = string_split(path, "/");
	int j = 0;
	uint16_t padre = DIRMONTAJE;
	uint16_t file;
	while (ruta[j + 1]) {
		file = buscarIndiceArchivo(padre, ruta[j], DIRECTORIO);
		if (file == DIRMONTAJE) {
			log_error(log,
					"Ya existe un elemento del mismo nombre en la carpeta");
			return -1;
		}
		padre = file;
		j++;
	}
	return buscarIndiceArchivo(padre, ruta[j], tipo);
}
int existeUnoIgual(uint16_t padre, char* nombre, osada_file_state tipo) {
	int i;
	for (i = 0; i < 2048; i++) {
		if ((padre == tablaDeArchivos[i].bloquePadre)
				&& (tienenElMismoNombre(tablaDeArchivos[i].nombreArchivo,
						nombre) == 0) && (tablaDeArchivos[i].estado == tipo)) {
			return 1;
		}
	}
	return 0;
}
int ingresarEnLaTArchivos(uint16_t padre, char *nombre, osada_file_state tipo) {
	int i;
	if (strlen(nombre) <= 17) {
		for (i = 0; i < 2048; i++) {
			if (tablaDeArchivos[i].estado == BORRADO) {
				tablaDeArchivos[i].bloqueInicial = -1;
				tablaDeAsignaciones[tablaDeArchivos[i].bloqueInicial] = -1;
				tablaDeArchivos[i].bloquePadre = padre;
				tablaDeArchivos[i].estado = tipo;
				tablaDeArchivos[i].fechaUltimaModif = time(NULL);
				memcpy(tablaDeArchivos[i].nombreArchivo, nombre, 17);
				tablaDeArchivos[i].tamanioArchivo = 0;
				return i;
			}
		}
	}
	if (17 <= strlen(nombre)) {
		log_error(log, "EL nombre es muy largo");
		return -3;
	} else
		log_error(log, "NO hay mas espacio en la tabla de archivo");
	return -2;
}
char * leerFile(int file) {
	char * lectura = malloc(tablaDeArchivos[file].tamanioArchivo + 1); // +1 para indicar fin de archivo
	int bloqueSiguiente = tablaDeArchivos[file].bloqueInicial;
	int puntero = 0;
	while (puntero != tablaDeArchivos[file].tamanioArchivo) {
		if ((tablaDeArchivos[file].tamanioArchivo - puntero) > BLOCK_SIZE) {
			pthread_mutex_lock(&sem_memory);
			memcpy(lectura + puntero,
					bloquesDeDatos + (BLOCK_SIZE * bloqueSiguiente),
					BLOCK_SIZE);
			pthread_mutex_unlock(&sem_memory);
			puntero += BLOCK_SIZE;
			bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
			if (bloqueSiguiente == -1)
				break;
		} else {
			pthread_mutex_lock(&sem_memory);
			memcpy(lectura + puntero,
					bloquesDeDatos + (BLOCK_SIZE * bloqueSiguiente),
					tablaDeArchivos[file].tamanioArchivo - puntero);
			pthread_mutex_unlock(&sem_memory);
			puntero += tablaDeArchivos[file].tamanioArchivo - puntero;
		}
	}
	lectura[tablaDeArchivos[file].tamanioArchivo] = '\0';
	return lectura;
}

char * leerArchivo(char * path, int *aux) {
	pthread_mutex_lock(&sem_tablaDeArchivos);
	int file = verificarSiExiste(path, ARCHIVO);
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	log_info(log, "Se va a leer el archivo: %s Tamaño: %u", path,
			tablaDeArchivos[file].tamanioArchivo);
	if (file == -1 && tablaDeArchivos[file].estado != ARCHIVO)
		return NULL;
	char * lectura = malloc(tablaDeArchivos[file].tamanioArchivo + 1); // +1 para indicar fin de archivo
	lectura = leerFile(file);
	*aux = tablaDeArchivos[file].tamanioArchivo;
	log_trace(log, "Lectura: %s", lectura);
	return lectura;
}

int deltaBuffer(int file, int tamanioNuevo, int offset) {
	int i;
	int tamanoArchivoNuevo = tamanioNuevo + offset;
	int puntero = 0;
	int bloqueActual = tablaDeArchivos[file].bloqueInicial;
	if (tablaDeArchivos[file].tamanioArchivo == (tamanioNuevo + offset))
		return 0;
	if (tamanoArchivoNuevo < tablaDeArchivos[file].tamanioArchivo) {
		for (i = 0; i < tablaDeArchivos[file].tamanioArchivo; i++) {
			if (puntero < tamanoArchivoNuevo) {
				bloqueActual = tablaDeAsignaciones[bloqueActual];
				puntero += BLOCK_SIZE;
			}
		}
		while (bloqueActual != -1) {
			pthread_mutex_lock(&sem_bitarray);
			bitarray_clean_bit(bitmap, bloqueActual);
			pthread_mutex_unlock(&sem_bitarray);
			tablaDeAsignaciones[bloqueActual] = -1;
			bloqueActual = tablaDeAsignaciones[bloqueActual];
		}
	}
	return tamanoArchivoNuevo - tablaDeArchivos[file].tamanioArchivo; // Agrego letras
}

int escribirArchivo(char * path, char * buffer, int offset, int tamanio) {
	log_trace(log, "Escribiendo en: %s  offset:%d", path, offset);
	pthread_mutex_lock(&sem_tablaDeArchivos);
	int file = verificarSiExiste(path, ARCHIVO);
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	if (file == -1)
		return -1;
	int aux = 0;
	int tam = tamanio;
	int puntero = 0; // Para saber el tamaño
	if (tablaDeArchivos[file].bloqueInicial == -1) {
		pthread_mutex_lock(&sem_bitarray);
		tablaDeArchivos[file].bloqueInicial = buscarBloqueLibre();
		pthread_mutex_unlock(&sem_bitarray);
	}
	if (tablaDeArchivos[file].bloqueInicial == -1)
		return -4;
	int bloqueActual = tablaDeArchivos[file].bloqueInicial;
	while (offset != puntero) {
		pthread_mutex_lock(&sem_bitarray);
		if (tablaDeAsignaciones[bloqueActual] == -1) {
			if (asignarBloqueLibre(bloqueActual) == -1) {
				pthread_mutex_unlock(&sem_bitarray);
				return -4;
			}
		}
		pthread_mutex_unlock(&sem_bitarray);
		if ((offset - puntero) > BLOCK_SIZE) {
			puntero += BLOCK_SIZE;
		} else {
			aux = offset - puntero;
			puntero = offset;
		}
		bloqueActual = tablaDeAsignaciones[bloqueActual];
	}
	puntero = 0;
	if (tam < (BLOCK_SIZE - aux)) {
		pthread_mutex_lock(&sem_memory);
		memcpy(bloquesDeDatos + (BLOCK_SIZE * bloqueActual) + aux, buffer, tam);
		pthread_mutex_unlock(&sem_memory);
		tam = 0;
	}
	while (tam != 0) {
		if (tam > BLOCK_SIZE) {
			pthread_mutex_lock(&sem_memory);
			memcpy(bloquesDeDatos + (BLOCK_SIZE * bloqueActual),
					buffer + puntero,
					BLOCK_SIZE);
			pthread_mutex_unlock(&sem_memory);
			puntero += BLOCK_SIZE;
			tam -= BLOCK_SIZE;

			if (tablaDeAsignaciones[bloqueActual] == -1) {
				pthread_mutex_lock(&sem_bitarray);
				if (asignarBloqueLibre(bloqueActual) == -1) {
					tam = 0;
					pthread_mutex_unlock(&sem_bitarray);
					break;
				}
				pthread_mutex_unlock(&sem_bitarray);
			}
			bloqueActual = tablaDeAsignaciones[bloqueActual];
		} else {
			pthread_mutex_lock(&sem_memory);
			memcpy(bloquesDeDatos + (BLOCK_SIZE * bloqueActual),
					buffer + puntero, tam);
			pthread_mutex_unlock(&sem_memory);
			puntero += tam;
			tam = 0;
		}
	}

	pthread_mutex_lock(&sem_tablaDeArchivos);
	if (tablaDeArchivos[file].tamanioArchivo == 0)
		tablaDeArchivos[file].tamanioArchivo = tamanio;
	else {
		if (tamanio == 4096 && offset != 0) {
			tablaDeArchivos[file].tamanioArchivo = offset + tamanio;
		} else {
			tablaDeArchivos[file].tamanioArchivo += deltaBuffer(file, tamanio,
					offset); // + offset;
			log_trace(log, "tamaño del archivo %u",
					tablaDeArchivos[file].tamanioArchivo);
		}
	}
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	sincronizarMemoria();
	return puntero;
}
int crearArchivo(char * path) {
	int i = 0;
	int j = 0;
	uint16_t padre = DIRMONTAJE;
	if (path != "/") {
		pthread_mutex_lock(&sem_tablaDeArchivos);
		padre = buscarAlPadre(path);
		pthread_mutex_unlock(&sem_tablaDeArchivos);
	}
	if (padre == -2)
		return -1;
	char ** ruta = string_split(path, "/");
	while (ruta[j + 1]) { // BUSCO EL NOMBRE
		j++;
	}
	pthread_mutex_lock(&sem_tablaDeArchivos);
	if (existeUnoIgual(padre, ruta[j], ARCHIVO)) {
		pthread_mutex_unlock(&sem_tablaDeArchivos);
		return -1;
	}
	i = ingresarEnLaTArchivos(padre, ruta[j], ARCHIVO);
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	if (i > 0)
		log_trace(log, "se ha creado un archivo en el bloque: %u, padre: %u",
				tablaDeArchivos[i].bloqueInicial, padre);
	sincronizarMemoria();
	return i;
}
int borrar(char * path) {
	log_trace(log, "Borrar archivo  %s", path);
	int j;

	pthread_mutex_lock(&sem_tablaDeArchivos);
	int file = verificarSiExiste(path, ARCHIVO);
	if (file != -1) {
		while (tablaDeArchivos[file].bloqueInicial != -1) {
			j = tablaDeArchivos[file].bloqueInicial;
			pthread_mutex_lock(&sem_bitarray);
			bitarray_clean_bit(bitmap, j);
			tablaDeArchivos[file].bloqueInicial = tablaDeAsignaciones[j];
			tablaDeAsignaciones[j] = -1;
			pthread_mutex_unlock(&sem_bitarray);
		}

		limpiarNombre(file);

		tablaDeArchivos[file].bloqueInicial = -1;
		tablaDeArchivos[file].bloquePadre = -1;
		tablaDeArchivos[file].estado = BORRADO;
		tablaDeArchivos[file].tamanioArchivo = 0;
		pthread_mutex_unlock(&sem_tablaDeArchivos);
		log_trace(log, "Se ha borrado el archivo: %s",
				tablaDeArchivos[file].nombreArchivo);
		sincronizarMemoria();
		return 1;
	}
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	return -1;
}
int crearDir(char * path) {
	log_info(log, "creando directorio");
	char ** ruta = string_split(path, "/");
	int i = 0;
	int j = 0;
	uint16_t padre = DIRMONTAJE;
	pthread_mutex_lock(&sem_tablaDeArchivos);
	if (path != "/" && (ruta[i + 1])) {
		while (ruta[i + 1]) {
			for (j = 0; j < 2048; ++j) {
				if ((tablaDeArchivos[j].bloquePadre == padre)
						&& (tablaDeArchivos[j].estado == DIRECTORIO)
						&& (tienenElMismoNombre(
								tablaDeArchivos[j].nombreArchivo, ruta[i]) == 0)) {
					padre = j;
					i++;

					break;
				}
			}
		}

	}
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	pthread_mutex_lock(&sem_tablaDeArchivos);
	if (existeUnoIgual(padre, ruta[i], DIRECTORIO)) {
		pthread_mutex_unlock(&sem_tablaDeArchivos);

		return -1;
	}
	i = ingresarEnLaTArchivos(padre, ruta[i], DIRECTORIO);
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	sincronizarMemoria();
	return i;
}
int borrarDir(char * path) {
	log_info(log, "borrando directorio: %s", path);
	pthread_mutex_lock(&sem_tablaDeArchivos);
	int file = verificarSiExiste(path, DIRECTORIO);

	int j;
	if (file != -1) {
		for (j = 0; j < 2048; j++) {
			if (archivoDirectorio(j)
					&& tablaDeArchivos[j].bloquePadre == file) {
				log_error(log, "TIene archivos adentro no puede ser borrado");
				pthread_mutex_unlock(&sem_tablaDeArchivos);
				return -1;
			}
		}
	}
	limpiarNombre(file);
	pthread_mutex_unlock(&sem_tablaDeArchivos);

	tablaDeArchivos[file].bloqueInicial = -1;
	tablaDeArchivos[file].bloquePadre = -1;
	tablaDeArchivos[file].estado = BORRADO;
	tablaDeArchivos[file].tamanioArchivo = 0;
	sincronizarMemoria();
	return 1;
}
int renombrar(char * path, char * path2) {
	log_info(log, "Renombrando: %s por %s", path, path2);
	uint16_t menosDos = -2;
	pthread_mutex_lock(&sem_tablaDeArchivos);
	uint16_t file = verificarSiExiste(path, ARCHIVO);
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	uint16_t padre = -2;
	int i = 0;
	char ** ruta = string_split(path2, "/");
	if (file == DIRMONTAJE) {
		pthread_mutex_lock(&sem_tablaDeArchivos);
		file = verificarSiExiste(path, DIRECTORIO);
		pthread_mutex_unlock(&sem_tablaDeArchivos);
	}
	if (file != DIRMONTAJE) {
		pthread_mutex_lock(&sem_tablaDeArchivos);
		padre = buscarAlPadre(path2);
		pthread_mutex_unlock(&sem_tablaDeArchivos);
		if (padre != menosDos) {
			while (ruta[i + 1]) { // busco nombre del archivo
				i++;
			}
			if (strlen(ruta[i]) > 17)
				return -3;
			pthread_mutex_lock(&sem_tablaDeArchivos);
			if (!existeUnoIgual(padre, ruta[i], tablaDeArchivos[file].estado)) {
				limpiarNombre(file);
				memcpy(&tablaDeArchivos[file].nombreArchivo, ruta[i],
						strlen(ruta[i]));
				tablaDeArchivos[file].nombreArchivo[strlen(ruta[i])] = '\0';
				tablaDeArchivos[file].bloquePadre = padre;
				sincronizarMemoria();
				pthread_mutex_unlock(&sem_tablaDeArchivos);
				return 1;
			} else {
				log_error(log, "Ya existe un archivo con el mismo nombre");
			}
		} else {
			log_error(log,
					"No existe alguna parte de la ruta donde quiere mover");
		}
	} else {
		log_error(log, "NO existe el archivo");
	}
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	sincronizarMemoria();
	return -1;
}
char * readAttr(char *path, int *var) {
	log_debug(log, "readAttr: %s", path);
	char * lista;
	int aux = 0;
	uint16_t file = -1;
	int j = 0;
	int i;
	if (strcmp(path, "/") != 0) {
		char ** ruta = string_split(path, "/");
		while (ruta[j]) {
			pthread_mutex_lock(&sem_tablaDeArchivos);
			file = buscarIndiceArchivo(file, ruta[j], DIRECTORIO);
			pthread_mutex_unlock(&sem_tablaDeArchivos);
			if (file == DIRMONTAJE)
				return NULL;
			j++;
		}
	}
	pthread_mutex_lock(&sem_tablaDeArchivos);
	for (i = 0; i < 2048; i++) {
		if ((file == tablaDeArchivos[i].bloquePadre) && (archivoDirectorio(i)))
			aux++;
	}
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	lista = malloc(aux * 17);
	aux = 0;
	pthread_mutex_lock(&sem_tablaDeArchivos);
	for (i = 0; i < 2048; i++) {
		if ((file == tablaDeArchivos[i].bloquePadre)
				&& (archivoDirectorio(i))) {
			memcpy(lista + (aux * 17), tablaDeArchivos[i].nombreArchivo, 17);
			aux++;
		}

	}
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	*var = aux;
	return lista;
}
int getAttr(char *path) {
	log_debug(log, "getAttr: %s", path);
	int j = 0;
	uint16_t file;
	char ** ruta = string_split(path, "/");
	uint16_t padre = DIRMONTAJE;
	while (ruta[j + 1]) {
		pthread_mutex_lock(&sem_tablaDeArchivos);
		file = buscarIndiceArchivo(padre, ruta[j], DIRECTORIO);
		pthread_mutex_unlock(&sem_tablaDeArchivos);
		if (file == -1)
			return -1;
		padre = file;
		j++;
	}
	pthread_mutex_lock(&sem_tablaDeArchivos);
	file = buscarIndiceArchivo(padre, ruta[j], DIRECTORIO);
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	if (file == DIRMONTAJE) {
		pthread_mutex_lock(&sem_tablaDeArchivos);
		file = buscarIndiceArchivo(padre, ruta[j], ARCHIVO);
		pthread_mutex_unlock(&sem_tablaDeArchivos);
	}
	return file;
}
int truncar(char * path, int tamanio) {
	pthread_mutex_lock(&sem_tablaDeArchivos);
	int file = verificarSiExiste(path, ARCHIVO);
	pthread_mutex_unlock(&sem_tablaDeArchivos);

	if (tablaDeArchivos[file].bloqueInicial == -1) {
		pthread_mutex_lock(&sem_bitarray);
		tablaDeArchivos[file].bloqueInicial = buscarBloqueLibre();
		pthread_mutex_unlock(&sem_bitarray);
	}

	if (tablaDeArchivos[file].bloqueInicial == -1)
		return -4;

	int puntero = tamanio / BLOCK_SIZE;
	if (tamanio % BLOCK_SIZE != 0)
		puntero++;
	int bloqueActual = tablaDeArchivos[file].bloqueInicial;
	int i;
	for (i = 0; i < puntero; i++) {
		if (asignarBloqueLibre(bloqueActual) == -1)
			return -4;
		bloqueActual = tablaDeAsignaciones[bloqueActual];
	}

	if (tamanio < tablaDeArchivos[file].tamanioArchivo) {
		while (bloqueActual != -1) {
			pthread_mutex_lock(&sem_bitarray);
			bitarray_clean_bit(bitmap, bloqueActual);
			pthread_mutex_unlock(&sem_bitarray);
			tablaDeAsignaciones[bloqueActual] = -1;
			bloqueActual = tablaDeAsignaciones[bloqueActual];
		}
	}

	tablaDeArchivos[file].tamanioArchivo = tamanio;

	log_trace(log, "Se quizo truncar el archivo %s al tamaño %u", path,
			tablaDeArchivos[file].tamanioArchivo);

	sincronizarMemoria();
	return 0;
}

int atenderPeticiones(int * aux) {
	int socket = aux;
	mensaje_CLIENTE_SERVIDOR * mensaje;
	int devolucion = 1;
	uint16_t devolucion16 = 1;
	char * puntero;
	int var;
	while ((mensaje = (mensaje_CLIENTE_SERVIDOR *) recibirMensaje(socket))
			!= NULL) {
		switch (mensaje->protolo) {
		case LEER:
			var = 0;
			mensaje->buffer = leerArchivo(mensaje->path, &var);
			if (mensaje->buffer == NULL) {
				mensaje->protolo = ERROR;
			}
			if (var == 0) {
				mensaje->protolo = VACIO;
			} else {
				mensaje->protolo = SLEER;
			}
			mensaje->tamano = var;

			break;
		case ESCRIBIR:

			devolucion = escribirArchivo(mensaje->path, mensaje->buffer,
					mensaje->offset, mensaje->tamano);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			if (devolucion == -4)
				mensaje->protolo = ERRORESPACIO;
			mensaje->tamano = devolucion;

			break;
		case CREAR:
			devolucion = crearArchivo(mensaje->path);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			if (devolucion == -2)
				mensaje->protolo = ERROREDQUOT;
			if (devolucion == -3)
				mensaje->protolo = ERRORENAMETOOLONG;
			mensaje->tamano = 0;
			break;
		case TRUNCAR:

			if (truncar(mensaje->path, mensaje->tamano) == -4) {
				mensaje->protolo = ERRORESPACIO;
			}

			break;

		case CREARDIR:
			devolucion = crearDir(mensaje->path);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			if (devolucion == -2)
				mensaje->protolo = ERROREDQUOT;
			if (devolucion == -3)
				mensaje->protolo = ERRORENAMETOOLONG;
			mensaje->tamano = 0;
			break;
		case BORRAR:
			devolucion = borrar(mensaje->path);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			mensaje->tamano = 0;
			break;
		case BORRARDIR:
			devolucion = borrarDir(mensaje->path);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			mensaje->tamano = 0;
			break;
		case RENOMBRAR:
			mensaje->path[mensaje->path_payload] = '\0';
			mensaje->buffer[mensaje->tamano] = '\0';
			devolucion = renombrar(mensaje->path, mensaje->buffer);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			if (devolucion == -3)
				mensaje->protolo = ERRORENAMETOOLONG;
			mensaje->tamano = 0;
			break;
		case LEERDIR:
			var = 0;
			puntero = readAttr(mensaje->path, &var);
			mensaje->tamano = var * 17;
			mensaje->buffer = malloc(mensaje->tamano);
			memcpy(mensaje->buffer, puntero, mensaje->tamano);
			devolucion = 1;
			mensaje->protolo = SLEERDIR;
			mensaje->tamano = var * 17;
			break;
		case GETATTR:
			mensaje->protolo = SGETATTR;
			if ((strcmp(mensaje->path, "/") != 0)) {
				devolucion16 = getAttr(mensaje->path);
				if (devolucion16 != DIRMONTAJE) {
					mensaje->tipoArchivo = estadoEnum(devolucion16);
					mensaje->tamano =
							tablaDeArchivos[devolucion16].tamanioArchivo;
					devolucion16 = 1;
				} else {
					mensaje->protolo = ERROR;
				}
			} else {
				mensaje->tipoArchivo = 2;
				devolucion16 = 1;
			}
			break;
		default:
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			log_error(log, "No te entendi wacho");
		}
		if (devolucion == -1)
			mensaje->protolo = ERROR;
		enviarMensaje(CLIENTE_SERVIDOR, socket, (void *) mensaje);
	}
}
void levantarHeader() {
	log_trace(log, "Levantando Header");
	memcpy(&fileHeader, data, sizeof(osadaHeader));
	log_trace(log, "Identificador: %s", fileHeader.identificador);
	log_trace(log, "Version: %d", fileHeader.version);
	log_trace(log, "Fs_blocks: %u", fileHeader.fs_blocks);
	log_trace(log, "Bitmap_blocks: %u", fileHeader.bitmap_blocks);
	log_trace(log, "Tabla_de_archivos: 1024");
	log_trace(log, "Inicio de tabla de asignaciones: %u",
			fileHeader.inicioTablaAsignaciones);
	log_trace(log, "Bloques de datos: %u", fileHeader.data_blocks);
}

uint32_t bitmapOcupados() {
	int i;
	uint32_t ocupados = 0;
	for (i = 0; i < fileHeader.data_blocks; ++i) {
		if (bitarray_test_bit(bitmap, i))
			ocupados++;
	}
	return ocupados;
}
int tamTAsignacion() {
	return (fileHeader.fs_blocks - fileHeader.data_blocks
			- fileHeader.inicioTablaAsignaciones) * BLOCK_SIZE;
}

void levantarOsada() {
	log_trace(log, "Levantando OSADA");
	levantarHeader();
	int puntero = BLOCK_SIZE;
	tablaDeArchivos = malloc(1024 * BLOCK_SIZE);
	tablaDeAsignaciones = malloc(tamTAsignacion());
	bloquesDeDatos = malloc(fileHeader.data_blocks * BLOCK_SIZE);
	bitmap = bitarray_create_with_mode(data + BLOCK_SIZE,
			fileHeader.bitmap_blocks * BLOCK_SIZE, LSB_FIRST);
	log_trace(log, "Cantidad de bloques ocupados %d", bitmapOcupados());
	puntero += fileHeader.bitmap_blocks * BLOCK_SIZE;
	memcpy(tablaDeArchivos, data + puntero, 1024 * BLOCK_SIZE);
	puntero = fileHeader.inicioTablaAsignaciones * BLOCK_SIZE;
	memcpy(tablaDeAsignaciones, data + puntero, tamTAsignacion());
	puntero = (fileHeader.fs_blocks - fileHeader.data_blocks) * BLOCK_SIZE;
	memcpy(bloquesDeDatos, data + puntero, fileHeader.data_blocks * BLOCK_SIZE);
}

int archivoDirectorio(int i) {
	if (tablaDeArchivos[i].estado == ARCHIVO
			|| tablaDeArchivos[i].estado == DIRECTORIO)
		return 1;

	return 0;
}

void imprimirDirectoriosRecursivo(int archivo, int nivel) {
	int i;
	int aux;
	for (i = 0; i < 2048; i++) {
		if (archivo == i)
			continue;
		if (archivoDirectorio(i) && tablaDeArchivos[i].bloquePadre == archivo) {
			char * guionParaLosPutos = string_repeat('-', nivel);
			log_debug(log, "%s %s -- %s", guionParaLosPutos,
					tablaDeArchivos[i].nombreArchivo, tipoArchivo(i));
			if (tablaDeArchivos[i].estado == DIRECTORIO) {
				aux = nivel + 1;
				imprimirDirectoriosRecursivo(i, aux);
			}
		}
	}
}
void mostrarTablaDeArchivos() {
	int i;
	log_trace(log, "Imprimiendo tabla de archivos");
	for (i = 0; i < 2048; i++) {
		if (archivoDirectorio(i)) {
			if (strcmp(tablaDeArchivos[i].nombreArchivo, "metadata") == 0) {
				memcpy(tablaDeArchivos[i].nombreArchivo, "metadata.txt",
						strlen("metadata.txt"));
			}
			log_trace(log,
					"%d|NOMBRE: %s|TIPO: %s|BLOQUE PADRE: %d|BLOQUE INICIAL: %d",
					i, tablaDeArchivos[i].nombreArchivo, tipoArchivo(i),
					tablaDeArchivos[i].bloquePadre,
					tablaDeArchivos[i].bloqueInicial);
		}
	}

}
void mostrarTablaDeAsignacion() {
	t_log * asignacion = log_create("asignacion.txt", "Osada", 1, 0);
	int i;
	log_trace(asignacion, "ID|||SIG");
	for (i = 0; i < tamTAsignacion(); i++) {
		log_trace(asignacion, "%d|||%d", i, tablaDeAsignaciones[i]);
	}
}

void imprimirArbolDeDirectorios() {
	log_trace(log, "Mostrando arbol de directorio");
	int i;
	for (i = 0; i < 2048; i++) {
		if (archivoDirectorio(i) && tablaDeArchivos[i].bloquePadre == DIRMONTAJE) {
			log_debug(log, "%s - %s", tablaDeArchivos[i].nombreArchivo,
					tipoArchivo(i));
			if (tablaDeArchivos[i].estado == DIRECTORIO)
				imprimirDirectoriosRecursivo(i, 1);
		}
	}
}

void mapearMemoria(char * nombreBin) {
	osadaFile =
			open(
					string_from_format(
							"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/PokedexServidor/Debug/%s",
							nombreBin), O_RDWR);
	struct stat s;
	fstat(osadaFile, &s);
	int size = s.st_size;
	log_trace(log, "Tamaño del disco: %d Kb", size / 1024);
	data = malloc(size);
	if ((data = (char*) mmap(0, size, PROT_READ | PROT_WRITE,
	MAP_SHARED, osadaFile, 0)) == -1)
		log_trace(log, "la estamos cagando");
	log_trace(log, "Memoria mapeada");
}
void sincronizarMemoria() {
	memcpy(data, &fileHeader, BLOCK_SIZE);
	int puntero = BLOCK_SIZE;
	pthread_mutex_lock(&sem_bitarray);
	memcpy(data + BLOCK_SIZE, bitmap->bitarray,
			fileHeader.bitmap_blocks * BLOCK_SIZE);
	pthread_mutex_unlock(&sem_bitarray);
	puntero += fileHeader.bitmap_blocks * BLOCK_SIZE;
	pthread_mutex_lock(&sem_tablaDeArchivos);
	memcpy(data + puntero, tablaDeArchivos, 1024 * BLOCK_SIZE);
	pthread_mutex_unlock(&sem_tablaDeArchivos);
	puntero = fileHeader.inicioTablaAsignaciones * BLOCK_SIZE;
	pthread_mutex_lock(&sem_bitarray);
	memcpy(data + puntero, tablaDeAsignaciones, tamTAsignacion());
	pthread_mutex_unlock(&sem_bitarray);
	puntero = (fileHeader.fs_blocks - fileHeader.data_blocks) * BLOCK_SIZE;
	pthread_mutex_lock(&sem_memory);
	memcpy(data + puntero, bloquesDeDatos, fileHeader.data_blocks * BLOCK_SIZE);
	pthread_mutex_unlock(&sem_memory);
}

void funcionAceptar() {
}

int main(int argc, void *argv[]) {

	log = log_create("log", "Osada", 1, 0);
	mapearMemoria(argv[2]);
	levantarOsada();
	imprimirArbolDeDirectorios();
	mostrarTablaDeArchivos();
	pthread_mutex_init(&sem_bitarray, NULL);
	pthread_mutex_init(&sem_memory, NULL);
	pthread_mutex_init(&sem_tablaDeArchivos, NULL);
	int listener = crearSocketServidor(argv[1]);
	while (1) {

		struct sockaddr_storage remoteaddr;
		socklen_t addrlen;
		addrlen = sizeof remoteaddr;
		int newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);
		pthread_t hiloCliente;
		pthread_create(&hiloCliente, NULL, atenderPeticiones, (void *) newfd);
	}
	log_destroy(log);
	free(tablaDeArchivos);
	free(tablaDeAsignaciones);
	bitarray_destroy(bitmap);
	return 0;
}
