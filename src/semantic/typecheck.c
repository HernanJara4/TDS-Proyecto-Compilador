#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/typecheck.h"

int errores_semanticos = 0;

/* Puntero al nodo NODE_METODO que se esta analizando actualmente.
 * Se usa al validar sentencias "return" (Reglas 6 y 7). */
static ASTNode *metodo_actual = NULL;

/* Contador para desambiguar variables locales en bloques anidados (shadowing) */
static int contador_shadow = 0;

/* Prototipos internos para el recorrido mutuo entre bloques, sentencias y expresiones */
static void        chequear_bloque(ASTNode *bloque, int abrir_scope);
static void        chequear_sentencia(ASTNode *stmt);
static const char *chequear_expresion(ASTNode *expr);
static const char *chequear_llamada(ASTNode *llamada, int en_expresion);

/* ---------------------------------------------------------------------
 * Funciones auxiliares de tipos y anotacion del AST
 * --------------------------------------------------------------------- */

static int es_numerico(const char *tipo) {
    return tipo != NULL &&
           (strcmp(tipo, "int") == 0 || strcmp(tipo, "float") == 0);
}

/* Dos tipos son compatibles si son identicos o si ambos son numericos
 * (Regla 14: se permiten coerciones o truncamientos entre int y float). */
static int tipos_compatibles(const char *t1, const char *t2) {
    if (t1 == NULL || t2 == NULL)
        return 0;
    if (strcmp(t1, t2) == 0)
        return 1;
    if (es_numerico(t1) && es_numerico(t2))
        return 1;
    return 0;
}

/* Guarda en nodo->simbolo->tipo_dato una copia propia del tipo inferido,
 * dejando el AST anotado para las etapas siguientes y respetando la
 * propiedad de memoria de liberar_ast(). */
static void anotar_tipo(ASTNode *nodo, const char *tipo) {
    if (nodo == NULL || nodo->simbolo == NULL || tipo == NULL)
        return;
    free(nodo->simbolo->tipo_dato);
    nodo->simbolo->tipo_dato = strdup(tipo);
}

/* Inserta una declaracion de variable o parametro en el nivel actual de la TS.
 * Verifica la Regla 1 (unicidad de identificadores dentro de un mismo bloque). */
static void registrar_declaracion(ASTNode *decl, CategoriaSimbolo categoria) {
    if (decl == NULL || decl->simbolo == NULL)
        return;

    /* Si estamos en un bloque anidado dentro de una funcion (nivel >= 2) y el
     * nombre oculta a un parametro o variable local de un nivel externo de la
     * misma funcion (niveles 1..nivel_actual-1), le asignamos un alias unico
     * en decl->simbolo->valor para que el Codigo Intermedio no mezcle ambas
     * variables al aplanar los bloques de la funcion. */
    if (nivel_actual >= 2 && decl->simbolo->nombre_id != NULL) {
        for (int niv = nivel_actual - 1; niv >= 1; niv--) {
            if (ts_buscar_en_nivel(decl->simbolo->nombre_id, niv) != NULL) {
                char buf[64];
                contador_shadow++;
                snprintf(buf, sizeof buf, "%s.%d", decl->simbolo->nombre_id, contador_shadow);
                free(decl->simbolo->valor);
                decl->simbolo->valor = strdup(buf);
                break;
            }
        }
    }

    EntradaTS *entrada = ts_crear_entrada(decl->simbolo, categoria, decl->linea);
    if (!ts_insertar(entrada)) {
        EntradaTS *previa = ts_buscar_en_nivel(decl->simbolo->nombre_id, nivel_actual);
        fprintf(stderr,
                "Error semantico en linea %d: el identificador '%s' ya fue declarado en este mismo bloque (linea %d)\n",
                decl->linea,
                decl->simbolo->nombre_id,
                previa != NULL ? previa->linea : decl->linea);
        errores_semanticos++;
        free(entrada); /* No se libera entrada->simbolo porque pertenece al AST */
    }
}

/* Verifica si un bloque garantiza la ejecucion de al menos una sentencia
 * "return" en todos sus caminos de control posibles (para metodos no-void). */
