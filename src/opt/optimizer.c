#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/optimizer.h"

/* ---------------------------------------------------------------------
 * Funciones auxiliares de inspeccion de operandos e instrucciones
 * --------------------------------------------------------------------- */

static int es_temporal(const char *s) {
    if (s == NULL || s[0] != 't' || !isdigit((unsigned char)s[1]))
        return 0;
    for (int i = 1; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }
    return 1;
}

static int es_const_entera(const char *s) {
    if (s == NULL || *s == '\0')
        return 0;
    int i = (s[0] == '-') ? 1 : 0;
    if (s[i] == '\0')
        return 0;
    for (; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }
    return 1;
}

static int es_const_booleana(const char *s) {
    return s != NULL && (strcmp(s, "true") == 0 || strcmp(s, "false") == 0);
}

static int es_constante(const char *s) {
    return es_const_entera(s) || es_const_booleana(s);
}

static void reemplazar_str(char **campo, const char *nuevo) {
    if (campo == NULL)
        return;
    char *copia = (nuevo != NULL) ? strdup(nuevo) : NULL;
    free(*campo);
    *campo = copia;
}

/* Convierte una instruccion en una asignacion simple: res = nuevo_arg1 */
static void convertir_en_asignacion(InstruccionIR *inst, const char *nuevo_arg1) {
    inst->op = IR_ASN;
    reemplazar_str(&inst->arg1, nuevo_arg1);
    reemplazar_str(&inst->arg2, NULL);
}

/* Cuenta cuantas veces se lee el simbolo "nombre" como operando (arg1 o arg2)
 * dentro de la funcion actual (desde "inicio" hasta IR_FUNC_END). */
static int contar_usos_en_funcion(InstruccionIR *inicio, const char *nombre) {
    if (nombre == NULL)
        return 0;
    int usos = 0;
    for (InstruccionIR *i = inicio; i != NULL && i->op != IR_FUNC_END; i = i->siguiente) {
        if (i->arg1 != NULL && strcmp(i->arg1, nombre) == 0 && i->op != IR_CALL)
            usos++;
        if (i->arg2 != NULL && strcmp(i->arg2, nombre) == 0 && i->op != IR_CALL)
            usos++;
    }
    return usos;
}

/* ---------------------------------------------------------------------
 * 1. Plegado de Constantes (Constant Folding) y Simplificacion Algebraica
 * --------------------------------------------------------------------- */

