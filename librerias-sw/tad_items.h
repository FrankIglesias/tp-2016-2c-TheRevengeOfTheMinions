#ifndef __TAD_ITEMS__

#define __TAD_ITEMS__

#include "nivel.h"
#include <commons/collections/list.h>




/*
 * @NAME: BorrarItem
 * @DESC: Quita el item del mapa
 * @PARAMS:
 * 	 items - lista de los items generales del mapa
 * 	 id - id del elemento a quitar del mapa
 */
void BorrarItem(t_list* items, char id);

/*
 * @NAME: restarRecurso
 * @DESC: Resta 1 del recurso (POKEMON) asignado en el mapa a una caja (POKEDEX)
 * @PARAMS:
 * 	 items - lista de los items generales del mapa
 * 	 id - id del elemento a restar
 */
void restarRecurso(t_list* items, char id);

/*
 * @NAME: MoverPersonaje
 * @DESC: Mueve el personaje hacia una posicion en particular
 * @PARAMS:
 * 	 items - lista de los items generales del mapa
 * 	personaje - id del elemento a mover
 * 	x - posicion en x
 * 	y - posicion en y
 */
void MoverPersonaje(t_list* items, char personaje, int x, int y);

/*
 * @NAME: MoverEnemigo
 * @DESC: Mueve el personaje hacia una posicion en particular
 * @PARAMS:
 * 	 items - lista de los items generales del mapa
 * 	personaje - id del elemento a mover
 * 	x - posicion en x
 * 	y - posicion en y
 */
void MoverEnemigo(t_list* items, char personaje, int x, int y);


/*
 * @NAME: CrearPersonaje
 * @DESC: Crea un personaje en una posicion en particular
 * @PARAMS:
 * 	 items - lista de los items generales del mapa
 * 	personaje - id del elemento a crear
 * 	x - posicion en x
 * 	y - posicion en y
 */
void CrearPersonaje(t_list* items, char id, int x , int y);

/*
 * @NAME: CrearEnemigo
 * @DESC: Crea un personaje en una posicion en particular
 * @PARAMS:
 * 	 items - lista de los items generales del mapa
 * 	personaje - id del elemento a crear
 * 	x - posicion en x
 * 	y - posicion en y
 */
void CrearEnemigo(t_list* items, char id, int x , int y);

/*
 * @NAME: CrearCaja
 * @DESC: Crea un personaje en una posicion en particular
 * @PARAMS:
 * 	 items - lista de los items generales del mapa
 * 	personaje - id del elemento a crear
 * 	x - posicion en x
 * 	y - posicion en y
 */
void CrearCaja(t_list* items, char id, int x , int y, int cant);

/*
 * @NAME: CrearItem
 * @DESC: Crea un personaje en una posicion en particular
 * @PARAMS:
 * 	 items - lista de los items generales del mapa
 * 	personaje - id del elemento a crear
 * 	x - posicion en x
 * 	y - posicion en y
 */
void CrearItem(t_list* items, char id, int x, int y, char tipo, int cant);

#endif