static int bloque_garantiza_retorno(ASTNode *bloque) {
    if (bloque == NULL || bloque->tipo != NODE_BLOQUE)
        return 0;

    for (ASTNode *s = bloque->hijo2; s != NULL; s = s->siguiente) {
        if (s->tipo == NODE_RETORNO)
            return 1;
        if (s->tipo == NODE_BLOQUE && bloque_garantiza_retorno(s))
            return 1;
        if (s->tipo == NODE_IF && s->hijo3 != NULL &&
            bloque_garantiza_retorno(s->hijo2) &&
            bloque_garantiza_retorno(s->hijo3))
            return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------------
 * Chequeo de llamadas a metodos (Reglas 2, 4 y 5)
 * --------------------------------------------------------------------- */

static const char *chequear_llamada(ASTNode *llamada, int en_expresion) {
    if (llamada == NULL || llamada->simbolo == NULL)
        return NULL;

    const char *nombre = llamada->simbolo->nombre_id;

    /* Primero se chequean las expresiones de todos los argumentos (de
     * izquierda a derecha) para detectar errores internos en ellos. */
    int cant_args = 0;
    for (ASTNode *arg = llamada->hijo1; arg != NULL; arg = arg->siguiente) {
        chequear_expresion(arg);
        cant_args++;
    }

    /* Regla 2: el metodo debe estar declarado previamente */
    EntradaTS *entrada = ts_buscar(nombre);
    if (entrada == NULL) {
        fprintf(stderr,
                "Error semantico en linea %d: el metodo '%s' es invocado sin haber sido declarado\n",
                llamada->linea, nombre);
        errores_semanticos++;
        return NULL;
    }

    /* Si un identificador local oculta al metodo o es una variable global */
    if (entrada->categoria != CAT_FUNCION) {
        fprintf(stderr,
                "Error semantico en linea %d: el identificador '%s' no es un metodo, no puede ser invocado\n",
                llamada->linea, nombre);
        errores_semanticos++;
        return NULL;
    }

    /* Regla 4: cantidad de argumentos formales vs reales */
    if (cant_args != entrada->cantidad_parametros) {
        fprintf(stderr,
                "Error semantico en linea %d: el metodo '%s' esperaba %d argumento/s pero recibio %d\n",
                llamada->linea, nombre, entrada->cantidad_parametros, cant_args);
        errores_semanticos++;
    }

    /* Regla 4 (y Regla 14): tipos de cada argumento */
    int idx = 0;
    for (ASTNode *arg = llamada->hijo1;
         arg != NULL && idx < entrada->cantidad_parametros;
         arg = arg->siguiente, idx++) {
        const char *tipo_arg   = (arg->simbolo != NULL) ? arg->simbolo->tipo_dato : NULL;
        const char *tipo_param = entrada->tipos_parametros[idx];

        if (tipo_arg != NULL && tipo_param != NULL &&
            !tipos_compatibles(tipo_param, tipo_arg)) {
            fprintf(stderr,
                    "Error semantico en linea %d: el argumento %d de '%s' es de tipo '%s', pero se esperaba '%s'\n",
                    arg->linea, idx + 1, nombre, tipo_arg, tipo_param);
            errores_semanticos++;
        }
    }

    const char *tipo_retorno = entrada->simbolo->tipo_dato;
    anotar_tipo(llamada, tipo_retorno);

    /* Regla 5: si la llamada se usa como expresion, debe retornar un valor */
    if (en_expresion && tipo_retorno != NULL && strcmp(tipo_retorno, "void") == 0) {
        fprintf(stderr,
                "Error semantico en linea %d: el metodo '%s' es 'void' y no puede usarse dentro de una expresion\n",
                llamada->linea, nombre);
        errores_semanticos++;
        return NULL;
    }

    return llamada->simbolo->tipo_dato;
}

/* ---------------------------------------------------------------------
 * Chequeo e inferencia de tipos en expresiones (Reglas 2, 8, 10, 11, 12, 14)
 * --------------------------------------------------------------------- */

static const char *chequear_expresion(ASTNode *expr) {
    if (expr == NULL)
        return NULL;

    switch (expr->tipo) {
        case NODE_NRO:
        case NODE_BOOL:
            /* Los literales ya traen su tipo_dato ("int", "float", "boolean") desde el parser */
            return (expr->simbolo != NULL) ? expr->simbolo->tipo_dato : NULL;

        case NODE_ID: {
            const char *nombre = expr->simbolo->nombre_id;
            EntradaTS  *entrada = ts_buscar(nombre);

            /* Regla 2: uso antes de declaracion */
            if (entrada == NULL) {
                fprintf(stderr,
                        "Error semantico en linea %d: la variable '%s' se usa sin haber sido declarada\n",
                        expr->linea, nombre);
                errores_semanticos++;
                return NULL;
            }

            /* Regla 8: una <location> debe ser variable local, global o parametro */
            if (entrada->categoria == CAT_FUNCION) {
                fprintf(stderr,
                        "Error semantico en linea %d: '%s' es un metodo y no puede usarse como variable\n",
                        expr->linea, nombre);
                errores_semanticos++;
                return NULL;
            }

            anotar_tipo(expr, entrada->simbolo->tipo_dato);
            if (entrada->simbolo->valor != NULL) {
                free(expr->simbolo->valor);
                expr->simbolo->valor = strdup(entrada->simbolo->valor);
            }
            return expr->simbolo->tipo_dato;
        }

        case NODE_LLAMADA:
            return chequear_llamada(expr, 1);

        case NODE_OP_UNARIA: {
            const char *op   = expr->simbolo->valor;
            const char *t_op = chequear_expresion(expr->hijo1);

            if (t_op == NULL)
                return NULL; /* El operando ya reporto un error previo */

            if (strcmp(op, "-") == 0) {
                /* Regla 10: menos unario requiere int o float */
                if (!es_numerico(t_op)) {
                    fprintf(stderr,
                            "Error semantico en linea %d: el operador unario '-' requiere un operando 'int' o 'float', pero recibio '%s'\n",
                            expr->linea, t_op);
                    errores_semanticos++;
                    return NULL;
                }
                anotar_tipo(expr, t_op);
                return expr->simbolo->tipo_dato;
            }

            if (strcmp(op, "!") == 0) {
                /* Regla 12: la negacion (!) requiere operando boolean */
                if (strcmp(t_op, "boolean") != 0) {
                    fprintf(stderr,
                            "Error semantico en linea %d: el operador '!' requiere un operando 'boolean', pero recibio '%s'\n",
                            expr->linea, t_op);
                    errores_semanticos++;
                    return NULL;
                }
                anotar_tipo(expr, "boolean");
                return expr->simbolo->tipo_dato;
            }

            return NULL;
        }

        case NODE_OP_BINARIA: {
            const char *op    = expr->simbolo->valor;
            const char *t_izq = chequear_expresion(expr->hijo1);
            const char *t_der = chequear_expresion(expr->hijo2);

            if (t_izq == NULL || t_der == NULL)
                return NULL; /* Alguno de los subarboles ya tuvo error */

            /* Operadores aritmeticos: +, -, *, / (Reglas 10 y 14) */
            if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
                strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
                if (!es_numerico(t_izq) || !es_numerico(t_der)) {
                    fprintf(stderr,
                            "Error semantico en linea %d: el operador '%s' requiere operandos 'int' o 'float' (recibio '%s' y '%s')\n",
                            expr->linea, op, t_izq, t_der);
                    errores_semanticos++;
                    return NULL;
                }
                /* Si alguno es float, el resultado se promueve a float (Regla 14) */
                const char *t_res = (strcmp(t_izq, "float") == 0 || strcmp(t_der, "float") == 0)
                                        ? "float"
                                        : "int";
                anotar_tipo(expr, t_res);
                return expr->simbolo->tipo_dato;
            }

            /* Operador resto de division entera: % (Regla 10 y especificacion pag. 5) */
            if (strcmp(op, "%") == 0) {
                if (!es_numerico(t_izq) || !es_numerico(t_der)) {
                    fprintf(stderr,
                            "Error semantico en linea %d: el operador '%%' requiere operandos numericos (recibio '%s' y '%s')\n",
                            expr->linea, t_izq, t_der);
                    errores_semanticos++;
                    return NULL;
                }
                anotar_tipo(expr, "int");
                return expr->simbolo->tipo_dato;
            }

            /* Operadores relacionales: <, > (Reglas 10 y 14) */
            if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0) {
                if (!es_numerico(t_izq) || !es_numerico(t_der)) {
                    fprintf(stderr,
                            "Error semantico en linea %d: el operador relacional '%s' requiere operandos 'int' o 'float' (recibio '%s' y '%s')\n",
                            expr->linea, op, t_izq, t_der);
                    errores_semanticos++;
                    return NULL;
                }
                anotar_tipo(expr, "boolean");
                return expr->simbolo->tipo_dato;
            }

            /* Operador de igualdad: == (Reglas 11 y 14) */
            if (strcmp(op, "==") == 0) {
                if (!tipos_compatibles(t_izq, t_der)) {
                    fprintf(stderr,
                            "Error semantico en linea %d: el operador '==' requiere operandos del mismo tipo o compatibles (recibio '%s' y '%s')\n",
                            expr->linea, t_izq, t_der);
                    errores_semanticos++;
                    return NULL;
                }
                anotar_tipo(expr, "boolean");
                return expr->simbolo->tipo_dato;
            }

            /* Operadores condicionales: &&, || (Regla 12) */
            if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
                if (strcmp(t_izq, "boolean") != 0 || strcmp(t_der, "boolean") != 0) {
                    fprintf(stderr,
                            "Error semantico en linea %d: el operador logico '%s' requiere operandos 'boolean' (recibio '%s' y '%s')\n",
                            expr->linea, op, t_izq, t_der);
                    errores_semanticos++;
                    return NULL;
                }
                anotar_tipo(expr, "boolean");
                return expr->simbolo->tipo_dato;
            }

            return NULL;
        }

        default:
            return NULL;
    }
}

