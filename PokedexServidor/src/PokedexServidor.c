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
#include <time.h>

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

int buscar(uint16_t padre, char * nombre, osada_file_state tipo) {
	int i;
	for (i = 0; i < 2048; i++) {
		if ((padre == tablaDeArchivos[i].bloquePadre)
				&& (tablaDeArchivos[i].estado == tipo)
				&& (strcmp(tablaDeArchivos[i].nombreArchivo, nombre) == 0)) {
			return i;
		}
	}
	return -1;
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
uint32_t estadoEnum(int i) {
	switch (tablaDeArchivos[i].estado) {
	case (BORRADO):
		return 0;
		break;
	case (ARCHIVO):
		return 1;
		break;
	case (DIRECTORIO):
		return 2;
		break;
	default:
		return -1;
	}
}
int buscarBloqueLibre() {
	int i;
	int aux;
	for (i = X; i < fileHeader.fs_blocks; ++i) {
		aux = bitarray_test_bit(bitmap, i);
		if (aux == 0) {
			bitarray_set_bit(bitmap, i);
			tablaDeAsignaciones[i] = -1;
			return i;
		}
	}
	log_trace(log, "No hay memoria suficiente");
	return -1;
}
int asignarBloqueLibre(int bloqueAnterior) {
	int bloqueLibre = buscarBloqueLibre();
	log_trace(log, "Bloque libre a ocupar: %d", bloqueLibre);
	if (bloqueLibre == -1) {
		return -1;
	}
	tablaDeAsignaciones[bloqueAnterior] = bloqueLibre;
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
	for (j = 0; j < 2048 && ruta[i + 1]; j++) {
		if (tablaDeArchivos[j].bloquePadre == padre
				&& (strcmp(tablaDeArchivos[j].nombreArchivo, ruta[i]) == 0)
				&& tablaDeArchivos[j].estado == DIRECTORIO) {
			padre = tablaDeArchivos[j].bloqueInicial;
			i++;
			j = 0;
		}
	}
	if (ruta[i + 1])
		return -2;
	return padre;
}
int verificarSiExiste(char * path, osada_file_state tipo) {
	char ** ruta = string_split(path, "/");
	int i;
	int j = 0;
	uint16_t padre = -1;
	int file;
	while (ruta[j + 1]) {
		file = buscar(padre, ruta[j], DIRECTORIO);
		if (file == -1)
			return -1;
		padre = tablaDeArchivos[file].bloqueInicial;
		j++;
	}
	return buscar(padre, ruta[j], tipo);
}
int existeUnoIgual(uint16_t padre, char* nombre, osada_file_state tipo) {
	int i;
	for (i = 0; i < 2048; i++) {
		if ((tablaDeArchivos[i].bloquePadre == padre)
				&& (strcmp(tablaDeArchivos[i].nombreArchivo, nombre) == 0)
				&& (tablaDeArchivos[i].estado == tipo)) {
			return 1;
		}
	}
	return 0;
}
int ingresarEnLaTArchivos(uint16_t padre, char *nombre, osada_file_state tipo) {
	int i;
	for (i = 0; i < 2048; i++) {
		if (tablaDeArchivos[i].estado == BORRADO) {
			tablaDeArchivos[i].bloqueInicial = buscarBloqueLibre();
			tablaDeArchivos[i].bloquePadre = padre;
			tablaDeArchivos[i].estado = tipo;
			tablaDeArchivos[i].fechaUltimaModif = 0; // Emmm fechas ?
			memcpy(tablaDeArchivos[i].nombreArchivo, nombre, 17);
			tablaDeArchivos[i].tamanioArchivo = 0;
			tablaDeAsignaciones[tablaDeArchivos[i].bloqueInicial] = -1;
			return i;
		}
	}
	return -1;
}

char * leerArchivo(char * path, int *aux) {
	log_info(log, "Se va a leer el archivo: %s", path);
	int file = verificarSiExiste(path, ARCHIVO);
	if (file == -1)
		return NULL;
	char * lectura = malloc(tablaDeArchivos[file].tamanioArchivo);
	int bloqueSiguiente = tablaDeArchivos[file].bloqueInicial;
	int puntero = 0;
	while (puntero != tablaDeArchivos[file].tamanioArchivo) {
		if ((tablaDeArchivos[file].tamanioArchivo - puntero) > BLOCK_SIZE) {
			memcpy(lectura + puntero, &bloquesDeDatos[bloqueSiguiente],
			BLOCK_SIZE);
			puntero += BLOCK_SIZE;
			bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
			if (bloqueSiguiente == -1)
				break;
		} else {
			memcpy(lectura + puntero, &bloquesDeDatos[bloqueSiguiente],
					tablaDeArchivos[file].tamanioArchivo - puntero);
			puntero += tablaDeArchivos[file].tamanioArchivo - puntero;
		}
	}
	*aux = puntero;
	return lectura;
}
int escribirArchivo(char * path, char * buffer, int offset) {
	log_info(log, "Escribiendo en: %s ,contenido:%s ", path, buffer);
	int file = verificarSiExiste(path, ARCHIVO);
	if (file == -1)
		return -1;
	int aux = 0;
	int tam = strlen(buffer);
	int puntero = 0; // Para saber el tamaño
	int bloqueActual = tablaDeArchivos[file].bloqueInicial;
	if (bloqueActual == -1)
		tablaDeArchivos[file].bloqueInicial = buscarBloqueLibre();
	bloqueActual = tablaDeArchivos[file].bloqueInicial;
	log_trace(log, "BLoque a escribir: %d", bloqueActual);
	while (offset != puntero) {
		if (tablaDeAsignaciones[bloqueActual] == -1) {
			if (asignarBloqueLibre(bloqueActual) == -1)
				return -1;
		}
		bloqueActual = tablaDeAsignaciones[bloqueActual];
		if ((offset - puntero) > BLOCK_SIZE) {
			puntero += BLOCK_SIZE;
		} else {
			aux = offset - puntero;
			puntero = offset;
		}
	}
	puntero = 0;
	if (tam > (BLOCK_SIZE - aux)) {
		memcpy(&bloquesDeDatos[bloqueActual] + aux, buffer,
		BLOCK_SIZE - aux);
		tam -= (BLOCK_SIZE - aux);
		puntero = BLOCK_SIZE - aux;
	} else {
		memcpy(&bloquesDeDatos[bloqueActual] + aux, buffer, tam);
	}
	tam = 0;
	while (tam != 0) {
		if (tablaDeAsignaciones[bloqueActual] == -1) {
			if (asignarBloqueLibre(bloqueActual) == -1) {
				tam = 0;
				break;
			}
		}
		bloqueActual = tablaDeAsignaciones[bloqueActual];
		if (tam > BLOCK_SIZE) {
			memcpy(&bloquesDeDatos[bloqueActual], buffer + puntero,
			BLOCK_SIZE);
			puntero += BLOCK_SIZE;
			tam -= BLOCK_SIZE;
		} else {
			memcpy(&bloquesDeDatos[bloqueActual], buffer + puntero, tam);
			puntero += BLOCK_SIZE;
			tam = 0;
		}
	}
	tablaDeArchivos[file].tamanioArchivo = strlen(buffer) + offset;
	sincronizarMemoria();
	return tablaDeArchivos[file].tamanioArchivo;

}
int crearArchivo(char * path) {
	log_trace(log, "creando archivo  %s", path);
	int i = 0;
	int j = 0;
	uint16_t padre = -1;
	if (path != "/") {
		padre = buscarAlPadre(path);
	}
	if (padre == -2)
		return -1;
	char ** ruta = string_split(path, "/");
	while (ruta[j + 1]) {
		j++;
	}
	if (existeUnoIgual(padre, ruta[j], ARCHIVO))
		return -1;
	i =ingresarEnLaTArchivos(padre, ruta[j], ARCHIVO);
	log_trace(log, "se ha creado un archivo en el bloque: %u, padre: %u",
			tablaDeArchivos[i].bloqueInicial, padre);
	sincronizarMemoria();
	return 1;
}
int borrar(char * path) {
	log_trace(log, "Borrar archivo  %s", path);
	int j;
	int file = verificarSiExiste(path, ARCHIVO);
	if (file != -1) {
		while (tablaDeArchivos[file].bloqueInicial != 0) {
			j = tablaDeArchivos[file].bloqueInicial;
			bitarray_clean_bit(&bitmap, j);
			tablaDeArchivos[file].bloqueInicial = tablaDeAsignaciones[j];
			tablaDeAsignaciones[j] = -1;
		}
		tablaDeArchivos[file].bloqueInicial = -1;
		tablaDeArchivos[file].bloquePadre = -1;
		tablaDeArchivos[file].estado = BORRADO;
		tablaDeArchivos[file].tamanioArchivo = 0;
		log_trace(log, "Se ah borrado el archivo: %s",
				tablaDeArchivos[file].nombreArchivo);
		sincronizarMemoria();
		return 1;
	}
	return -1;
}
int crearDir(char * path) {
	log_info(log, "creando directorio");
	char ** ruta = string_split(path, "/");
	int i = 0;
	int j = 0;
	uint16_t padre = -1;
	if (path != "/" && (ruta[i + 1])) {
		while (ruta[i + 1]) {
			for (j = 0; j < 2048; ++j) {
				if ((tablaDeArchivos[j].estado == DIRECTORIO)
						&& (strcmp(tablaDeArchivos[j].nombreArchivo, ruta[i])
								== 0)) {
					padre = tablaDeArchivos[j].bloqueInicial;
					i++;
					break;
				}
			}
		}
	}
	if (existeUnoIgual(padre, ruta[i], DIRECTORIO))
		return -1;
	i= ingresarEnLaTArchivos(padre, ruta[i], DIRECTORIO);
	log_trace(log, "Se ha creado el directorio");
	sincronizarMemoria();
	return 1;
}
int borrarDir(char * path) {
	log_info(log, "borrando directorio: %s", path);
	int file = verificarSiExiste(path, DIRECTORIO);
	int j;
	if (file != -1) {
		for (j = 0; j < 2048; j++) {
			if (archivoDirectorio(j)
					&& tablaDeArchivos[j].bloquePadre
							== tablaDeArchivos[file].bloqueInicial) {
				log_error(log, "TIene archivos adentro no puede ser borrado");
				return -1;
			}
		}
	}
	tablaDeArchivos[file].bloqueInicial = -1;
	tablaDeArchivos[file].bloquePadre = -1;
	tablaDeArchivos[file].estado = BORRADO;
	tablaDeArchivos[file].tamanioArchivo = 0;
	log_trace(log, "Se ah borrado el directorio: %s",
			tablaDeArchivos[file].nombreArchivo);
	sincronizarMemoria();
	return 1;
}
int renombrar(char * path, char * nombre) {
	log_info(log, "Renombrando: %s por %s", path, nombre);

	int file = verificarSiExiste(path, ARCHIVO);
	if (file == -1)
		file = verificarSiExiste(path, DIRECTORIO);
	if (file != -1) {
		log_trace(log, "Antiguo: %s", tablaDeArchivos[file].nombreArchivo);
		log_trace(log, "Nuevo: %s", nombre);
		memcpy(&tablaDeArchivos[file].nombreArchivo, nombre,
				strlen(nombre) + 1);
		//tablaDeArchivos[file].nombreArchivo[strlen(nombre) +1] = "/0";
		sincronizarMemoria();
		return 1;
	}
	return -1;
}
char * readAttr(char *path, int *var) {
	log_debug(log, "readAttr: %s", path);
	char * lista;
	int aux = 0;
	int file =-1;
	int j = 0;
	int i;
	uint16_t padre = -1;
	if (strcmp(path, "/") != 0) {
		char ** ruta = string_split(path, "/");
		while (ruta[j]) {
			file = buscar(padre, ruta[j], DIRECTORIO);
			if (file == -1)
				return NULL;
			padre = tablaDeArchivos[file].bloqueInicial;
			j++;
		}
	}
	for (i = 0; i < 2048; i++) {
		if ((padre == tablaDeArchivos[i].bloquePadre)
				&& (archivoDirectorio(i) && (i != file)))
			aux++;
	}
	lista = malloc(aux * 17);
	aux = 0;
	for (i = 0; i < 2048; i++) {
		if ((padre == tablaDeArchivos[i].bloquePadre)
				&& (archivoDirectorio(i) && (i != file))) {
			memcpy(lista + (aux * 17), tablaDeArchivos[i].nombreArchivo, 17);
			aux++;
		}
	}
	*var = aux;
	return lista;
}
int getAttr(char *path) {
	log_debug(log, "getAttr: %s", path);
	int i;
	int j = 0;
	int file;
	char ** ruta = string_split(path, "/");
	uint16_t padre = -1;
	while (ruta[j + 1]) {
		file = buscar(padre, ruta[j], DIRECTORIO);
		if (file == -1)
			return -1;
		padre = tablaDeArchivos[file].bloqueInicial;
		j++;
	}
	file = buscar(padre, ruta[j], DIRECTORIO);
	if (file == -1)
		file = buscar(padre, ruta[j], ARCHIVO);
	return file;
}

void atenderPeticiones(int socket) { // es necesario la ruta de montaje?
	mensaje_CLIENTE_SERVIDOR * mensaje = malloc(
			sizeof(mensaje_CLIENTE_SERVIDOR));
	int devolucion = 1;
	char * puntero;
	int var;
	while ((mensaje = (mensaje_CLIENTE_SERVIDOR *) recibirMensaje(socket))
			!= NULL) { // no esta echa el envio
		switch (mensaje->protolo) {
		case LEER:
			mensaje->buffer = leerArchivo(mensaje->path, &var);
			if (mensaje->buffer == NULL) {
				mensaje->protolo = ERROR;
			}
			mensaje->tamano = var;
			break;
		case ESCRIBIR:
			devolucion = escribirArchivo(mensaje->path, mensaje->buffer,
					mensaje->offset);
			if (devolucion != (strlen(mensaje->buffer) + mensaje->offset))
				log_error(log, "NO se pudo escribir completamente");
			mensaje->tamano = devolucion;
			break;
		case CREAR:
			devolucion = crearArchivo(mensaje->path);
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
			puntero = readAttr(mensaje->path, &var);
			mensaje->tamano = var * 17;
			mensaje->buffer = malloc(mensaje->tamano);
			memcpy(mensaje->buffer, puntero, mensaje->tamano);
			devolucion = 1;
			mensaje->protolo = SLEERDIR;
			mensaje->tamano = var * 17;
			break;
		case GETATTR:
			if ((strcmp(mensaje->path, "/") != 0)) {
				devolucion = getAttr(mensaje->path);
				if (devolucion != -1) {
					mensaje->tipoArchivo = estadoEnum(devolucion);
					mensaje->tamano =
							tablaDeArchivos[devolucion].tamanioArchivo;
				}
			} else {
				mensaje->tipoArchivo = 2;
				devolucion = 1;
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
	levantarHeader();
	B = BLOCK_SIZE;
	F = fileHeader.fs_blocks;
	N = fileHeader.fs_blocks / 8 / B;
	A = ((F - 1 - N - 1024) * 4) / B;
	X = F - 1 - N - 1024 - A;
	bitmap = malloc(N * B);
	bitmap = bitarray_create(data + 64, N * B);
	tablaDeArchivos = malloc(1024 * B);
	tablaDeAsignaciones = malloc(A * B);
	bloquesDeDatos = malloc(X * B);
	log_trace(log,"Vamo a setear ocupado la parte administrativa porque yolo");
	int i;
	X = fileHeader.fs_blocks - X;
	for (i = 0; i < X; ++i) {
			bitarray_set_bit(bitmap, i);
		}
	uint32_t ocupados = bitmapOcupados();
	log_trace(log, "ocupados: %u", ocupados);
	log_trace(log, "Bitmap: Libres: %u    Ocupados:%u",
			fileHeader.fs_blocks - ocupados, ocupados);
	log_trace(log, "tamaño tabla asignacion: %d en bloques", A);
	memcpy(tablaDeArchivos, data + B + N * B, 1024 * B);
	memcpy(tablaDeAsignaciones, data + (B + N + 1024) * B, A);
	memcpy(bloquesDeDatos, data + (B + N + 1024 + A) * B, X);
}

int archivoDirectorio(int i) {
	if (tablaDeArchivos[i].estado == ARCHIVO)
		return 1;
	if (tablaDeArchivos[i].estado == DIRECTORIO)
		return 1;
	return 0;
}

void imprimirDirectoriosRecursivo(archivos_t archivo, int nivel, uint16_t padre) {
	int i;
	int aux;
	for (i = 0; i < 2048; i++) {
		if (tablaDeArchivos[i].bloqueInicial == archivo.bloqueInicial)
			continue;
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
	log_trace(log, "%s|%s|%s|%s|%s", "Num", "nombre", "tipo", "padre",
			"inicial");
	for (i = 0; i < 2048; i++) {
		if (archivoDirectorio(i))
			log_trace(log, "%d|%s|%s|%d|%d", i,
					tablaDeArchivos[i].nombreArchivo, tipoArchivo(i),
					tablaDeArchivos[i].bloquePadre,
					tablaDeArchivos[i].bloqueInicial);
	}
}

void imprimirArbolDeDirectorios() {
	log_trace(log, "Mostrando arbol de directorio");
	int i;
	uint16_t padre = -1;
	for (i = 0; i < 2048; i++) {
		if (archivoDirectorio(i) && tablaDeArchivos[i].bloquePadre == padre) {
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
			open(
					string_from_format(
							"/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/PokedexServidor/Debug/%s",
							nombreBin), O_RDWR);
	struct stat s;
	fstat(osadaFile, &s);
	int size = s.st_size;
	log_trace(log, "%d", size);
	data = malloc(size);
	if ((data = (char*) mmap(0, size, PROT_READ | PROT_WRITE,
	MAP_SHARED, osadaFile, 0)) == -1)
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
	imprimirArbolDeDirectorios();
	mostrarTablaDeArchivos();
	theMinionsRevengeSelect(argv[1], funcionAceptar, atenderPeticiones);
	free(log);
	free(tablaDeArchivos);
	free(tablaDeAsignaciones);
	free(bitmap);
	return 0;
}
