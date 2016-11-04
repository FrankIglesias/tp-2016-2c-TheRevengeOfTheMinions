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

uint16_t buscar(uint16_t padre, char * nombre, osada_file_state tipo) {
	uint16_t i;
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
uint32_t estadoEnum(uint16_t i) {
	int aux = i;
	switch (tablaDeArchivos[aux].estado) {
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
		if (padre == tablaDeArchivos[j].bloquePadre
				&& (strcmp(tablaDeArchivos[j].nombreArchivo, ruta[i]) == 0)
				&& tablaDeArchivos[j].estado == DIRECTORIO) {
			padre = j;
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
	uint16_t file;
	uint16_t menosuno = -1;
	while (ruta[j + 1]) {
		file = buscar(padre, ruta[j], DIRECTORIO);
		if (file == menosuno)
			return -1;
		padre = file;
		j++;
	}
	return buscar(padre, ruta[j], tipo);
}
int existeUnoIgual(uint16_t padre, char* nombre, osada_file_state tipo) {
	int i;
	for (i = 0; i < 2048; i++) {
		if ((padre == tablaDeArchivos[i].bloquePadre)
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
			tablaDeArchivos[i].bloqueInicial = -1;
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
	int file = verificarSiExiste(path, ARCHIVO);
	log_info(log, "Se va a leer el archivo: %s Tamaño: %u", path,
			tablaDeArchivos[file].tamanioArchivo);
	if (file == -1 && tablaDeArchivos[file].estado != ARCHIVO)
		return NULL;
	char * lectura = malloc(tablaDeArchivos[file].tamanioArchivo + 1); // +1 para indicar fin de archivo
	int bloqueSiguiente = tablaDeArchivos[file].bloqueInicial;
	int puntero = 0;
	while (puntero != tablaDeArchivos[file].tamanioArchivo) {
		if ((tablaDeArchivos[file].tamanioArchivo - puntero) > BLOCK_SIZE) {
			log_trace(log, "A Leer: %d", bloqueSiguiente);
			memcpy(lectura + puntero,
					bloquesDeDatos + (BLOCK_SIZE * bloqueSiguiente),
					BLOCK_SIZE);
			puntero += BLOCK_SIZE;
			bloqueSiguiente = tablaDeAsignaciones[bloqueSiguiente];
			if (bloqueSiguiente == -1)
				break;
		} else {
			log_trace(log, "A Leer: %d", bloqueSiguiente);
			memcpy(lectura + puntero,
					bloquesDeDatos + (BLOCK_SIZE * bloqueSiguiente),
					tablaDeArchivos[file].tamanioArchivo - puntero);
			puntero += tablaDeArchivos[file].tamanioArchivo - puntero;
		}
	}
	lectura[puntero] = '\0';
	*aux = puntero;
	log_trace(log, "Lectura: %s", lectura);
	return lectura;
}
int escribirArchivo(char * path, char * buffer, int offset) {
	log_info(log, "Escribiendo en: %s ,contenido:%s offset:%d", path, buffer,
			offset);
	int file = verificarSiExiste(path, ARCHIVO);
	if (file == -1)
		return -1;
	int aux = 0;
	int tam = strlen(buffer);
	int puntero = 0; // Para saber el tamaño
	if (tablaDeArchivos[file].bloqueInicial == -1)
		tablaDeArchivos[file].bloqueInicial = buscarBloqueLibre();
	int bloqueActual = tablaDeArchivos[file].bloqueInicial;
	while (offset != puntero) {
		log_trace(log, "si offset es 0 no deberia nunca entrar aca");
		if (tablaDeAsignaciones[bloqueActual] == -1) {
			if (asignarBloqueLibre(bloqueActual) == -1)
				return -1;
		}
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
		log_trace(log, "Unica escritura: %d", bloqueActual);
		memcpy(bloquesDeDatos + (BLOCK_SIZE * bloqueActual) + aux, buffer, tam);
		tam = 0;
	}
	int direccion;
	while (tam != 0) {
		if (tam > BLOCK_SIZE) {
			log_trace(log, "A escribir: %d", bloqueActual);
			memcpy(bloquesDeDatos + (BLOCK_SIZE * bloqueActual),
					buffer + puntero,
					BLOCK_SIZE);
			puntero += BLOCK_SIZE;
			tam -= BLOCK_SIZE;
			if (tablaDeAsignaciones[bloqueActual] == -1) {
				if (asignarBloqueLibre(bloqueActual) == -1) {
					tam = 0;
					break;
				}
			}
			bloqueActual = tablaDeAsignaciones[bloqueActual];
		} else {
			log_trace(log, "A escribir: %d", bloqueActual);
			memcpy(bloquesDeDatos + (BLOCK_SIZE * bloqueActual),
					buffer + puntero, tam);
			puntero += tam;
			tam = 0;
		}
	}
	tablaDeArchivos[file].tamanioArchivo = strlen(buffer) + offset;
	sincronizarMemoria();
	log_trace(log, "tamaño del archivo escrito %u",
			tablaDeArchivos[file].tamanioArchivo);
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
	i = ingresarEnLaTArchivos(padre, ruta[j], ARCHIVO);
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
				if ((tablaDeArchivos[j].bloquePadre == padre)
						&& (tablaDeArchivos[j].estado == DIRECTORIO)
						&& (strcmp(tablaDeArchivos[j].nombreArchivo, ruta[i])
								== 0)) {
					padre = j;
					i++;
					break;
				}
			}
		}
	}
	if (existeUnoIgual(padre, ruta[i], DIRECTORIO))
		return -1;
	i = ingresarEnLaTArchivos(padre, ruta[i], DIRECTORIO);
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
					&& tablaDeArchivos[j].bloquePadre == file) {
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
int renombrar(char * path, char * path2) {
	log_info(log, "Renombrando: %s por %s", path, path2);
	uint16_t menosUno = -1;
	uint16_t menosDos = -2;
	uint16_t file = verificarSiExiste(path, ARCHIVO);
	uint16_t padre = -2;
	int i = 0;
	char ** ruta = string_split(path2, "/");
	if (file == menosUno)
		file = verificarSiExiste(path, DIRECTORIO);
	if (file != menosUno) {
		padre = buscarAlPadre(path2);
		if (padre != -2) {
			while (ruta[i + 1]) {
				i++;
			}
			if (!existeUnoIgual(padre, ruta[i], tablaDeArchivos[file].estado)) {
				memcpy(&tablaDeArchivos[file].nombreArchivo, ruta[i],
						strlen(ruta[i]));
				tablaDeArchivos[file].nombreArchivo[strlen(ruta[i])] = '\0';
				tablaDeArchivos[file].bloquePadre = padre;
				sincronizarMemoria();
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
	uint16_t unoMenos = -1;
	if (strcmp(path, "/") != 0) {
		char ** ruta = string_split(path, "/");
		while (ruta[j]) {
			file = buscar(file, ruta[j], DIRECTORIO);
			if (file == unoMenos)
				return NULL;
			j++;
		}
	}
	for (i = 0; i < 2048; i++) {
		if ((file == tablaDeArchivos[i].bloquePadre) && (archivoDirectorio(i)))
			aux++;
	}
	lista = malloc(aux * 17);
	log_trace(log, "%d", aux);
	aux = 0;

	for (i = 0; i < 2048; i++) {
		if ((file == tablaDeArchivos[i].bloquePadre)
				&& (archivoDirectorio(i))) {
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
	uint16_t file;
	char ** ruta = string_split(path, "/");
	uint16_t padre = -1;
	uint16_t menosuno = -1;
	while (ruta[j + 1]) {
		file = buscar(padre, ruta[j], DIRECTORIO);
		if (file == -1)
			return -1;
		padre = file;
		j++;
	}
	file = buscar(padre, ruta[j], DIRECTORIO);
	if (file == menosuno)
		file = buscar(padre, ruta[j], ARCHIVO);
	return file;
}

void atenderPeticiones(int socket) { // es necesario la ruta de montaje?
	mensaje_CLIENTE_SERVIDOR * mensaje = malloc(
			sizeof(mensaje_CLIENTE_SERVIDOR));
	int devolucion = 1;
	uint16_t devolucion16 = 1;
	uint16_t menosuno = -1;
	char * puntero;
	int var;
	while ((mensaje = (mensaje_CLIENTE_SERVIDOR *) recibirMensaje(socket))
			!= NULL) { // no esta echa el envio
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
					mensaje->offset);
			if (devolucion == -1)
				mensaje->protolo = ERROR;
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
				if (devolucion16 != menosuno) {
					mensaje->tipoArchivo = estadoEnum(devolucion16);
					mensaje->tamano =
							tablaDeArchivos[devolucion16].tamanioArchivo;
					devolucion16 = 1;
				} else
					mensaje->protolo = ERROR;
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
int tamTAsignacion(){
	return (fileHeader.fs_blocks-fileHeader.data_blocks-fileHeader.inicioTablaAsignaciones) * BLOCK_SIZE;
}

void levantarOsada() {
	log_trace(log, "Levantando osada");
	levantarHeader();
	int puntero = BLOCK_SIZE;
	bitmap = malloc(fileHeader.bitmap_blocks * BLOCK_SIZE);
	bitmap = bitarray_create(data + BLOCK_SIZE, fileHeader.bitmap_blocks * BLOCK_SIZE);
	tablaDeArchivos = malloc(1024 * BLOCK_SIZE);
	tablaDeAsignaciones = malloc(tamTAsignacion());
	bloquesDeDatos = malloc(fileHeader.data_blocks * BLOCK_SIZE);
	log_trace(log, "Vamo a setear ocupado la parte administrativa porque yolo");
/*	int i;
	X = fileHeader.fs_blocks - X;
	for (i = 0; i < X; ++i) {
		bitarray_set_bit(bitmap, i);
	}
	uint32_t ocupados = bitmapOcupados();
	log_trace(log, "ocupados: %u", ocupados);
	log_trace(log, "Bitmap: Libres: %u    Ocupados:%u",
			fileHeader.fs_blocks - ocupados, ocupados);
	log_trace(log, "tamaño tabla asignacion: %d en bloques", A);*/
	puntero+=fileHeader.bitmap_blocks * BLOCK_SIZE;
	memcpy(tablaDeArchivos, data + puntero, 1024 * BLOCK_SIZE);
	puntero=fileHeader.inicioTablaAsignaciones * BLOCK_SIZE;
	memcpy(tablaDeAsignaciones, data + puntero,tamTAsignacion());
	puntero=(fileHeader.fs_blocks-fileHeader.data_blocks) * BLOCK_SIZE;
	memcpy(bloquesDeDatos, data + puntero, fileHeader.data_blocks*BLOCK_SIZE);
}

int archivoDirectorio(int i) {
	if (tablaDeArchivos[i].estado == ARCHIVO)
		return 1;
	if (tablaDeArchivos[i].estado == DIRECTORIO)
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
void mostrarTablaDeAsignacion() {
	t_log * asignacion = log_create("asginacino.txt", "Osada", 1, 0);
	int i;
	log_trace(asignacion, "%s|||%s        TABLA DE ASIGNACION", "ID", "SIG");
	for (i = 0; i < A; i++) {
		log_trace(asignacion, "%d|||%d", i, tablaDeAsignaciones[i]);
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
	log_trace(log, "%d", size);
	data = malloc(size);
	if ((data = (char*) mmap(0, size, PROT_READ | PROT_WRITE,
	MAP_SHARED, osadaFile, 0)) == -1)
		log_trace(log, "la estamos cagando");

	log_trace(log, "memoria mapeada");
}
void sincronizarMemoria() {
	memcpy(data, &fileHeader, BLOCK_SIZE);
	int puntero = BLOCK_SIZE;
	memcpy(data + BLOCK_SIZE, bitmap,fileHeader.bitmap_blocks*BLOCK_SIZE);
	puntero +=fileHeader.bitmap_blocks*BLOCK_SIZE;
	memcpy(data + puntero, tablaDeArchivos, 1024 * BLOCK_SIZE);
	puntero=fileHeader.inicioTablaAsignaciones*BLOCK_SIZE;
	memcpy(data + puntero, tablaDeAsignaciones,tamTAsignacion());
	puntero=(fileHeader.fs_blocks-fileHeader.data_blocks) * BLOCK_SIZE;
	memcpy(data + puntero, bloquesDeDatos, fileHeader.data_blocks*BLOCK_SIZE);
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
//	mostrarTablaDeAsignacion();
	theMinionsRevengeSelect(argv[1], funcionAceptar, atenderPeticiones);
	free(log);
	free(tablaDeArchivos);
	free(tablaDeAsignaciones);
	free(bitmap);
	return 0;
}
