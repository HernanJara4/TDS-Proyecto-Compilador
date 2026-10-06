/* =====================================================================
 * typecheck.c
 * Motor semantico (analizador semantico) del compilador C-TDS - Etapa 2
 *
 * Que hace: recorre el AST que construyo el parser, poblando la Tabla
 * de Simbolos (TS) y verificando las 14 reglas semanticas de la
 * especificacion (Docs/01-TDS-spec-lenguaje), dejando ademas anotado
 * en cada nodo del AST el tipo resuelto de su expresion.
 *
 * Como funciona (esquema del recorrido):
 *
 *   analizar_semantica(raiz)
 *     |
 *     +-- por cada declaracion del ambito global, EN ORDEN DE FUENTE:
 *     |     NODE_DECLARACION -> se inserta en el nivel 0
 *     |     NODE_METODO      -> procesar_metodo()
 *     |         1) se inserta la firma (nombre, tipo de retorno,
 *     |            cantidad/tipos de parametros) en el nivel 0
 *     |         2) ts_abrir_nivel()  -> nivel 1
 *     |         3) se insertan los parametros en el nivel 1
 *     |         4) el cuerpo del metodo se procesa en ese MISMO nivel
 *     |            (los parametros son alcance-local del metodo)
 *     |         5) ts_cerrar_nivel()
 *     |
 *     +-- cualquier otro NODE_BLOQUE (if/while/bloque suelto) abre y
 *         cierra su propio nivel: ahi esta el anidamiento de scopes y
 *         el shadowing que exige la spec
 *     |
 *     +-- al final: verificar_main() (regla 3)
 *
 * Recorrer en orden de fuente es lo que hace valida la regla 2: el
 * simbolo se inserta recien cuando se encuentra su declaracion, luego
 * toda referencia anterior simplemente no se encuentra en la TS.
 *
 * Tipos: se manejan como char* ("int","float","boolean","void"),
 * igual que en el AST y en la TS (una sola representacion, comparada
 * con strcmp). Se usa un tipo sentinela TIPO_ERROR ("veneno"): toda
 * subexpresion que ya reporto un error devuelve TIPO_ERROR y los
 * chequeos padre la ignoran, para no emitir cascadas de errores
 * derivados del mismo problema.
 *
 * Reglas implementadas (numeracion = spec):
 *   1  identificador dos veces en el mismo ambito
 *   2  identificador usado antes de declararse
 *   3  existe main sin parametros
 *   4  cantidad y tipos de los argumentos de una invocacion
 *   5  una invocacion usada como expresion debe retornar valor
 *   6  return con/sin expresion segun el tipo de retorno del metodo
 *   7  tipo de la expresion del return = tipo de retorno del metodo
 *   8  un id usado como location debe ser variable/parametro
 *   9  la condicion de if/while debe ser boolean
 *   10 operandos de aritmeticos/relacionales int o float
 *   11 operandos de == del mismo tipo
 *   12 operandos de && || ! boolean
 *   13 tipos iguales en una asignacion
 *   14 (permiso) coerciones/truncamientos int <-> float: aplica a las
 *      comparaciones de los puntos 4, 7, 11 y 13
 *
 * Decisiones de diseno (ampliadas en la documentacion de la etapa):
 *   - Se sigue analizando luego de cada error para reportar todos los
 *     problemas de una pasada; el programa es valido solo si
 *     errores_semanticos == 0.
 *   - "%" exige operandos int: la spec describe % como "el resto de
 *     una division de numeros enteros".
 *   - Ademas de las 14 reglas numeradas, se aplica el requisito de la
 *     prosa de la spec: un metodo que retorna valor no puede alcanzar
 *     el fin del metodo sin ejecutar un return (se comprueba con un
 *     analisis de "todas las ramas retornan": return, if/else donde
 *     ambos casos retornan, o un bloque que retorna).
 * ===================================================================== */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/semantica.h"

/* ---------- Estado del analisis ---------- */

