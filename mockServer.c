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
	int cantEncontrados=2;

	mensaje_CLIENTE_SERVIDOR * respuesta = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	respuesta->path=malloc(30);
	mensaje_CLIENTE_SERVIDOR * mensajeAEnviar = malloc (sizeof(mensaje_CLIENTE_SERVIDOR));
	mensajeAEnviar->path=malloc(10);

	printf ("Cree las estructuras de respuesta y mensaje \n");

	//Creo el archivo para la lista si me preguntan por el directorio /hola
	char * nombreArchivo = malloc(17);
	nombreArchivo="chau.txt";

	char * nombreArchivo2 = malloc(17);
	nombreArchivo2="perro.txt";

	//Creo el archivo para la lista si me preguntan por el directorio "/"
	char * nombreDirectorio = malloc(17);
	nombreDirectorio="hola";
	char * nombreDirectorio2 = malloc(17);
	nombreDirectorio2="perro";

	//Creo la lista para devolver si me preguntan por /hola
	t_list * listaArchivos1 = list_create();
	int cant1 =list_add(listaArchivos1,(void*)nombreArchivo);
	printf ("Creo lista 1 y lo asigno \n");

	//Creo la lista para devolver si me preguntan por "/"
	t_list * listaArchivos2 = list_create();
	int cant2= list_add(listaArchivos2,(void*)nombreDirectorio);
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
					mensajeAEnviar->protolo=SGETATTR;
					mensajeAEnviar->tipoArchivo=2;
					mensajeAEnviar->path=malloc(1);
					mensajeAEnviar->buffer=malloc(1);
				}else if(strcmp(respuesta->path, "/hola") == 0){
					mensajeAEnviar->protolo=SGETATTR;
					printf ("Recibi /hola getattr \n");
					mensajeAEnviar->tipoArchivo=2;
					mensajeAEnviar->path=malloc(1);
					mensajeAEnviar->buffer=malloc(1);
				}else if(strcmp(respuesta->path, "/perro") == 0){
					mensajeAEnviar->protolo=SGETATTR;
					printf ("Recibi /hola getattr \n");
					mensajeAEnviar->tipoArchivo=2;
					mensajeAEnviar->path=malloc(1);
					mensajeAEnviar->buffer=malloc(1);
				}else if(strcmp(respuesta->path, "/hola/chau.txt") == 0){
					mensajeAEnviar->protolo=SGETATTR;
					printf ("Recibi /chau.txt getattr \n");
					mensajeAEnviar->tipoArchivo=1;
					mensajeAEnviar->tamano=20;
					mensajeAEnviar->path=malloc(1);
					mensajeAEnviar->buffer=malloc(1);
				}else if(strcmp(respuesta->path, "/hola/perro.txt") == 0){
					mensajeAEnviar->protolo=SGETATTR;
					printf ("Recibi /chau.txt getattr \n");
					mensajeAEnviar->tipoArchivo=1;
					mensajeAEnviar->tamano=20;
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
					mensajeAEnviar->protolo=SLEERDIR;
					printf ("Recibi / readdir \n");
					mensajeAEnviar->buffer = malloc(cantEncontrados*17);
					memcpy(mensajeAEnviar->buffer,nombreDirectorio,17);
					memcpy(mensajeAEnviar->buffer+17,nombreDirectorio2,17);
					mensajeAEnviar->tamano=cantEncontrados*17;

					printf ("Creo lista 2 y lo asigno \n");
				}

				if(strcmp(respuesta->path, "/hola") == 0){
					mensajeAEnviar->protolo=SLEERDIR;
					printf ("Recibi /hola readdir \n");
					mensajeAEnviar->buffer = malloc(cantEncontrados*17);
					memcpy(mensajeAEnviar->buffer,nombreArchivo,17);
					memcpy(mensajeAEnviar->buffer+17,nombreArchivo2,17);
					mensajeAEnviar->tamano=cantEncontrados*17;
				}
				printf ("Recibi /Voy a responder \n");
				enviarMensaje(tipoMensaje,socketCliente,(void *) mensajeAEnviar);

				break;

			case LEER:
				if(strcmp(respuesta->path, "/hola/chau.txt") == 0){
					mensajeAEnviar->protolo=SLEER;

					//Defino el contenido del archivo
					char* textoChau=malloc(17);
					textoChau="012345678912345\n";

					//Valido si lo que me piden leer es mas grande de lo que puedo leer
					int cantidadALeer;
					if(respuesta->tamano>strlen(textoChau)){
						cantidadALeer=strlen(textoChau);
					}else{
						cantidadALeer=respuesta->tamano;
					}

					//Malloqueo tanto espacio en el buffer como el tamaño que me pida leer el cliente
					mensajeAEnviar->buffer=malloc(cantidadALeer);

					memcpy(mensajeAEnviar->buffer,textoChau+respuesta->offset,cantidadALeer);
					mensajeAEnviar->tamano=cantidadALeer;
				}else{
					mensajeAEnviar->protolo=ERROR;
				}
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