static int pasada_constant_folding(InstruccionIR *lista) {
    int cambios = 0;
    char buf[64];

    for (InstruccionIR *i = lista; i != NULL; i = i->siguiente) {
        /* Operadores unarios sobre constantes */
        if (i->op == IR_NEG && es_const_entera(i->arg1)) {
            snprintf(buf, sizeof buf, "%d", -atoi(i->arg1));
            convertir_en_asignacion(i, buf);
            cambios++;
            continue;
        }
        if (i->op == IR_NOT && es_const_booleana(i->arg1)) {
            const char *neg = (strcmp(i->arg1, "true") == 0) ? "false" : "true";
            convertir_en_asignacion(i, neg);
            cambios++;
            continue;
        }

        /* Operadores binarios sobre dos constantes enteras */
        if (es_const_entera(i->arg1) && es_const_entera(i->arg2)) {
            int a = atoi(i->arg1);
            int b = atoi(i->arg2);
            switch (i->op) {
                case IR_ADD:
                    snprintf(buf, sizeof buf, "%d", a + b);
                    convertir_en_asignacion(i, buf);
                    cambios++;
                    continue;
                case IR_SUB:
                    snprintf(buf, sizeof buf, "%d", a - b);
                    convertir_en_asignacion(i, buf);
                    cambios++;
                    continue;
                case IR_MUL:
                    snprintf(buf, sizeof buf, "%d", a * b);
                    convertir_en_asignacion(i, buf);
                    cambios++;
                    continue;
                case IR_DIV:
                    if (b != 0) {
                        snprintf(buf, sizeof buf, "%d", a / b);
                        convertir_en_asignacion(i, buf);
                        cambios++;
                        continue;
                    }
                    break;
                case IR_MOD:
                    if (b != 0) {
                        snprintf(buf, sizeof buf, "%d", a % b);
                        convertir_en_asignacion(i, buf);
                        cambios++;
                        continue;
                    }
                    break;
                case IR_LT:
                    convertir_en_asignacion(i, (a < b) ? "true" : "false");
                    cambios++;
                    continue;
                case IR_GT:
                    convertir_en_asignacion(i, (a > b) ? "true" : "false");
                    cambios++;
                    continue;
                case IR_EQ:
                    convertir_en_asignacion(i, (a == b) ? "true" : "false");
                    cambios++;
                    continue;
                default:
                    break;
            }
        }

        /* Igualdad entre constantes booleanas */
        if (i->op == IR_EQ && es_const_booleana(i->arg1) && es_const_booleana(i->arg2)) {
            convertir_en_asignacion(i, (strcmp(i->arg1, i->arg2) == 0) ? "true" : "false");
            cambios++;
            continue;
        }

        /* Simplificaciones algebraicas (identidades con 0 y 1) */
        if (i->op == IR_ADD) {
            if (i->arg1 != NULL && strcmp(i->arg1, "0") == 0) {
                convertir_en_asignacion(i, i->arg2);
                cambios++;
                continue;
            }
            if (i->arg2 != NULL && strcmp(i->arg2, "0") == 0) {
                convertir_en_asignacion(i, i->arg1);
                cambios++;
                continue;
            }
        }
        if (i->op == IR_SUB) {
            if (i->arg2 != NULL && strcmp(i->arg2, "0") == 0) {
                convertir_en_asignacion(i, i->arg1);
                cambios++;
                continue;
            }
            if (i->arg1 != NULL && i->arg2 != NULL && strcmp(i->arg1, i->arg2) == 0) {
                convertir_en_asignacion(i, "0");
                cambios++;
                continue;
            }
        }
        if (i->op == IR_MUL) {
            if (i->arg1 != NULL && strcmp(i->arg1, "1") == 0) {
                convertir_en_asignacion(i, i->arg2);
                cambios++;
                continue;
            }
            if (i->arg2 != NULL && strcmp(i->arg2, "1") == 0) {
                convertir_en_asignacion(i, i->arg1);
                cambios++;
                continue;
            }
            if ((i->arg1 != NULL && strcmp(i->arg1, "0") == 0) ||
                (i->arg2 != NULL && strcmp(i->arg2, "0") == 0)) {
                convertir_en_asignacion(i, "0");
                cambios++;
                continue;
            }
        }
        if (i->op == IR_DIV) {
            if (i->arg2 != NULL && strcmp(i->arg2, "1") == 0) {
                convertir_en_asignacion(i, i->arg1);
                cambios++;
                continue;
            }
        }
    }

    return cambios;
}

/* ---------------------------------------------------------------------
 * 2. Propagacion de Constantes y Fusion de Copias de Temporales
 * --------------------------------------------------------------------- */

static int definiciones_de_temporal(InstruccionIR *inicio_fn, const char *tmp) {
    int defs = 0;
    for (InstruccionIR *i = inicio_fn; i != NULL && i->op != IR_FUNC_END; i = i->siguiente) {
        if (i->res != NULL && strcmp(i->res, tmp) == 0 && i->op != IR_LABEL)
            defs++;
    }
    return defs;
}