int errores_semanticos = 0;
static int debug_activo = 0;

/* Tipo "veneno": ya se reporto un error en esa subexpresion. Es la
 * misma direccion de memoria en todas las llamadas, por lo que la
 * comparacion por puntero (es_veneno) es valida. */
static const char * const TIPO_ERROR = "<error>";

void sem_set_debug(int activo) {
    debug_activo = activo;
}

/* ---------- Reporte de errores y traza (-debug) ---------- */

static void error_sem(int linea, const char *formato, ...) {
    va_list args;

    fprintf(stderr, "Error semantico en linea %d: ", linea);
    va_start(args, formato);
    vfprintf(stderr, formato, args);
    va_end(args);
    fputc('\n', stderr);

    errores_semanticos++;
}

static void traza(const char *formato, ...) {
    va_list args;

    if (!debug_activo)
        return;

    va_start(args, formato);
    vprintf(formato, args);
    va_end(args);
    putchar('\n');
}

/* ---------- Registro de declaraciones (solo para mensajes) ----------
 * La spec distingue "usado antes de declararse" (regla 2) de "no existe".
 * Para poder armar ese mensaje se recoge una vez, al empezar, la linea
 * de cada declaracion del programa; no participa de la verificacion. */

typedef struct DeclaracionVista {
    char *nombre;
    int linea;
    struct DeclaracionVista *siguiente;
} DeclaracionVista;

static DeclaracionVista *declaraciones = NULL;

static void registrar_declaraciones(ASTNode *nodo) {
    for (; nodo != NULL; nodo = nodo->siguiente) {
        if ((nodo->tipo == NODE_DECLARACION || nodo->tipo == NODE_METODO) &&
            nodo->simbolo != NULL && nodo->simbolo->nombre_id != NULL) {
            DeclaracionVista *nueva = (DeclaracionVista *)malloc(sizeof(DeclaracionVista));
            if (nueva == NULL) {
                fprintf(stderr, "Error interno: sin memoria para el registro de declaraciones\n");
                exit(1);
            }
            nueva->nombre = strdup(nodo->simbolo->nombre_id);
            nueva->linea = nodo->linea;
            nueva->siguiente = declaraciones;
            declaraciones = nueva;
        }
        registrar_declaraciones(nodo->hijo1);
        registrar_declaraciones(nodo->hijo2);
        registrar_declaraciones(nodo->hijo3);
    }
}

static void liberar_declaraciones(void) {
    while (declaraciones != NULL) {
        DeclaracionVista *sig = declaraciones->siguiente;
        free(declaraciones->nombre);
        free(declaraciones);
        declaraciones = sig;
    }
}

/* Reporta la regla 2 indicando, si existe, donde esta la declaracion
 * que el programador tenia en mente. "rol" es "el identificador" o
 * "el metodo", segun el contexto. */
static void error_no_declarado(int linea, const char *nombre, const char *rol) {
    for (DeclaracionVista *d = declaraciones; d != NULL; d = d->siguiente) {
        if (strcmp(d->nombre, nombre) == 0 && d->linea > linea) {
            error_sem(linea,
                      "%s '%s' no esta declarado en este punto "
                      "(hay una declaracion en la linea %d) (regla 2)",
                      rol, nombre, d->linea);
            return;
        }
    }
    error_sem(linea, "%s '%s' no esta declarado (regla 2)", rol, nombre);
}

/* ---------- Utilidades sobre tipos ---------- */

static const char *o_interrogacion(const char *s) {
    return s != NULL ? s : "?";
}

static int es_veneno(const char *t) {
    return t == NULL || t == TIPO_ERROR;
}

static int es_int(const char *t)     { return t != NULL && strcmp(t, "int") == 0; }
static int es_float(const char *t)   { return t != NULL && strcmp(t, "float") == 0; }
static int es_boolean(const char *t) { return t != NULL && strcmp(t, "boolean") == 0; }
static int es_void(const char *t)    { return t != NULL && strcmp(t, "void") == 0; }
static int es_numerico(const char *t){ return es_int(t) || es_float(t); }

