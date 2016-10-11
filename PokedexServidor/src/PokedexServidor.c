/*
 ============================================================================
 Averiguar el formato de fechas
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <commons/bitarray.h>
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

#define BLOCK_SIZE 64

typedef enum
	__attribute__((packed)) { // wtf ???
		BORRADO = '\0',
	ARCHIVO = '\1',
	DIRECTORIO = '\2',
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
int B;
int N;
int A;
int X;
int F;
int string_contains(const char *path, char letra) {
	int teta;
	for (teta = 0; path[teta] != '\0'; teta++) {
		if (path[teta] == letra)
			return 1;
	}
	return 0;
}
void borrarPrimeraLetra(char * palabra) { // En caso de que haya un /123 comparando
	int i = 1;
	while (palabra[i]) {
		palabra[i - 1] = palabra[i];
		i++;
	}
}

int buscarNroTArchivos(char ** path) { // En caso de archivo
	int i;
	uint16_t padre = -1;
	int contador = 0;
	while (!string_contains(path[contador], '.')) {
		for (i = 0; i < 2048; i++) {
			if ((tablaDeArchivos[i].estado == DIRECTORIO)
					&& (padre == tablaDeArchivos[i].bloquePadre)) {
//				log_trace(log,"tabla %s",tablaDeArchivos[i].nombreArchivo);
//				log_trace(log,"path %s",path[contador]);
//				log_trace(log,"padre: %u",padre);
				if (strcmp(tablaDeArchivos[i].nombreArchivo, path[contador])
						== 0) {
					padre = tablaDeArchivos[i].bloqueInicial;
					contador++;
					break;
				}
			}
		}
	}
	for (i = 0; i < 2048; i++) {
		if (tablaDeArchivos[i].estado == ARCHIVO) {
			if (padre == tablaDeArchivos[i].bloquePadre) {
				if (string_equals_ignore_case(tablaDeArchivos[i].nombreArchivo,
						path[contador])) {
					log_trace(log, "Encontre el archivo: %d", i);
					return i;
				}
			}
		}
	}
	log_error(log, "No encontre el archivo");
	return -1;
}
archivos_t * obtenerArchivo(char * path) {
	char ** ruta = string_split(path, "/");
	int numeroDeArchivo = buscarNroTArchivos(ruta);
	if (numeroDeArchivo == -1)
		log_error(log, "No se encontro el archivo");
	return &tablaDeArchivos[numeroDeArchivo];
}
int buscarBloqueLibre() {
	int i;
	for (i = 0; i < fileHeader.fs_blocks; ++i) {
		if (bitarray_test_bit(bitmap, i) == 0) {
			bitarray_set_bit(bitmap, i);
			return i;
		}
	}
	return -1;
}
int asignarBloqueLibre(int *bloqueAnterior) {
	int bloqueLibre = buscarBloqueLibre();
	log_trace(log, "Bloque libre a ocupar: %d", bloqueLibre);
	if (bloqueLibre == -1) {
		return -1;
	}
	tablaDeAsignaciones[*bloqueAnterior] = bloqueLibre;
	return 0;
}
int verificarBloqueSiguiente(int *actual, int *siguiente) {
	if (tablaDeAsignaciones[*siguiente] == 0) {
		log_trace(log, "No hay un bloque siguiente");
		if (asignarBloqueLibre(actual) == -1)
			return -1;
	}
	log_trace(log, "bloque siguiente: %d", *siguiente);
	*actual = tablaDeAsignaciones[*actual];
	*siguiente = tablaDeAsignaciones[*actual];
	return 0;
}
uint16_t buscarAlPadre(char *path) { // Del ultimo directorio sirve Directorios
	int i = 0;
	int j = 0;
	uint16_t padre = -1;
	char ** ruta = string_split(path, "/");
	while (ruta[i]) {
		if (tablaDeArchivos[j].bloquePadre == padre
				&& (strcmp(tablaDeArchivos[j].nombreArchivo, ruta[i]) == 0)
				&& tablaDeArchivos[j].estado == DIRECTORIO) {
			padre = tablaDeArchivos[j].bloqueInicial;
			i++;
			j = 0;
		} else {
			j++;
		}
	}
	return padre;
}
int verificarSiExiste(char * path, osada_file_state estado) {
	char ** ruta = string_split(path, "/");
	int i = 0;
	int j;
	uint16_t padre = -1;
	int bandera = 1;
	while (ruta[i] && bandera) {
		bandera = 0;
		for (j = 0; j < 2048; j++) {
			if (ruta[i + 1]) {
				if (tablaDeArchivos[j].nombreArchivo == ruta[i]
						&& tablaDeArchivos[j].estado == estado) {
					return 1;
				}
			} else {
				if (tablaDeArchivos[j].nombreArchivo == ruta[i]
						&& tablaDeArchivos[j].estado == DIRECTORIO) {
					bandera = 1;
					i++;
					padre = tablaDeArchivos[j].bloqueInicial;
				}
			}
			if (j == 1023 && bandera == 0) {
				return -1;
			}
		}
	}
	return -1;
}

char * leerArchivo(char * path, int tamano, int offset) {
	log_info(log, "Se va a leer el archivo: %s", path);
	archivos_t * archivo = obtenerArchivo(path);
	if (archivo->tamanioArchivo + offset < tamano) {
		log_error(log, "Se va a leer basura");
		return NULL;
	}
	char * lectura = malloc(tamano);
	int bloqueSiguiente = archivo->bloqueInicial;
	int puntero = 0;
	while (offset > BLOCK_SIZE) { // Muevo al nodo que tenga algo para leer
		if (bloqueSiguiente == 0) {
			log_trace(log, "ERROR");
			return NULL;
		}
		bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
		offset -= BLOCK_SIZE;
	}
	if (tamano > BLOCK_SIZE - offset) { // Leo el contenido del primer nodo a partir offset
		memcpy(lectura, bloquesDeDatos + bloqueSiguiente * BLOCK_SIZE + offset,
		BLOCK_SIZE - offset);
		tamano = tamano - (BLOCK_SIZE - offset);
		puntero = BLOCK_SIZE - offset;
		bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
	} else {  // Si es menor de lo que le queda al nodo
		memcpy(lectura,
				bloquesDeDatos + (bloqueSiguiente * BLOCK_SIZE) + offset,
				tamano);
		return lectura;
	}
	while (tamano > 0) { // prosigo con la lectura si hay que leer mas
		if (tamano > BLOCK_SIZE) {
			memcpy(lectura + puntero,
					bloquesDeDatos + bloqueSiguiente * BLOCK_SIZE,
					BLOCK_SIZE);
			tamano -= BLOCK_SIZE;
			puntero += BLOCK_SIZE;
			bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
		} else {
			memcpy(lectura + puntero,
					bloquesDeDatos + bloqueSiguiente * BLOCK_SIZE, tamano);
			tamano = 0;
		}
	}
	return lectura;
}
int escribirArchivo(char * path, char * buffer, int offset) {
	log_info(log, "Escribiendo en: %s ,contenido:%s ", path, buffer);
	archivos_t * archivo = malloc(sizeof(archivos_t));
	archivo = obtenerArchivo(path);
	int aux = offset;
	int tam = strlen(buffer);
	int puntero = 0;
	int contador = 0;
	int bloqueActual = archivo->bloqueInicial;
	int bloqueSiguiente = tablaDeAsignaciones[bloqueActual];
	if (bloqueActual == -1) {
		log_error(log, "la cagamo amigo, nada para ver");
		return -1;
	}
	log_trace(log, "BLoque al cual se va a escribir: %d", bloqueActual);

	while (offset > BLOCK_SIZE) { // Muevo al nodo que tenga algo para escribir
		if (verificarBloqueSiguiente(&bloqueActual, &bloqueSiguiente) == -1)
			return -1;
		offset -= BLOCK_SIZE;
	}
	if (tam > BLOCK_SIZE - offset) {
		memcpy(bloquesDeDatos + bloqueActual * 64 + offset, buffer,
		BLOCK_SIZE - offset);
		puntero += BLOCK_SIZE - offset;
		if (verificarBloqueSiguiente(&bloqueActual, &bloqueSiguiente) == -1)
			return -1;

	} else {
		memcpy(bloquesDeDatos + bloqueActual * 64 + offset, buffer, tam);
		if (archivo->tamanioArchivo < tam + aux)
			archivo->tamanioArchivo = tam + aux;
		free(archivo);
		return 0;
	}
	while (tam > puntero) {
		if ((tam - puntero) >= BLOCK_SIZE) {
			memcpy(bloquesDeDatos + bloqueActual * 64, buffer + puntero,
			BLOCK_SIZE);
			puntero += BLOCK_SIZE;
			if (verificarBloqueSiguiente(&bloqueActual, &bloqueSiguiente) == -1)
				return -1;
		} else {
			memcpy(bloquesDeDatos + bloqueActual * 64, buffer + puntero,
					tam - puntero);
			log_trace(log, "Se ha escrito exitosamente bytes: %d", tam);
			if (archivo->tamanioArchivo < tam + aux)
				archivo->tamanioArchivo = aux + tam;
			return 0;
		}
	}
	free(archivo);
	sincronizarMemoria();
	return 0;

}
int crearArchivo(char * path, char * nombre) {
	log_trace(log, "creando archivo  %s", nombre);
	int i = 0;
	uint16_t padre = -1;
	if (path != "/") {
		padre = buscarAlPadre(path); // hay que verificar esto si devuelve que no tiene padre no deberia.
	}
	for (i = 0; i < 2048; ++i) {
		if (tablaDeArchivos[i].estado == BORRADO) {
			tablaDeArchivos[i].bloqueInicial = buscarBloqueLibre();
			tablaDeArchivos[i].bloquePadre = padre;
			tablaDeArchivos[i].estado = ARCHIVO;
			tablaDeArchivos[i].fechaUltimaModif = 0; // Emmm fechas ?
			if (nombre[0] == "/")
				borrarPrimeraLetra(nombre);
			strcpy(tablaDeArchivos[i].nombreArchivo, nombre);
			tablaDeArchivos[i].tamanioArchivo = 1;
			break;
		}
	}
	log_trace(log, "se ha creado un archivo en el bloque: %d, padre: %u",
			tablaDeArchivos[i].bloqueInicial, padre);
	sincronizarMemoria();
	return 1;
}
int borrar(char * path) {
	log_trace(log, "Borrar archivo  %s", path);

	char ** ruta = string_split(path, "/");
	archivos_t * archivo;
	int i = 0;
	int j = 0;
	uint16_t padre = -1;
	while (j > -1) {
		for (i = 0; i < 2048; i++) {
			//	log_trace(log, "Padre:%u", padre);
			//	log_trace(log, "Tabla:%s", tablaDeArchivos[i].nombreArchivo);
			//	log_trace(log, "Ruta:%s", ruta[j]);

			if ((padre == tablaDeArchivos[i].bloquePadre)
					&& (strcmp(tablaDeArchivos[i].nombreArchivo, ruta[j]) == 0)) {
				if (tablaDeArchivos[i].estado == ARCHIVO) {
					archivo = &tablaDeArchivos[i];
					j = -1000;
				}
				padre = tablaDeArchivos[i].bloqueInicial;
				j++;
				break;

			}

		}

	}
	while (archivo->bloqueInicial != 0) {
		j = archivo->bloqueInicial;
		bitarray_clean_bit(bitmap, j);
		archivo->bloqueInicial = tablaDeAsignaciones[j];
		tablaDeAsignaciones[j] = 0;
	}
	archivo->bloqueInicial = -1;
	archivo->bloquePadre = -1;
	archivo->estado = BORRADO;
	archivo->tamanioArchivo = 0;
	log_trace(log, "Se ah borrado el archivo: %s", archivo->nombreArchivo);
	sincronizarMemoria();
	return 0;
}
int crearDir(char * path) {
	log_info(log, "creando directorio");
	char ** ruta = string_split(path, "/");
	int i = 0;
	int j = 0;
	uint32_t padre = -1;
	if (path != "/" && (ruta[i + 1])) {
		while (ruta[i + 1]) {
			for (j = 0; j < 2048; ++j) {
				if ((tablaDeArchivos[j].estado == DIRECTORIO)
						&& (strcmp(tablaDeArchivos[j].nombreArchivo, ruta[i])
								== 0)) {
					padre = tablaDeArchivos[j].bloqueInicial;
					i++;
					j = 1025;
				}

			}
		}
	}
	uint32_t minion;
	for (j = 0; j < 2048; j++) {
		if (tablaDeArchivos[j].estado == BORRADO) {
			tablaDeArchivos[j].bloqueInicial = buscarBloqueLibre();
			tablaDeArchivos[j].bloquePadre = padre;
			minion = tablaDeArchivos[j].bloqueInicial;
			tablaDeArchivos[j].estado = DIRECTORIO;
			tablaDeArchivos[j].fechaUltimaModif = 0; // Emmm fechas ?
			strcpy(tablaDeArchivos[j].nombreArchivo, ruta[i]);
			tablaDeArchivos[j].tamanioArchivo = 1;
			break;
		}
	}
	log_trace(log, "Se ha creado el directorio: %s , padre; %u, inicial; %u",
			ruta[i], padre, minion);
	sincronizarMemoria();
	return 1;
}
int borrarDir(char * path) {
	log_info(log, "borrando directorio: %s", path);
	char ** ruta = string_split(path, "/");
	archivos_t * file;
	int i = 0;
	int j = 0;
	while (ruta[i]) {
		for (j = 0; j < 2048; j++) {
			if ((strcmp(ruta[i], tablaDeArchivos[j].nombreArchivo) == 0)
					&& tablaDeArchivos[j].estado == DIRECTORIO) {
				file = &tablaDeArchivos[j];
				i++;
				break;
			}
		}
	}
	for (j = 0; j < 2048; j++) {
		if (tablaDeArchivos[j].estado != BORRADO
				&& tablaDeArchivos[j].bloquePadre == file->bloqueInicial) {
			log_error(log, "TIene archivos adentro no puede ser borrado");
			return -1;
		}
	}
	file->bloqueInicial = -1;
	file->bloquePadre = -1;
	file->estado = BORRADO;
	file->tamanioArchivo = 0;
	log_trace(log, "Se ah borrado el directorio: %s", file->nombreArchivo);
	return 1;
}
int renombrar(char * path, char * nombre) {
	log_info(log, "Renombrando: %s por %s", path, nombre);

	char ** ruta = string_split(path, "/");
	int i = 0;
	int j = 0;
	uint16_t padre = -1;
	while (ruta[i + 1])
		for (j = 0; j < 2048; j++) {
			if ((padre == tablaDeArchivos[j].bloquePadre)
					&& tablaDeArchivos[j].estado == DIRECTORIO) {
				if (strcmp(tablaDeArchivos[j].nombreArchivo, ruta[i]) == 0) {
					padre = tablaDeArchivos[j].bloqueInicial;
					i++;
					break;
				}
			}

		}
	for (j = 0; j < 2048; j++) {
		if (padre == tablaDeArchivos[j].bloquePadre
				&& tablaDeArchivos[j].estado != BORRADO) {
			if (strcmp(tablaDeArchivos[j].nombreArchivo, ruta[i]) == 0) {
				log_trace(log, "Antiguo: %s", tablaDeArchivos[j].nombreArchivo);
				log_trace(log, "Nuevo: %s", nombre);
				memcpy(&tablaDeArchivos[j].nombreArchivo, nombre,
						strlen(nombre) + 1);
				return 1;
			}
		}
	}
	sincronizarMemoria();
	return -1;
}
char * readAttr(char *path, int *var) {
	char * lista = malloc(17);
	*var = 0;
	int i;
	int j;
	uint16_t padre = -1;
	if (path != "/") {
		char ** ruta = string_split(path, "/");
		for (i = 0; (i < 2048) && ruta[j]; i++) {
			if ((padre == tablaDeArchivos[i].bloquePadre)
					&& (tablaDeArchivos[i].estado == DIRECTORIO)
					&& (strcmp(tablaDeArchivos[i].nombreArchivo, ruta[j]) == 0)) {
				padre = tablaDeArchivos[i].bloqueInicial;
				i = 0;
				j++;
			}
		}
	}
	for (i = 0; i < 2048; i++) {
		if (padre == tablaDeArchivos[i].bloquePadre) {
			log_trace(log, "diganme que no da error aca");
			realloc(lista, *var * 17);
			memcpy(lista + (*var * 17), tablaDeArchivos[i].nombreArchivo, 17);
			var++;
		}
	}
	return lista;
}
osada_file_state getAttr(char *path) {
	int i;
	int j;
	uint16_t padre = -1;
	char ** ruta = string_split(path, "/");
	for (i = 0; (i < 2048) && ruta[j + 1]; i++) {
		if ((padre == tablaDeArchivos[i].bloquePadre)
				&& (tablaDeArchivos[i].estado == DIRECTORIO)
				&& (strcmp(tablaDeArchivos[i].nombreArchivo, ruta[j]) == 0)) {
			padre = tablaDeArchivos[i].bloqueInicial;
			i = 0;
			j++;
		}
	}
	for (i = 0; i < 2048; i++) {
		if ((strcmp(ruta[j], tablaDeArchivos[i].nombreArchivo) == 0)
				&& (tablaDeArchivos[i].bloquePadre = padre)) {
			return tablaDeArchivos[i].estado;
		}
	}
	return -1;
}

void atenderPeticiones(int socket) { // es necesario la ruta de montaje?
	mensaje_CLIENTE_SERVIDOR * mensaje;
	int devolucion = 1;
	int var;
	t_list * lista;
	while ((mensaje = (mensaje_CLIENTE_SERVIDOR *) recibirMensaje(socket))
			!= NULL) { // no esta echa el envio
		switch (mensaje->protolo) {
		case LEER:
			mensaje->buffer = leerArchivo(mensaje->path, mensaje->tamano,
					mensaje->offset);
			if (mensaje->buffer == NULL) {
				mensaje->protolo = ERROR;
			}
			mensaje->tamano = strlen(mensaje->buffer);
			break;
		case ESCRIBIR:
			devolucion = escribirArchivo(mensaje->path, mensaje->buffer,
					mensaje->offset);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			mensaje->tamano = 0;
			break;
		case CREAR:
			devolucion = crearArchivo(mensaje->path, mensaje->buffer);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			mensaje->tamano = 0;
			break;
		case CREARDIR:
			devolucion = crearDir(mensaje->path);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
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
			devolucion = renombrar(mensaje->path, mensaje->buffer);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			mensaje->tamano = 0;
			break;
		case LEERDIR:
			mensaje->buffer = readAttr(mensaje->path, &var);
			devolucion = 1;
			mensaje->tamano = var * 17;
			break;
		case GETATTR:
			mensaje->buffer =(char*) getAttr(mensaje->path);
			devolucion = 1;
			mensaje->tamano = sizeof(osada_file_state);
			break;
		default:
			if (devolucion == -1)
				mensaje->protolo = ERROR;
			log_error(log, "No te entendi wacho");
		}
		enviarMensaje(SERVIDOR_CLIENTE, socket, (void *) &mensaje);
	}
}

void levantarHeader() {
	log_trace(log, "Levantando Header");
	//read(&fileHeader, sizeof(osadaHeader), 1, osadaFile);
	memcpy(&fileHeader, data, sizeof(osadaHeader));
	//fileHeader.identificador[7] = '\0'; <-- pisa la version
	log_trace(log, "Identificador: %s", fileHeader.identificador);
	log_trace(log, "Version:%d", fileHeader.version);
	log_trace(log, "Fs_blocks:%u", fileHeader.fs_blocks);
	log_trace(log, "Bitmap_blocks:%u", fileHeader.bitmap_blocks);
	log_trace(log, "Tabla_de_archivos :1024");
	log_trace(log, "Inicio de tabla de asignaciones:%u",
			fileHeader.inicioTablaAsignaciones);
	log_trace(log, "Data_blocks:%u", fileHeader.data_blocks);
}

uint32_t bitmapOcupados() {
	int i;
	uint32_t ocupados = 0;
	for (i = 0; i < N * BLOCK_SIZE * 8; ++i) {
		if (bitarray_test_bit(bitmap, i))
			ocupados++;
	}
	return ocupados;
}

void levantarOsada() {
	log_trace(log, "Levantando osada");
	int i;
	levantarHeader();
	B = BLOCK_SIZE;
	F = fileHeader.fs_blocks;
	N = fileHeader.fs_blocks / 8 / B;
	A = ((F - 1 - N - 1024) * 4) / B;
	X = F - 1 - N - 1024 - A;
	bitmap = bitarray_create(data + 64, N * B);
	tablaDeArchivos = malloc(1024 * B);
	tablaDeAsignaciones = malloc(A * B);
	bloquesDeDatos = malloc(X * B);
	//memcpy(bitmap, data + B, N * B);
	uint32_t ocupados = bitmapOcupados();
	log_trace(log, "ocupados: %u", ocupados);
	log_trace(log, "Bitmap: Libres: %u    Ocupados:%u",
			fileHeader.fs_blocks - ocupados, ocupados);
	log_trace(log, "tamaño tabla asignacion: %d en bloques", A);
	memcpy(tablaDeArchivos, data + B + N * B, 1024 * B);
	memcpy(tablaDeAsignaciones, data + (B + N + 1024) * B, A);
	memcpy(bloquesDeDatos, data + (B + N + 1024 + A) * B, X);
}

char * tipoArchivo(int i) {
	switch (tablaDeArchivos[i].estado) {
	case (BORRADO):
		return "BORRADO";
		break;
	case (ARCHIVO):
		return "ARCHIVO";
		break;
	case (DIRECTORIO):
		return "DIRECTORIO";
		break;
	default:
		return "SARASA";
	}
}
int archivoDirectorio(int i) {
	if (tablaDeArchivos[i].estado == ARCHIVO)
		return 1;
	if (tablaDeArchivos[i].estado == DIRECTORIO)
		return 1;
	return 0;
}

void imprimirArchivosDe(char* path) {
	log_trace(log, "Archivos de: %s", path);
	archivos_t archivito = tablaDeArchivos[buscarAlPadre(path)];
	int i;
	for (i = 0; i < 2048; ++i) {
		if (tablaDeArchivos[i].estado == ARCHIVO) {
			log_trace(log, "%s", tablaDeArchivos[i].nombreArchivo);
		}
	}
}
void imprimirDirectoriosRecursivo(archivos_t archivo, int nivel, uint16_t padre) {
	int i;
	int aux;
	for (i = 0; i < 2048; i++) {
		if (archivoDirectorio(i) && tablaDeArchivos[i].bloquePadre == padre) {
			char * coshita = string_repeat('-', nivel);
			log_debug(log, "%s %s -- %s", coshita,
					tablaDeArchivos[i].nombreArchivo, tipoArchivo(i));
			if (tablaDeArchivos[i].estado == DIRECTORIO) {
				aux = nivel + 1;
				imprimirDirectoriosRecursivo(tablaDeArchivos[i], aux,
						tablaDeArchivos[i].bloqueInicial);
			}
		}
	}
}
void mostrarTablaDeArchivos() {
	int i;
	for (i = 0; i < 2048; i++) {
		if (archivoDirectorio(i))
			log_trace(log, "%d . %s . %s", i, tablaDeArchivos[i].nombreArchivo,
					tipoArchivo(i));
	}
}

void imprimirArbolDeDirectorios() {
	log_trace(log, "Mostrando arbol de directorio");
	int i;
	uint16_t padre = -1;
	for (i = 0; i < 2048; i++) {
		if (tablaDeArchivos[i].estado != BORRADO
				&& tablaDeArchivos[i].bloquePadre == padre) {
			log_debug(log, "%s - %s", tablaDeArchivos[i].nombreArchivo,
					tipoArchivo(i));
			if (tablaDeArchivos[i].estado == DIRECTORIO)
				imprimirDirectoriosRecursivo(tablaDeArchivos[i], 1,
						tablaDeArchivos[i].bloqueInicial);
		}
	}
}

void mapearMemoria(char * nombreBin) {
	osadaFile =
			open(string_from_format(
					"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/PokedexServidor/Debug/%s", nombreBin),O_RDWR);
	struct stat s;
	int status = fstat(osadaFile, &s);
	int size = s.st_size;
	log_trace(log, "%d", size);
	data = malloc(size);
	if ((data = (char*) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			osadaFile, 0)) == -1)
		log_trace(log, "la estamos cagando");

	log_trace(log, "memoria mapeada");
}
void sincronizarMemoria() {
	memcpy(data, &fileHeader, BLOCK_SIZE);
	memcpy(data + BLOCK_SIZE, bitmap, N);
	memcpy(data + (1 + N) * BLOCK_SIZE, tablaDeArchivos, 1024 * BLOCK_SIZE);
	memcpy(data + (1025 + N) * BLOCK_SIZE, tablaDeAsignaciones, A);
	memcpy(data + (1025 + N + A) * BLOCK_SIZE, bloquesDeDatos, X);
	log_trace(log, "Memoria sincronizada");

}

void funcionAceptar() {
}
int main(int argc, void *argv[]) {
	log = log_create("log", "Osada", 1, 0);
	mapearMemoria(argv[2]);
	levantarOsada();
	sincronizarMemoria();
	theMinionsRevengeSelect(argv[1], funcionAceptar, atenderPeticiones);
	free(log);
	free(tablaDeArchivos);
	free(tablaDeAsignaciones);
	free(bitmap);
	return 0;
}