/* ---------------------------------------------------------------------
 * Chequeo de sentencias y bloques (Reglas 6, 7, 8, 9, 13 y 14)
 * --------------------------------------------------------------------- */

static void chequear_sentencia(ASTNode *stmt) {
    if (stmt == NULL)
        return;

    switch (stmt->tipo) {
        case NODE_ASIGNACION: {
            const char *nombre  = stmt->simbolo->nombre_id;
            const char *t_expr  = chequear_expresion(stmt->hijo1);
            EntradaTS  *entrada = ts_buscar(nombre);

            /* Regla 2: variable declarada antes de su uso */
            if (entrada == NULL) {
                fprintf(stderr,
                        "Error semantico en linea %d: asignacion a variable '%s' no declarada\n",
                        stmt->linea, nombre);
                errores_semanticos++;
                return;
            }

            /* Regla 8: <location> debe ser variable local, global o parametro */
            if (entrada->categoria == CAT_FUNCION) {
                fprintf(stderr,
                        "Error semantico en linea %d: no se puede asignar un valor al metodo '%s'\n",
                        stmt->linea, nombre);
                errores_semanticos++;
                return;
            }

            const char *t_var = entrada->simbolo->tipo_dato;
            anotar_tipo(stmt, t_var);
            if (entrada->simbolo->valor != NULL) {
                free(stmt->simbolo->valor);
                stmt->simbolo->valor = strdup(entrada->simbolo->valor);
            }

            /* Reglas 13 y 14: compatibilidad de tipos en la asignacion */
            if (t_expr != NULL && t_var != NULL && !tipos_compatibles(t_var, t_expr)) {
                fprintf(stderr,
                        "Error semantico en linea %d: tipos incompatibles en asignacion a '%s' (variable '%s', expresion '%s')\n",
                        stmt->linea, nombre, t_var, t_expr);
                errores_semanticos++;
            }
            break;
        }

        case NODE_LLAMADA:
            /* Llamada a metodo como sentencia: puede ser void o retornar valor (se ignora) */
            chequear_llamada(stmt, 0);
            break;

        case NODE_IF: {
            /* Regla 9: la condicion del if debe ser boolean */
            const char *t_cond = chequear_expresion(stmt->hijo1);
            if (t_cond != NULL && strcmp(t_cond, "boolean") != 0) {
                fprintf(stderr,
                        "Error semantico en linea %d: la condicion del 'if' debe ser 'boolean', pero es '%s'\n",
                        stmt->linea, t_cond);
                errores_semanticos++;
            }
            chequear_bloque(stmt->hijo2, 1); /* bloque then */
            if (stmt->hijo3 != NULL)
                chequear_bloque(stmt->hijo3, 1); /* bloque else */
            break;
        }

        case NODE_WHILE: {
            /* Regla 9: la condicion del while debe ser boolean */
            const char *t_cond = chequear_expresion(stmt->hijo1);
            if (t_cond != NULL && strcmp(t_cond, "boolean") != 0) {
                fprintf(stderr,
                        "Error semantico en linea %d: la condicion del 'while' debe ser 'boolean', pero es '%s'\n",
                        stmt->linea, t_cond);
                errores_semanticos++;
            }
            chequear_bloque(stmt->hijo2, 1);
            break;
        }

        case NODE_RETORNO: {
            if (metodo_actual == NULL || metodo_actual->simbolo == NULL)
                return;

            const char *t_metodo = metodo_actual->simbolo->tipo_dato;
            const char *nom_met  = metodo_actual->simbolo->nombre_id;

            if (strcmp(t_metodo, "void") == 0) {
                /* Regla 6: un metodo void no puede retornar una expresion */
                if (stmt->hijo1 != NULL) {
                    chequear_expresion(stmt->hijo1);
                    fprintf(stderr,
                            "Error semantico en linea %d: el metodo '%s' es 'void' y no debe retornar un valor\n",
                            stmt->linea, nom_met);
                    errores_semanticos++;
                }
                anotar_tipo(stmt, "void");
            } else {
                /* Reglas 6 y 7: un metodo con retorno debe devolver una expresion compatible */
                if (stmt->hijo1 == NULL) {
                    fprintf(stderr,
                            "Error semantico en linea %d: el metodo '%s' debe retornar un valor de tipo '%s'\n",
                            stmt->linea, nom_met, t_metodo);
                    errores_semanticos++;
                } else {
                    const char *t_ret = chequear_expresion(stmt->hijo1);
                    if (t_ret != NULL && !tipos_compatibles(t_metodo, t_ret)) {
                        fprintf(stderr,
                                "Error semantico en linea %d: el tipo de retorno '%s' no es compatible con el tipo '%s' del metodo '%s'\n",
                                stmt->linea, t_ret, t_metodo, nom_met);
                        errores_semanticos++;
                    }
                }
                anotar_tipo(stmt, t_metodo);
            }
            break;
        }

        case NODE_BLOQUE:
            chequear_bloque(stmt, 1);
            break;

        default:
            break;
    }
}