/* Regla 14: entre int y float se permiten coerciones (int->float) y
 * truncamientos (float->int), asi que para las comparaciones de las
 * reglas 4, 7, 11 y 13 un par int/float se considera compatible. */
static int mismo_tipo(const char *a, const char *b) {
    if (a == NULL || b == NULL)
        return 0;
    if (strcmp(a, b) == 0)
        return 1;
    return (es_int(a) && es_float(b)) || (es_float(a) && es_int(b));
}

/* Anota el tipo resuelto en un nodo del AST. Ese es el tipo que luego
 * muestra el volcado .sem. No anota TIPO_ERROR (el nodo queda sin
 * tipo, lo que ya indica en el volcado que fallo). */
static void anotar(ASTNode *nodo, const char *tipo) {
    if (nodo == NULL || nodo->simbolo == NULL || es_veneno(tipo))
        return;
    if (nodo->simbolo->tipo_dato != NULL && strcmp(nodo->simbolo->tipo_dato, tipo) == 0)
        return;

    free(nodo->simbolo->tipo_dato);          /* propiedad del AST */
    nodo->simbolo->tipo_dato = strdup(tipo);
}

/* ---------- Acciones sobre la Tabla de Simbolos ---------- */

/* Inserta la declaracion "decl" en el nivel actual. Devuelve 1 si se
 * inserto; 0 si ya habia un identificador con ese nombre en el MISMO
 * ambito (regla 1): en ese caso reporta el error y libera el envoltorio
 * (el Simbolo sigue siendo del AST y no se toca). */
static int insertar_declaracion(ASTNode *decl, CategoriaSimbolo categoria) {
    EntradaTS *entrada = ts_crear_entrada(decl->simbolo, categoria, decl->linea);

    if (ts_insertar(entrada)) {
        traza("[TS] insertar nivel %d: '%s' : %s (%s) linea %d -> ok",
              nivel_actual,
              o_interrogacion(decl->simbolo->nombre_id),
              o_interrogacion(decl->simbolo->tipo_dato),
              ts_nombre_categoria(categoria),
              decl->linea);
        return 1;
    }

    EntradaTS *previa = ts_buscar_en_nivel(decl->simbolo->nombre_id, nivel_actual);
    error_sem(decl->linea,
              "el identificador '%s' ya esta declarado en este ambito "
              "(declarado previamente en linea %d) (regla 1)",
              decl->simbolo->nombre_id,
              previa != NULL ? previa->linea : 0);
    traza("[TS] insertar nivel %d: '%s' (%s) linea %d -> ERROR, ya declarado en el nivel",
          nivel_actual,
          o_interrogacion(decl->simbolo->nombre_id),
          ts_nombre_categoria(categoria),
          decl->linea);

    free(entrada);
    return 0;
}

/* Busca un identificador bajando desde el nivel actual hasta el 0
 * (resuelve el shadowing por el orden de busqueda) y deja traza del
 * nivel donde lo encontro. */
static EntradaTS *buscar(const char *nombre) {
    for (int nivel = nivel_actual; nivel >= 0; nivel--) {
        EntradaTS *encontrada = ts_buscar_en_nivel(nombre, nivel);
        if (encontrada != NULL) {
            traza("[TS] buscar '%s' -> nivel %d (%s)",
                  nombre, nivel, ts_nombre_categoria(encontrada->categoria));
            return encontrada;
        }
    }

    traza("[TS] buscar '%s' -> no encontrado", nombre);
    return NULL;
}

/* ---------- Análisis de expresiones (bottom-up) ---------- */

static const char *inferir_tipo(ASTNode *expr);

/* Recorre una invocacion: chequea reglas 4 (numero/tipos de argumentos)
 * y, si "como_expresion", la regla 5 (void no sirve como valor).
 * Devuelve el tipo de retorno de la funcion o TIPO_ERROR. */
