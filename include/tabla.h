#ifndef TABLA_H
#define TABLA_H

#include "ast.h"

#define MAX_NIVELES 128     // Profundidad maxima de bloques anidados
#define MAX_PARAMETROS 32   // Parametros maximos por funcion

typedef enum {
  
    CAT_VAR_GLOBAL,
    CAT_VAR_LOCAL,
    CAT_PARAMETRO,
    CAT_FUNCION

} CategoriaSimbolo;

typedef struct EntradaTS EntradaTS;

struct EntradaTS {

  Simbolo *simbolo;              // Mismo Simbolo* del nodo AST (Prestado)
  CategoriaSimbolo categoria;
  int linea;                    // Linea de la Declaracion
  
  // Solo valido si categoria == CAT_FUNCION:
  int cantidad_parametros;
  char *tipos_parametros[MAX_PARAMETROS];

  EntradaTS *siguiente;         // Lista Enlazada Dentro Del Mismo Nivel

};

/* ---------------------------------------------------------------------
 * Estado global de la TS
 * --------------------------------------------------------------------- */

extern EntradaTS *TS[MAX_NIVELES];
extern int nivel_actual;

/* ---------------------------------------------------------------------
 * Ciclo de vida de la tabla y de los niveles
 * --------------------------------------------------------------------- */

/* Deja la TS en su estado inicial: todos los niveles vacios y
 * nivel_actual = 0 (scope global). Se llama una sola vez, antes de
 * empezar a recorrer el AST. */
void ts_inicializar(void);

/* Abre un nuevo scope (incrementa nivel_actual). Se llama al entrar a
 * cualquier NODE_BLOQUE. */
void ts_abrir_nivel(void);

/* Cierra el scope actual: libera (free) cada EntradaTS de
 * TS[nivel_actual] -sin tocar entrada->simbolo, ver nota de arriba-,
 * deja TS[nivel_actual] en NULL y decrementa nivel_actual. */
void ts_cerrar_nivel(void);

/* ---------------------------------------------------------------------
 * Construccion de entradas
 * --------------------------------------------------------------------- */

/* Crea una EntradaTS a partir de un Simbolo ya existente (el mismo
 * puntero que cuelga de un nodo del AST: nodo->simbolo). No duplica
 * nada. Para CAT_FUNCION, cantidad_parametros/tipos_parametros se
 * completan aparte (recorriendo la lista de parametros del metodo),
 * antes de insertar la entrada. */
EntradaTS *ts_crear_entrada(Simbolo *simbolo, CategoriaSimbolo categoria, int linea);

/* ---------------------------------------------------------------------
 * Busqueda e insercion
 *
 * El reporte del error (el mensaje por consola con formato propio) lo
 * hace quien llama a estas funciones -el motor semantico-, no la TS:
 * estas funciones solo informan el resultado.
 * --------------------------------------------------------------------- */

/* Busca "nombre" unicamente en TS[nivel]. Devuelve la entrada si la
 * encuentra, o NULL si no esta en ESE nivel puntual. Se usa antes de
 * insertar, para detectar redeclaracion en el mismo scope. */
EntradaTS *ts_buscar_en_nivel(const char *nombre, int nivel);

/* Busca "nombre" empezando en TS[nivel_actual] y bajando hasta TS[0].
 * Devuelve la primera coincidencia (la mas local: resuelve shadowing
 * automaticamente por el orden de busqueda), o NULL si no se
 * encontro en ningun nivel. */
EntradaTS *ts_buscar(const char *nombre);

/* Inserta "entrada" al inicio de la lista de TS[nivel_actual].
 * Devuelve 1 si se inserto, o 0 si entrada->simbolo->nombre_id ya
 * estaba declarado en TS[nivel_actual] (en ese caso NO inserta, y es
 * responsabilidad de quien llama reportar el error y decidir que
 * hacer con la EntradaTS ya creada). */
int ts_insertar(EntradaTS *entrada);

#endif 