static int pasada_copy_propagation(InstruccionIR *lista) {
    int cambios = 0;
    InstruccionIR *inicio_fn = lista;

    for (InstruccionIR *i = lista; i != NULL; i = i->siguiente) {
        if (i->op == IR_FUNC_BEGIN) {
            inicio_fn = i;
            continue;
        }

        /* A. Si una instruccion es "t_k = constante" y t_k tiene una unica
         *    definicion, propagar esa constante a todos los usos de t_k. */
        if (i->op == IR_ASN && es_temporal(i->res) && es_constante(i->arg1) &&
            definiciones_de_temporal(inicio_fn, i->res) == 1) {
            for (InstruccionIR *u = i->siguiente;
                 u != NULL && u->op != IR_FUNC_END;
                 u = u->siguiente) {
                if (u->op != IR_CALL && u->arg1 != NULL && strcmp(u->arg1, i->res) == 0) {
                    reemplazar_str(&u->arg1, i->arg1);
                    cambios++;
                }
                if (u->op != IR_CALL && u->arg2 != NULL && strcmp(u->arg2, i->res) == 0) {
                    reemplazar_str(&u->arg2, i->arg1);
                    cambios++;
                }
            }
        }

        /* B. Fusion de temporal intermedio con su variable destino:
         *    Si "i" calcula un valor en "t_k" y la siguiente instruccion "sig"
         *    es "var = t_k" (donde t_k solo se usa en "sig"), escribir el
         *    resultado de "i" directamente en "var". */
        InstruccionIR *sig = i->siguiente;
        if (sig != NULL && sig->op == IR_ASN &&
            i->res != NULL && es_temporal(i->res) &&
            sig->arg1 != NULL && strcmp(sig->arg1, i->res) == 0 &&
            i->op != IR_LABEL && i->op != IR_JMP &&
            i->op != IR_JMP_FALSE && i->op != IR_JMP_TRUE &&
            definiciones_de_temporal(inicio_fn, i->res) == 1 &&
            contar_usos_en_funcion(inicio_fn, i->res) == 1) {

            reemplazar_str(&i->res, sig->res);
            reemplazar_str(&sig->arg1, sig->res);
            cambios++;
        }
    }

    return cambios;
}

/* ---------------------------------------------------------------------
 * 3. Eliminacion de Codigo Muerto, Saltos Constantes y Codigo Inalcanzable
 * --------------------------------------------------------------------- */

static void liberar_instruccion_unica(InstruccionIR *inst) {
    if (inst == NULL)
        return;
    free(inst->tipo);
    free(inst->arg1);
    free(inst->arg2);
    free(inst->res);
    free(inst);
}

static int contar_saltos_a_etiqueta(InstruccionIR *inicio_fn, const char *etiqueta) {
    if (etiqueta == NULL)
        return 0;
    int saltos = 0;
    for (InstruccionIR *i = inicio_fn; i != NULL && i->op != IR_FUNC_END; i = i->siguiente) {
        if ((i->op == IR_JMP || i->op == IR_JMP_FALSE || i->op == IR_JMP_TRUE) &&
            i->res != NULL && strcmp(i->res, etiqueta) == 0) {
            saltos++;
        }
    }
    return saltos;
}