static const char *inferir_llamada(ASTNode *call, int como_expresion) {
    const char *nombre = call->simbolo->nombre_id;
    EntradaTS *funcion = buscar(nombre);

    if (funcion == NULL || funcion->categoria != CAT_FUNCION) {
        /* El error del callee no inhibe el analisis de los argumentos:
         * adentro puede haber problemas propios (regla 2, etc.). */
        if (funcion == NULL)
            error_no_declarado(call->linea, nombre, "el metodo");
        else
            error_sem(call->linea, "'%s' no es una funcion: no se puede invocar (regla 4)",
                      nombre);

        for (ASTNode *arg = call->hijo1; arg != NULL; arg = arg->siguiente)
            inferir_tipo(arg);
        return TIPO_ERROR;
    }

    int cantidad_args = 0;
    for (ASTNode *arg = call->hijo1; arg != NULL; arg = arg->siguiente)
        cantidad_args++;

    int coincide_cantidad = (cantidad_args == funcion->cantidad_parametros);
    if (!coincide_cantidad) {
        error_sem(call->linea,
                  "la invocacion a '%s' recibe %d argumento/s pero '%s' declara %d (regla 4)",
                  nombre, cantidad_args, nombre, funcion->cantidad_parametros);
    }

    /* Anota el tipo de TODOS los argumentos (aunque sobren o falten,
     * para seguir encontrando errores anidados) y chequea los que
     * tienen parametro declarado (regla 4 + coercion de la regla 14). */
    int i = 0;
    for (ASTNode *arg = call->hijo1; arg != NULL; arg = arg->siguiente, i++) {
        const char *tipo_arg = inferir_tipo(arg);
        if (i >= funcion->cantidad_parametros || es_veneno(tipo_arg))
            continue;

        const char *tipo_formal = funcion->tipos_parametros[i];
        if (tipo_formal != NULL && !mismo_tipo(tipo_arg, tipo_formal)) {
            error_sem(arg->linea,
                      "el argumento %d de la invocacion a '%s' tiene tipo '%s' "
                      "y se esperaba '%s' (regla 4)",
                      i + 1, nombre, tipo_arg, tipo_formal);
        }
    }

    const char *retorno = funcion->simbolo->tipo_dato;
    anotar(call, retorno);

    if (como_expresion && es_void(retorno)) {
        error_sem(call->linea,
                  "la invocacion a la funcion void '%s' no puede usarse como "
                  "expresion (regla 5)", nombre);
        return TIPO_ERROR;
    }

    /* Con la cantidad mal no se confia en el tipo para no encadenar
     * errores en la expresion padre. */
    if (!coincide_cantidad)
        return TIPO_ERROR;

    return retorno;
}