static void chequear_bloque(ASTNode *bloque, int abrir_scope) {
    if (bloque == NULL || bloque->tipo != NODE_BLOQUE)
        return;

    if (abrir_scope)
        ts_abrir_nivel();

    /* 1. Declaraciones de variables locales del bloque */
    for (ASTNode *decl = bloque->hijo1; decl != NULL; decl = decl->siguiente)
        registrar_declaracion(decl, CAT_VAR_LOCAL);

    /* 2. Sentencias del bloque */
    for (ASTNode *stmt = bloque->hijo2; stmt != NULL; stmt = stmt->siguiente)
        chequear_sentencia(stmt);

    if (abrir_scope)
        ts_cerrar_nivel();
}

/* ---------------------------------------------------------------------
 * Chequeo de metodos y programa completo (Reglas 1, 2, 3)
 * --------------------------------------------------------------------- */

static void chequear_metodo(ASTNode *metodo) {
    if (metodo == NULL || metodo->simbolo == NULL)
        return;

    /* 1. Crear entrada del metodo en el nivel global (TS[0]) */
    EntradaTS *entrada_fn = ts_crear_entrada(metodo->simbolo, CAT_FUNCION, metodo->linea);

    for (ASTNode *param = metodo->hijo1; param != NULL; param = param->siguiente) {
        if (entrada_fn->cantidad_parametros >= MAX_PARAMETROS) {
            fprintf(stderr,
                    "Error semantico en linea %d: el metodo '%s' supera el maximo de parametros (%d)\n",
                    metodo->linea, metodo->simbolo->nombre_id, MAX_PARAMETROS);
            errores_semanticos++;
            break;
        }
        entrada_fn->tipos_parametros[entrada_fn->cantidad_parametros++] = param->simbolo->tipo_dato;
    }

    /* Regla 1: verificar que el nombre del metodo no este redeclarado en el scope global */
    if (!ts_insertar(entrada_fn)) {
        EntradaTS *previa = ts_buscar_en_nivel(metodo->simbolo->nombre_id, 0);
        fprintf(stderr,
                "Error semantico en linea %d: el identificador global '%s' ya fue declarado en la linea %d\n",
                metodo->linea,
                metodo->simbolo->nombre_id,
                previa != NULL ? previa->linea : metodo->linea);
        errores_semanticos++;
        free(entrada_fn);
    }

    /* 2. Abrir el scope del metodo (nivel 1): contiene tanto a sus
     *    parametros formales como a las variables locales de su bloque. */
    metodo_actual = metodo;
    contador_shadow = 0;
    ts_abrir_nivel();

    for (ASTNode *param = metodo->hijo1; param != NULL; param = param->siguiente)
        registrar_declaracion(param, CAT_PARAMETRO);

    /* El bloque principal del metodo usa el nivel recien abierto (abrir_scope = 0) */
    chequear_bloque(metodo->hijo2, 0);

    /* Si el metodo retorna un valor, verificar que no alcance el fin sin return */
    if (metodo->simbolo->tipo_dato != NULL &&
        strcmp(metodo->simbolo->tipo_dato, "void") != 0 &&
        !bloque_garantiza_retorno(metodo->hijo2)) {
        fprintf(stderr,
                "Error semantico en linea %d: el metodo '%s' (tipo '%s') puede alcanzar el final sin una sentencia 'return'\n",
                metodo->linea, metodo->simbolo->nombre_id, metodo->simbolo->tipo_dato);
        errores_semanticos++;
    }

    ts_cerrar_nivel();
    metodo_actual = NULL;
}

