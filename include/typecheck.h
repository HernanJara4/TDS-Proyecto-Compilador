#ifndef TYPECHECK_H
#define TYPECHECK_H

#include "ast.h"
#include "tabla.h"

/* Contador global de errores semanticos detectados durante el recorrido */
extern int errores_semanticos;

/* Recorre el AST completo a partir de "raiz" (NODE_PROGRAMA), poblando
 * la Tabla de Simbolos por niveles, verificando las 14 reglas semanticas
 * de C-TDS y anotando los tipos inferidos en cada nodo del AST.
 *
 * Devuelve la cantidad total de errores semanticos encontrados (0 si el
 * programa es semanticamente valido). */
int analizar_semantica(ASTNode *raiz);

#endif