static const char *inferir_binaria(ASTNode *expr) {
    const char *op = expr->simbolo->valor;   /* + - * / % < > == && || */
    const char *t_izq = inferir_tipo(expr->hijo1);
    const char *t_der = inferir_tipo(expr->hijo2);

    if (es_veneno(t_izq) || es_veneno(t_der))
        return TIPO_ERROR;

    /* Regla 12: operadores de corto circuito */
    if (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) {
        if (!es_boolean(t_izq) || !es_boolean(t_der)) {
            error_sem(expr->linea,
                      "los operandos de '%s' deben ser boolean, pero son '%s' y '%s' (regla 12)",
                      op, t_izq, t_der);
            return TIPO_ERROR;
        }
        anotar(expr, "boolean");
        return "boolean";
    }

    /* Regla 11: igualdad entre tipos iguales (con coercion de la 14) */
    if (strcmp(op, "==") == 0) {
        if (!mismo_tipo(t_izq, t_der)) {
            error_sem(expr->linea,
                      "los operandos de '==' deben tener el mismo tipo, "
                      "pero son '%s' y '%s' (regla 11)", t_izq, t_der);
            return TIPO_ERROR;
        }
        anotar(expr, "boolean");
        return "boolean";
    }

    /* Regla 10: relacionales sobre tipos numericos */
    if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0) {
        if (!es_numerico(t_izq) || !es_numerico(t_der)) {
            error_sem(expr->linea,
                      "los operandos de '%s' deben ser int o float, "
                      "pero son '%s' y '%s' (regla 10)", op, t_izq, t_der);
            return TIPO_ERROR;
        }
        anotar(expr, "boolean");
        return "boolean";
    }

    /* Regla 10: aritmeticos. % exige enteros porque la spec lo define
     * como resto de una division de numeros enteros. */
    if (!es_numerico(t_izq) || !es_numerico(t_der)) {
        error_sem(expr->linea,
                  "los operandos de '%s' deben ser int o float, "
                  "pero son '%s' y '%s' (regla 10)", op, t_izq, t_der);
        return TIPO_ERROR;
    }
    if (strcmp(op, "%") == 0 && (!es_int(t_izq) || !es_int(t_der))) {
        error_sem(expr->linea,
                  "los operandos de '%%' deben ser int, pero son '%s' y '%s' (regla 10)",
                  t_izq, t_der);
        return TIPO_ERROR;
    }

    const char *resultado = (es_float(t_izq) || es_float(t_der)) ? "float" : "int";
    anotar(expr, resultado);
    return resultado;
}

static const char *inferir_unaria(ASTNode *expr) {
    const char *op = expr->simbolo->valor;   /* "-" o "!" */
    const char *tipo = inferir_tipo(expr->hijo1);

    if (es_veneno(tipo))
        return TIPO_ERROR;

    if (strcmp(op, "!") == 0) {
        if (!es_boolean(tipo)) {
            error_sem(expr->linea,
                      "el operando de '!' debe ser boolean, pero es '%s' (regla 12)", tipo);
            return TIPO_ERROR;
        }
        anotar(expr, "boolean");
        return "boolean";
    }

    /* menos unario: regla 10 */
    if (!es_numerico(tipo)) {
        error_sem(expr->linea,
                  "el operando del menos unario debe ser int o float, "
                  "pero es '%s' (regla 10)", tipo);
        return TIPO_ERROR;
    }
    anotar(expr, tipo);
    return tipo;
}

/* Devuelve el tipo de la expresion y lo anota en el nodo. */
static const char *inferir_tipo(ASTNode *expr) {
    if (expr == NULL)
        return TIPO_ERROR;

    switch (expr->tipo) {
        case NODE_NRO:
        case NODE_BOOL:
            /* ya vienen tipados desde el parser */
            return expr->simbolo != NULL ? expr->simbolo->tipo_dato : TIPO_ERROR;

        case NODE_ID: {
            EntradaTS *entrada = buscar(expr->simbolo->nombre_id);
            if (entrada == NULL) {
                error_no_declarado(expr->linea, expr->simbolo->nombre_id, "el identificador");
                return TIPO_ERROR;
            }
            if (entrada->categoria == CAT_FUNCION) {
                error_sem(expr->linea,
                          "'%s' es una funcion y no puede usarse como valor (regla 8)",
                          expr->simbolo->nombre_id);
                return TIPO_ERROR;
            }
            anotar(expr, entrada->simbolo->tipo_dato);
            return entrada->simbolo->tipo_dato;
        }

        case NODE_LLAMADA:
            return inferir_llamada(expr, 1);

        case NODE_OP_BINARIA:
            return inferir_binaria(expr);

        case NODE_OP_UNARIA:
            return inferir_unaria(expr);

        default:
            return TIPO_ERROR;   /* no deberia llegar: no es una expresion */
    }
}

/* ---------- Análisis de sentencias ---------- */

static void procesar_bloque(ASTNode *bloque, const char *retorno);
static void procesar_bloque_abierto(ASTNode *bloque, const char *retorno);