int analizar_semantica(ASTNode *raiz) {
    errores_semanticos = 0;

    if (raiz == NULL || raiz->tipo != NODE_PROGRAMA)
        return 0;

    ts_inicializar();

    int ya_hubo_metodo = 0;

    for (ASTNode *decl = raiz->hijo1; decl != NULL; decl = decl->siguiente) {
        if (decl->tipo == NODE_DECLARACION) {
            if (ya_hubo_metodo) {
                fprintf(stderr,
                        "Error semantico en linea %d: la variable global '%s' debe declararse antes de todos los metodos\n",
                        decl->linea, decl->simbolo->nombre_id);
                errores_semanticos++;
            }
            registrar_declaracion(decl, CAT_VAR_GLOBAL);
        } else if (decl->tipo == NODE_METODO) {
            ya_hubo_metodo = 1;
            chequear_metodo(decl);
        }
    }

    /* Regla 3: Todo programa debe contener un metodo llamado "main" sin parametros */
    EntradaTS *entrada_main = ts_buscar_en_nivel("main", 0);
    if (entrada_main == NULL || entrada_main->categoria != CAT_FUNCION) {
        fprintf(stderr,
                "Error semantico: el programa debe contener la declaracion de un metodo 'main'\n");
        errores_semanticos++;
    } else if (entrada_main->cantidad_parametros != 0) {
        fprintf(stderr,
                "Error semantico en linea %d: el metodo 'main' no debe tener parametros (tiene %d)\n",
                entrada_main->linea, entrada_main->cantidad_parametros);
        errores_semanticos++;
    }

    /* Limpiar el nivel global (TS[0]) antes de terminar, sin liberar los Simbolo* del AST */
    EntradaTS *actual = TS[0];
    while (actual != NULL) {
        EntradaTS *sig = actual->siguiente;
        free(actual);
        actual = sig;
    }
    TS[0] = NULL;

    return errores_semanticos;
}
