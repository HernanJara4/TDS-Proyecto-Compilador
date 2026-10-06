#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/tabla.h"

EntradaTS *TS[MAX_NIVELES];
int        nivel_actual = 0;

/* ---------------------------------------------------------------------
 * Ciclo de vida de la tabla y de los niveles
 * --------------------------------------------------------------------- */

void ts_inicializar(void) {
    for (int i = 0; i < MAX_NIVELES; i++)
        TS[i] = NULL;
    nivel_actual = 0;
}

void ts_abrir_nivel(void) {
    if (nivel_actual + 1 >= MAX_NIVELES) {
        fprintf(stderr,
                "Error interno: se supero la profundidad maxima de bloques anidados (%d)\n",
                MAX_NIVELES);
        exit(1);
    }
    nivel_actual++;
    TS[nivel_actual] = NULL;   /* por las dudas, aunque deberia estar limpio */
}

void ts_cerrar_nivel(void) {
    EntradaTS *actual = TS[nivel_actual];
    while (actual != NULL) {
        EntradaTS *siguiente = actual->siguiente;
        free(actual);   /* NO se libera actual->simbolo: es del AST, ver ts.h */
        actual = siguiente;
    }
    TS[nivel_actual] = NULL;
    nivel_actual--;
}

/* ---------------------------------------------------------------------
 * Construccion de entradas
 * --------------------------------------------------------------------- */

EntradaTS *ts_crear_entrada(Simbolo *simbolo, CategoriaSimbolo categoria, int linea) {
    EntradaTS *entrada = (EntradaTS *)malloc(sizeof(EntradaTS));
    if (entrada == NULL) {
        fprintf(stderr, "Error interno: sin memoria para crear una entrada de la TS\n");
        exit(1);
    }

    entrada->simbolo   = simbolo;
    entrada->categoria = categoria;
    entrada->linea     = linea;

    entrada->cantidad_parametros = 0;
    for (int i = 0; i < MAX_PARAMETROS; i++)
        entrada->tipos_parametros[i] = NULL;

    entrada->siguiente = NULL;
    return entrada;
}

/* ---------------------------------------------------------------------
 * Busqueda e insercion
 * --------------------------------------------------------------------- */

EntradaTS *ts_buscar_en_nivel(const char *nombre, int nivel) {
    if (nombre == NULL || nivel < 0 || nivel >= MAX_NIVELES)
        return NULL;

    for (EntradaTS *e = TS[nivel]; e != NULL; e = e->siguiente) {
        if (e->simbolo != NULL && e->simbolo->nombre_id != NULL &&
            strcmp(e->simbolo->nombre_id, nombre) == 0) {
            return e;
        }
    }
    return NULL;
}

EntradaTS *ts_buscar(const char *nombre) {
    for (int nivel = nivel_actual; nivel >= 0; nivel--) {
        EntradaTS *encontrada = ts_buscar_en_nivel(nombre, nivel);
        if (encontrada != NULL)
            return encontrada;
    }
    return NULL;
}

int ts_insertar(EntradaTS *entrada) {
    if (entrada == NULL || entrada->simbolo == NULL || entrada->simbolo->nombre_id == NULL) {
        fprintf(stderr, "Error interno: se intento insertar una entrada invalida en la TS\n");
        exit(1);
    }

    if (ts_buscar_en_nivel(entrada->simbolo->nombre_id, nivel_actual) != NULL)
        return 0;   /* ya declarado en este mismo nivel: no se inserta */

    entrada->siguiente = TS[nivel_actual];
    TS[nivel_actual] = entrada;
    return 1;
}

/* ---------------------------------------------------------------------
 * Utilidades de consulta / volcado
 * --------------------------------------------------------------------- */

const char *ts_nombre_categoria(CategoriaSimbolo categoria) {
    switch (categoria) {
        case CAT_VAR_GLOBAL: return "variable global";
        case CAT_VAR_LOCAL:  return "variable local";
        case CAT_PARAMETRO:  return "parametro";
        case CAT_FUNCION:    return "funcion";
    }
    return "?";
}

int ts_contar_entradas(int nivel) {
    if (nivel < 0 || nivel >= MAX_NIVELES)
        return 0;

    int n = 0;
    for (EntradaTS *e = TS[nivel]; e != NULL; e = e->siguiente)
        n++;
    return n;
}

void ts_imprimir(FILE *f) {
    for (int nivel = 0; nivel < MAX_NIVELES; nivel++) {
        if (TS[nivel] == NULL)
            continue;

        fprintf(f, "Nivel %d%s:\n", nivel, nivel == 0 ? " (global)" : "");

        for (EntradaTS *e = TS[nivel]; e != NULL; e = e->siguiente) {
            const char *nombre = (e->simbolo && e->simbolo->nombre_id) ? e->simbolo->nombre_id : "?";
            const char *tipo   = (e->simbolo && e->simbolo->tipo_dato) ? e->simbolo->tipo_dato : "?";

            fprintf(f, "  '%s' : %s  (%s)  linea %d",
                    nombre, tipo, ts_nombre_categoria(e->categoria), e->linea);

            if (e->categoria == CAT_FUNCION) {
                if (e->cantidad_parametros == 0) {
                    fprintf(f, ", sin parametros");
                } else {
                    fprintf(f, ", %d parametro%s: [", e->cantidad_parametros,
                            e->cantidad_parametros == 1 ? "" : "s");
                    for (int i = 0; i < e->cantidad_parametros; i++) {
                        const char *tp = e->tipos_parametros[i] ? e->tipos_parametros[i] : "?";
                        fprintf(f, "%s%s", i > 0 ? ", " : "", tp);
                    }
                    fprintf(f, "]");
                }
            }
            fputc('\n', f);
        }
    }
}