/* Regla 8 + reglas 13/14 de la asignacion. */
static void procesar_asignacion(ASTNode *asig) {
    const char *nombre = asig->simbolo->nombre_id;
    EntradaTS *loc = buscar(nombre);
    const char *tipo_loc = NULL;

    if (loc == NULL) {
        error_no_declarado(asig->linea, nombre, "el identificador");
    } else if (loc->categoria == CAT_FUNCION) {
        error_sem(asig->linea,
                  "no se le puede asignar a '%s': es una funcion (regla 8)", nombre);
    } else {
        tipo_loc = loc->simbolo->tipo_dato;
    }

    /* La expresion se analiza igual, para seguir encontrando errores
     * adentro (aunque el destino este mal o no se conozca su tipo). */
    const char *tipo_expr = inferir_tipo(asig->hijo1);

    if (tipo_loc == NULL || es_veneno(tipo_expr))
        return;

    anotar(asig, tipo_loc);
    if (!mismo_tipo(tipo_loc, tipo_expr)) {
        error_sem(asig->linea,
                  "tipos incompatibles en la asignacion a '%s': se esperaba '%s' "
                  "y la expresion es de tipo '%s' (regla 13)",
                  nombre, o_interrogacion(tipo_loc), tipo_expr);
    }
}

/* Regla 6 + regla 7 del return. */
static void procesar_retorno(ASTNode *ret, const char *retorno) {
    ASTNode *expr = ret->hijo1;

    if (es_void(retorno)) {
        if (expr != NULL) {
            error_sem(ret->linea,
                      "el metodo es void: su sentencia return no puede tener "
                      "una expresion (regla 6)");
            inferir_tipo(expr);   /* igual se recorre para hallar errores internos */
        }
        return;
    }

    if (expr == NULL) {
        error_sem(ret->linea,
                  "el metodo retorna '%s': su sentencia return debe tener "
                  "una expresion (regla 6)", o_interrogacion(retorno));
        return;
    }

    const char *tipo = inferir_tipo(expr);
    if (es_veneno(tipo))
        return;

    if (!mismo_tipo(tipo, retorno)) {
        error_sem(ret->linea,
                  "la expresion del return tiene tipo '%s' y el metodo "
                  "retorna '%s' (regla 7)", tipo, o_interrogacion(retorno));
    }
}

/* Regla 9 de las condiciones de control. */
static void procesar_condicion(ASTNode *sentencia, const char *nombre_sentencia) {
    const char *tipo = inferir_tipo(sentencia->hijo1);

    if (es_veneno(tipo))
        return;
    if (!es_boolean(tipo)) {
        error_sem(sentencia->linea,
                  "la condicion del %s debe ser boolean, pero es de tipo '%s' (regla 9)",
                  nombre_sentencia, tipo);
    }
}

static void procesar_sentencia(ASTNode *s, const char *retorno) {
    if (s == NULL)
        return;

    switch (s->tipo) {
        case NODE_ASIGNACION:
            procesar_asignacion(s);
            break;

        case NODE_LLAMADA:
            /* como sentencia: void y no-void estan permitidos (la spec
             * dice que el resultado no-void puede ignorarse) */
            inferir_llamada(s, 0);
            break;

        case NODE_IF:
            procesar_condicion(s, "if");
            procesar_bloque(s->hijo2, retorno);
            if (s->hijo3 != NULL)
                procesar_bloque(s->hijo3, retorno);
            break;

        case NODE_WHILE:
            procesar_condicion(s, "while");
            procesar_bloque(s->hijo2, retorno);
            break;

        case NODE_RETORNO:
            procesar_retorno(s, retorno);
            break;

        case NODE_BLOQUE:
            procesar_bloque(s, retorno);
            break;

        default:
            break;   /* cualquier otro caso no es una sentencia */
    }
}

/* ---------- Scopes (pila de niveles) ---------- */

/* Procesa el contenido de un bloque EN EL NIVEL YA ABIERTO:
 * primero las declaraciones de variables (hijo1), despues las
 * sentencias (hijo2). Asi se respeta el orden de la gramatica y la
 * regla 2 dentro del bloque. */
