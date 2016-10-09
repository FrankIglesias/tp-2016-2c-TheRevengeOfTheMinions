/*
 ============================================================================
 Name        : mockServer.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
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
typedef enum
	__attribute__((packed)) { // wtf ???
		BORRADO = '\0',
	ARCHIVO = '\1',
	DIRECTORIO = '\2',
} osada_file_state;

typedef struct osadaFile {
	osada_file_state estado; //0 borrado, 1 ocupado, 2 directorio
	char* nombreArchivo;
	uint16_t bloquePadre;
	uint32_t tamanioArchivo;
	uint32_t fechaUltimaModif; //como hago fechas?
	uint32_t bloqueInicial;
} archivos_t;
int socketParaServidor;


int main(void) {
	mensaje_t tipoMensaje = CLIENTE_SERVIDOR;
	char* puerto=malloc(10);
	puerto="6000";
	socketParaServidor=crearSocketServidor(puerto);

	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta->path=malloc(30);
	mensaje_CLIENTE_SERVIDOR * mensajeAEnviar = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	mensajeAEnviar->path=malloc(10);

	printf ("Cree las estructuras de respuesta y mensaje \n");

	//Creo el archivo para la lista si me preguntan por el directorio /hola
	char * nombreArchivo = malloc(17);
	nombreArchivo="chau.txt";
	archivos_t* archivoChau=malloc(sizeof(archivos_t));
	archivoChau->nombreArchivo=malloc(17);
	strcpy(archivoChau->nombreArchivo,nombreArchivo);
	archivoChau->tamanioArchivo=144;


	//Creo el archivo para la lista si me preguntan por el directorio "/"
	char * nombreDirectorio = malloc(17);
	nombreDirectorio="hola";
	archivos_t* directorioHola=malloc(sizeof(archivos_t));
	directorioHola->nombreArchivo =malloc(17);
	strcpy(directorioHola->nombreArchivo,nombreDirectorio);
	printf ("Creo archivo 2 y lo asigno \n");

	//Creo la lista para devolver si me preguntan por /hola
	t_list * listaArchivos1 = list_create();
	list_add(listaArchivos1,(void*)archivoChau);
	printf ("Creo lista 1 y lo asigno \n");

	//Creo la lista para devolver si me preguntan por "/"
	t_list * listaArchivos2 = list_create();
	list_add(listaArchivos2,(void*)directorioHola);
	printf ("Creo lista 2 y lo asigno \n");

	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int socketCliente = accept(socketParaServidor, (struct sockaddr *) &addr, &addrlen);

	while(1){

		printf ("Entro al while y estoy por bloquearme \n");


		respuesta =(mensaje_CLIENTE_SERVIDOR*) recibirMensaje(socketCliente);

		printf ("Recibi respuesta \n");



		switch(respuesta->protolo){
			case GETATTR:
				if(strcmp(respuesta->path, "/") == 0){
					printf ("Recibi / getattr \n");
					mensajeAEnviar->protolo=GETATTR;
					mensajeAEnviar->tipoArchivo=2;
					mensajeAEnviar->path=malloc(1);
					mensajeAEnviar->buffer=malloc(1);
				}
				if(strcmp(respuesta->path, "/hola") == 0){
					mensajeAEnviar->protolo=GETATTR;
					printf ("Recibi /hola getattr \n");
					mensajeAEnviar->tipoArchivo=2;
					mensajeAEnviar->path=malloc(1);
					mensajeAEnviar->buffer=malloc(1);
				}
				if(strcmp(respuesta->path, "/chau.txt") == 0){
					mensajeAEnviar->protolo=GETATTR;
					printf ("Recibi /chau.txt getattr \n");
					mensajeAEnviar->tipoArchivo=1;
					mensajeAEnviar->tamano=144;
					mensajeAEnviar->path=malloc(1);
					mensajeAEnviar->buffer=malloc(1);
				}else{
					printf("Recibi basura");
					mensajeAEnviar->protolo=ERROR;
				}
				printf ("Voy a responder \n");
				enviarMensaje(tipoMensaje,socketCliente,(void *) mensajeAEnviar);
				break;

			case LEERDIR:

				if(strcmp(respuesta->path, "/") == 0){
					mensajeAEnviar->protolo=LEERDIR;
					printf ("Recibi / readdir \n");
					//mensajeAEnviar->buffer = malloc(sizeof((char*)listaArchivos2));
					mensajeAEnviar->buffer = malloc((list_size(listaArchivos2)*sizeof(archivos_t))+17);
					mensajeAEnviar->buffer=(char *) listaArchivos2;
					mensajeAEnviar->tamano=(list_size(listaArchivos2)*sizeof(archivos_t))+17;
					strcpy(mensajeAEnviar->path,"LEERV");

					//Pruebo si anda esto de parsear la lista en un char*
					t_list * pruebaLista = list_create();
					archivos_t* archivoPrueba=malloc(sizeof(archivos_t));
					archivoPrueba->nombreArchivo=malloc(17);
					pruebaLista=(t_list*)mensajeAEnviar->buffer;
					int verTamano = (list_size(pruebaLista)*sizeof(archivos_t))+17;
					archivoPrueba=(archivos_t*)list_get(pruebaLista,0);
					printf("Lo que contiene el archivo que voy a mandar es:%s \n",archivoPrueba->nombreArchivo);
					printf("El tamaño del buffer a enviar es %d y el del de prueba es %d \n",mensajeAEnviar->tamano,verTamano);

					printf ("Creo lista 2 y lo asigno \n");
				}

				if(strcmp(respuesta->path, "/hola") == 0){
					mensajeAEnviar->protolo=LEERDIR;
					printf ("Recibi /hola readdir \n");
					mensajeAEnviar->buffer = malloc(sizeof((char*)listaArchivos1));
					mensajeAEnviar->buffer=(char *) listaArchivos1;
				}
				printf ("Recibi /Voy a responder \n");
				enviarMensaje(tipoMensaje,socketCliente,(void *) mensajeAEnviar);

				break;

			default:
				printf("Algo fallo");
				mensajeAEnviar->protolo=ERROR;
				enviarMensaje(tipoMensaje,socketCliente,(void *) mensajeAEnviar);
				break;
		}
	}
	return 0;
}
