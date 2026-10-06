#ifndef SEMANTICA_H
#define SEMANTICA_H

#include "ast.h"
#include "tabla.h"

/* =====================================================================
 * semantica.h
 * Analizador Semantico del compilador C-TDS - Etapa 2
 *
 * El analisis semantico recorre el AST (que ya construyo el parser),
 * poblando la Tabla de Simbolos (TS) y verificando las 14 reglas
 * semanticas de la especificacion del lenguaje (Docs/01-TDS-spec).
 *
 * Como funciona (resumen):
 *  - Un unico recorrido, en el MISMO orden en que aparecen las
 *    declaraciones en el fuente. Ese orden es el que hace valida la
 *    regla 2 ("ningun identificador es usado antes de ser declarado"):
 *    al insertar cada simbolo recien cuando llega su declaracion, una
 *    referencia tardia simplemente no se encuentra en la TS.
 *  - Por cada NODE_METODO: se inserta la firma en el nivel global
 *    (nivel 0), se abre un nivel nuevo, se insertan los parametros y
 *    se procesa el cuerpo del metodo en ESE mismo nivel (los parametros
 *    son alcance-local del metodo, comparten nivel con las variables
 *    locales de su cuerpo, tal como define la spec).
 *  - Cualquier otro NODE_BLOQUE (el de un if, de un while, o un bloque
 *    suelto) abre y cierra su propio nivel: eso produce el anidamiento
 *    de scopes y el shadowing que pide la spec.
 *  - Las expresiones se analizan de forma bottom-up: cada llamada a
 *    inferir_tipo() devuelve el tipo de la expresion (o TIPO_ERROR) y
 *    anota ese tipo en el nodo del AST. El anotador es el que hace que
 *    el volcado .sem muestre el arbol con tipos resueltos.
 *
 * TIPO_ERROR es un "veneno": cualquier subexpresion que ya reporto un
 * error devuelve TIPO_ERROR y los chequeos padre la ignoran, para no
 * emitir una cascada de errores secundarios por el mismo problema.
 *
 * Decisiones de diseño (ver Docs/): mensaje de error unico por
 * ocurrencia, se sigue analizando luego de un error para reportar todos
 * los problemas de una pasada, y el programa SOLO se considera valido
 * (y solo se escribe el archivo .sem) si errores_semanticos == 0.
 * ===================================================================== */

/* Cantidad de errores semanticos detectados en la ultima pasada. */
extern int errores_semanticos;

/* Activa/desactiva la traza de depuracion (-debug): cada operacion
 * sobre la TS (insertar/buscar/abrir/cerrar nivel) se imprime por
 * stdout a medida que ocurre. */
void sem_set_debug(int activo);

/* Punto de entrada del analisis semantico.
 * Recorre "raiz" (un NODE_PROGRAMA), llena la TS, anota los tipos en el
 * AST y reporta por stderr los errores detectados.
 * Devuelve la cantidad de errores (0 = programa semanticamente valido).
 * Debe llamarse con la TS en su estado inicial (ts_inicializar() lo
 * hace la propia funcion). */
int analizar_semantica(ASTNode *raiz);

/* Escribe en "archivo" el resultado de la etapa (.sem):
 *  1) el AST ya anotado con los tipos resueltos, y
 *  2) el volcado de la Tabla de Simbolos (ambito global, los locales
 *     ya fueron destruidos al cerrar sus niveles).
 * Se llama solo cuando el analisis termino sin errores. */
void volcar_resultado_semantico(FILE *archivo, ASTNode *raiz);

#endif