static void procesar_bloque_abierto(ASTNode *bloque, const char *retorno) {
    if (bloque == NULL)
        return;

    CategoriaSimbolo categoria = (nivel_actual == 0) ? CAT_VAR_GLOBAL : CAT_VAR_LOCAL;
    for (ASTNode *decl = bloque->hijo1; decl != NULL; decl = decl->siguiente)
        insertar_declaracion(decl, categoria);

    for (ASTNode *s = bloque->hijo2; s != NULL; s = s->siguiente)
        procesar_sentencia(s, retorno);
}

/* Abre un scope nuevo, procesa el bloque y lo cierra (bloques de if,
 * while o bloques sueltos: cada { } es un ambito). */
static void procesar_bloque(ASTNode *bloque, const char *retorno) {
    if (bloque == NULL)
        return;

    ts_abrir_nivel();
    traza("[TS] abrir nivel %d", nivel_actual);

    procesar_bloque_abierto(bloque, retorno);

    traza("[TS] cerrar nivel %d (%d entrada/s)", nivel_actual, ts_contar_entradas(nivel_actual));
    ts_cerrar_nivel();
}

/* ---------- Análisis de métodos ---------- */

/* "Las sentencias return solo..." - la spec prohíbe que un metodo que
 * retorna valor alcance el fin del metodo. Se comprueba si TODAS las
 * ramas retornan: una sentencia garantiza el retorno si es un return,
 * un if/else donde ambos casos retornan, o un bloque que retorna.
 * while nunca garantiza (al terminar la condicion se sale del ciclo). */
static int sentencias_garantizan_return(ASTNode *lista);

static int bloque_garantiza_return(ASTNode *bloque) {
    if (bloque == NULL)
        return 0;
    return sentencias_garantizan_return(bloque->hijo2);
}

static int sentencia_garantiza_return(ASTNode *s) {
    if (s == NULL)
        return 0;

    switch (s->tipo) {
        case NODE_RETORNO:
            return 1;
        case NODE_IF:
            /* solo if con else puede garantizar, y solo si ambas
             * ramas retornan */
            return s->hijo3 != NULL &&
                   bloque_garantiza_return(s->hijo2) &&
                   bloque_garantiza_return(s->hijo3);
        case NODE_BLOQUE:
            return sentencias_garantizan_return(s->hijo2);
        default:
            return 0;
    }
}

static int sentencias_garantizan_return(ASTNode *lista) {
    for (ASTNode *s = lista; s != NULL; s = s->siguiente) {
        if (sentencia_garantiza_return(s))
            return 1;
    }
    return 0;
}

/* Procesa una declaracion de metodo completa (reglas 1, 2, 4, 6, 7,
 * 9... mas el requisito de retorno de la spec). */
