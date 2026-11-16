#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "ir.h"

/* Aplica pasadas de optimizacion sobre la lista de instrucciones de
 * Tres Direcciones "lista" segun el modo indicado en "modo_opt":
 *   - "all" : ejecuta todas las optimizaciones en un ciclo de punto fijo.
 *   - "cf"  : Plegado de Constantes (Constant Folding) y Simplificacion Algebraica.
 *   - "cp"  : Propagacion de Constantes/Copias y Fusion de Temporales.
 *   - "dce" : Eliminacion de Codigo Muerto e Inalcanzable (Dead Code Elimination).
 *
 * Devuelve la cantidad total de transformaciones/instrucciones optimizadas. */
int optimizar_ir(InstruccionIR *lista, const char *modo_opt);

#endif