static int pasada_dead_code_elimination(InstruccionIR *lista) {
    int cambios = 0;
    InstruccionIR *inicio_fn = lista;

    /* 3.1. Simplificar saltos condicionales cuya condicion ya es constante */
    for (InstruccionIR *i = lista; i != NULL; i = i->siguiente) {
        if (i->op == IR_JMP_FALSE && es_const_booleana(i->arg1)) {
            if (strcmp(i->arg1, "false") == 0) {
                i->op = IR_JMP;
                reemplazar_str(&i->arg1, NULL);
                cambios++;
            } else {
                i->op = IR_ASN;
                reemplazar_str(&i->arg1, NULL);
                reemplazar_str(&i->res, NULL);
                cambios++;
            }
        } else if (i->op == IR_JMP_TRUE && es_const_booleana(i->arg1)) {
            if (strcmp(i->arg1, "true") == 0) {
                i->op = IR_JMP;
                reemplazar_str(&i->arg1, NULL);
                cambios++;
            } else {
                i->op = IR_ASN;
                reemplazar_str(&i->arg1, NULL);
                reemplazar_str(&i->res, NULL);
                cambios++;
            }
        }
    }

    /* 3.2. Eliminar codigo inalcanzable inmediatamente despues de "goto" o "return"
     *      hasta la proxima etiqueta (IR_LABEL) o fin de funcion (IR_FUNC_END). */
    for (InstruccionIR *i = lista; i != NULL; i = i->siguiente) {
        if (i->op == IR_JMP || i->op == IR_RET) {
            while (i->siguiente != NULL &&
                   i->siguiente->op != IR_LABEL &&
                   i->siguiente->op != IR_FUNC_END &&
                   i->siguiente->op != IR_FUNC_BEGIN &&
                   i->siguiente->op != IR_LOCAL) {
                InstruccionIR *muerta = i->siguiente;
                i->siguiente = muerta->siguiente;
                liberar_instruccion_unica(muerta);
                cambios++;
            }
        }
    }

    /* 3.3. Eliminar asignaciones redundantes (x = x), saltos a etiqueta contigua,
     *      etiquetas sin referencias y temporales no leidos. */
    InstruccionIR *prev = NULL;
    InstruccionIR *act  = lista;

    while (act != NULL) {
        if (act->op == IR_FUNC_BEGIN)
            inicio_fn = act;

        int borrar = 0;

        /* NOP o autoasignacion "x = x" */
        if (act->op == IR_ASN &&
            ((act->res == NULL && act->arg1 == NULL) ||
             (act->res != NULL && act->arg1 != NULL && strcmp(act->res, act->arg1) == 0))) {
            borrar = 1;
        }

        /* Salto incondicional a la etiqueta inmediatamente siguiente */
        if (!borrar && act->op == IR_JMP && act->siguiente != NULL &&
            act->siguiente->op == IR_LABEL &&
            act->res != NULL && act->siguiente->res != NULL &&
            strcmp(act->res, act->siguiente->res) == 0) {
            borrar = 1;
        }

        /* Etiqueta (IR_LABEL) a la que ya no salta ninguna instruccion */
        if (!borrar && act->op == IR_LABEL && act->res != NULL &&
            contar_saltos_a_etiqueta(inicio_fn, act->res) == 0) {
            borrar = 1;
        }

        /* Temporal calculado pero nunca usado (sin contar llamadas a metodos) */
        if (!borrar && act->op != IR_CALL && act->op != IR_LABEL &&
            act->op != IR_JMP && act->op != IR_JMP_FALSE && act->op != IR_JMP_TRUE &&
            act->res != NULL && es_temporal(act->res) &&
            contar_usos_en_funcion(inicio_fn, act->res) == 0) {
            borrar = 1;
        }

        if (borrar && prev != NULL) {
            prev->siguiente = act->siguiente;
            liberar_instruccion_unica(act);
            act = prev->siguiente;
            cambios++;
        } else {
            prev = act;
            act  = act->siguiente;
        }
    }

    return cambios;
}

/* ---------------------------------------------------------------------
 * Punto de entrada principal del Optimizador
 * --------------------------------------------------------------------- */

int optimizar_ir(InstruccionIR *lista, const char *modo_opt) {
    if (lista == NULL || modo_opt == NULL)
        return 0;

    int hacer_cf  = (strcmp(modo_opt, "all") == 0 || strcmp(modo_opt, "cf") == 0);
    int hacer_cp  = (strcmp(modo_opt, "all") == 0 || strcmp(modo_opt, "cp") == 0);
    int hacer_dce = (strcmp(modo_opt, "all") == 0 || strcmp(modo_opt, "dce") == 0);

    int total_cambios = 0;
    int iteraciones   = 0;

    while (iteraciones < 32) {
        int cambios_ronda = 0;

        if (hacer_cf)
            cambios_ronda += pasada_constant_folding(lista);
        if (hacer_cp)
            cambios_ronda += pasada_copy_propagation(lista);
        if (hacer_dce)
            cambios_ronda += pasada_dead_code_elimination(lista);

        total_cambios += cambios_ronda;
        iteraciones++;

        if (cambios_ronda == 0)
            break;
    }

    return total_cambios;
}