static void procesar_metodo(ASTNode *metodo) {
    const char *retorno = metodo->simbolo->tipo_dato;

    /* 1) firma en el nivel global (nivel actual) */
    EntradaTS *firma = ts_crear_entrada(metodo->simbolo, CAT_FUNCION, metodo->linea);

    int i = 0;
    for (ASTNode *p = metodo->hijo1; p != NULL; p = p->siguiente, i++) {
        if (i >= MAX_PARAMETROS) {
            error_sem(metodo->linea,
                      "el metodo '%s' declara mas de %d parametros",
                      metodo->simbolo->nombre_id, MAX_PARAMETROS);
            i = MAX_PARAMETROS;   /* no se desborda el arreglo */
            break;
        }
        firma->cantidad_parametros = i + 1;
        firma->tipos_parametros[i] = p->simbolo->tipo_dato;   /* prestado del AST */
    }

    if (ts_insertar(firma)) {
        traza("[TS] insertar nivel %d: '%s' : %s (funcion) linea %d -> ok "
              "(%d parametro/s)",
              nivel_actual, metodo->simbolo->nombre_id, o_interrogacion(retorno),
              metodo->linea, firma->cantidad_parametros);
    } else {
        EntradaTS *previa = ts_buscar_en_nivel(metodo->simbolo->nombre_id, nivel_actual);
        error_sem(metodo->linea,
                  "el identificador '%s' ya esta declarado en este ambito "
                  "(declarado previamente en linea %d) (regla 1)",
                  metodo->simbolo->nombre_id, previa != NULL ? previa->linea : 0);
        traza("[TS] insertar nivel %d: '%s' (funcion) linea %d -> ERROR, "
              "ya declarado en el nivel",
              nivel_actual, metodo->simbolo->nombre_id, metodo->linea);
        free(firma);
        firma = NULL;
    }

    /* 2) ambito del metodo: los parametros y el cuerpo comparten nivel */
    ts_abrir_nivel();
    traza("[TS] abrir nivel %d", nivel_actual);

    for (ASTNode *p = metodo->hijo1; p != NULL; p = p->siguiente)
        insertar_declaracion(p, CAT_PARAMETRO);

    procesar_bloque_abierto(metodo->hijo2, retorno);

    if (!es_void(retorno) && metodo->hijo2 != NULL &&
        !bloque_garantiza_return(metodo->hijo2)) {
        error_sem(metodo->linea,
                  "el metodo '%s' retorna '%s' pero puede alcanzar el fin del "
                  "metodo sin ejecutar una sentencia return",
                  metodo->simbolo->nombre_id, o_interrogacion(retorno));
    }

    traza("[TS] cerrar nivel %d (%d entrada/s)", nivel_actual, ts_contar_entradas(nivel_actual));
    ts_cerrar_nivel();
}

/* ---------- Regla 3: el programa debe definir main sin parámetros ---------- */

static void verificar_main(ASTNode *raiz) {
    EntradaTS *main_ = ts_buscar_en_nivel("main", 0);

    if (main_ == NULL) {
        error_sem(raiz->linea, "el programa no define el metodo main (regla 3)");
        return;
    }
    if (main_->categoria != CAT_FUNCION) {
        error_sem(main_->linea, "main debe ser un metodo, no una variable (regla 3)");
        return;
    }
    if (main_->cantidad_parametros != 0) {
        error_sem(main_->linea, "el metodo main no debe tener parametros (regla 3)");
    }
}

/* ---------- Punto de entrada ---------- */

int analizar_semantica(ASTNode *raiz) {
    errores_semanticos = 0;
    ts_inicializar();

    if (raiz == NULL || raiz->tipo != NODE_PROGRAMA) {
        fprintf(stderr, "Error interno: el analisis semantico requiere un "
                        "arbol NODE_PROGRAMA\n");
        return 1;
    }

    traza("[TS] nivel inicial: 0 (ambito global)");

    /* registro de declaraciones: solo para el mensaje de la regla 2 */
    registrar_declaraciones(raiz);

    /* Una sola pasada, en orden de fuente: es lo que hace que valga la
     * regla 2 (usar antes de declarar simplemente no se encuentra). */
    for (ASTNode *decl = raiz->hijo1; decl != NULL; decl = decl->siguiente) {
        if (decl->tipo == NODE_DECLARACION)
            insertar_declaracion(decl, CAT_VAR_GLOBAL);
        else if (decl->tipo == NODE_METODO)
            procesar_metodo(decl);
    }

    verificar_main(raiz);
    liberar_declaraciones();
    return errores_semanticos;
}

/* ---------- Volcado de la etapa (.sem) ---------- */

void volcar_resultado_semantico(FILE *archivo, ASTNode *raiz) {
    fprintf(archivo, "=== Arbol de Sintaxis Abstracta (anotado con tipos) ===\n");
    imprimir_ast_en(archivo, raiz, 0);

    fprintf(archivo, "\n=== Tabla de Simbolos ===\n");
    fprintf(archivo, "Nota: al terminar el analisis solo persiste el ambito "
                     "global, los niveles locales se destruyen al cerrarse.\n");
    ts_imprimir(archivo);
}
