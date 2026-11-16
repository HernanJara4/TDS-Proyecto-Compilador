#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/ir.h"
#include "../../include/tabla.h"

InstruccionIR *codigo_ir = NULL;
static InstruccionIR *cola_ir = NULL;

static int contador_temp  = 0;
static int contador_label = 0;

/* Prototipos internos */
static void  gen_bloque(ASTNode *bloque);
static void  gen_sentencia(ASTNode *stmt);
static char *gen_expresion(ASTNode *expr);
static char *gen_llamada(ASTNode *llamada, int usar_resultado);

/* ---------------------------------------------------------------------
 * Constructores de temporales, etiquetas e instrucciones (Cuadruplas)
 * --------------------------------------------------------------------- */

static char *copiar_str(const char *s) {
    return (s != NULL) ? strdup(s) : NULL;
}

static char *nuevo_temporal(void) {
    char buf[32];
    snprintf(buf, sizeof buf, "t%d", contador_temp++);
    return strdup(buf);
}

static char *nueva_etiqueta(void) {
    char buf[32];
    snprintf(buf, sizeof buf, "L%d", contador_label++);
    return strdup(buf);
}

static InstruccionIR *emitir(OpIR op, const char *tipo,
                             const char *arg1, const char *arg2, const char *res) {
    InstruccionIR *inst = (InstruccionIR *)malloc(sizeof(InstruccionIR));
    if (inst == NULL) {
        fprintf(stderr, "Error interno: sin memoria para crear una instruccion IR\n");
        exit(1);
    }

    inst->op        = op;
    inst->tipo      = copiar_str(tipo);
    inst->arg1      = copiar_str(arg1);
    inst->arg2      = copiar_str(arg2);
    inst->res       = copiar_str(res);
    inst->siguiente = NULL;

    if (codigo_ir == NULL) {
        codigo_ir = inst;
        cola_ir   = inst;
    } else {
        cola_ir->siguiente = inst;
        cola_ir            = inst;
    }

    return inst;
}

/* Devuelve el nombre efectivo de un identificador para el IR:
 * si el analizador semantico le asigno un alias en simbolo->valor por
 * shadowing en un bloque anidado, usa ese alias; de lo contrario usa nombre_id. */
static const char *nombre_efectivo(ASTNode *nodo) {
    if (nodo == NULL || nodo->simbolo == NULL)
        return "?";
    if (nodo->simbolo->valor != NULL)
        return nodo->simbolo->valor;
    return nodo->simbolo->nombre_id;
}

/* ---------------------------------------------------------------------
 * Generacion de IR para llamadas a metodos
 * --------------------------------------------------------------------- */

static char *gen_llamada(ASTNode *llamada, int usar_resultado) {
    if (llamada == NULL || llamada->simbolo == NULL)
        return NULL;

    const char *nombre_fn = llamada->simbolo->nombre_id;
    const char *tipo_ret  = llamada->simbolo->tipo_dato;

    /* 1. Evaluar los argumentos de izquierda a derecha ANTES de emitir
     *    las instrucciones "param". Esto evita que llamadas anidadas como
     *    f(g(1), h(2)) mezclen los "param" de g/h con los de f. */
    char *operandos_args[MAX_PARAMETROS];
    const char *tipos_args[MAX_PARAMETROS];
    int cant_args = 0;

    for (ASTNode *arg = llamada->hijo1;
         arg != NULL && cant_args < MAX_PARAMETROS;
         arg = arg->siguiente) {
        operandos_args[cant_args] = gen_expresion(arg);
        tipos_args[cant_args]     = (arg->simbolo != NULL) ? arg->simbolo->tipo_dato : NULL;
        cant_args++;
    }

    /* 2. Emitir todos los "param" contiguos justo antes del "call" */
    for (int i = 0; i < cant_args; i++) {
        emitir(IR_ARG, tipos_args[i], operandos_args[i], NULL, NULL);
        free(operandos_args[i]);
    }

    char buf_cant[16];
    snprintf(buf_cant, sizeof buf_cant, "%d", cant_args);

    /* 3. Emitir la instruccion de llamada */
    if (usar_resultado && tipo_ret != NULL && strcmp(tipo_ret, "void") != 0) {
        char *tmp = nuevo_temporal();
        emitir(IR_CALL, tipo_ret, nombre_fn, buf_cant, tmp);
        return tmp;
    } else {
        emitir(IR_CALL, tipo_ret, nombre_fn, buf_cant, NULL);
        return NULL;
    }
}

/* ---------------------------------------------------------------------
 * Generacion de IR para expresiones (con corto circuito en && y ||)
 * --------------------------------------------------------------------- */

static char *gen_expresion(ASTNode *expr) {
    if (expr == NULL || expr->simbolo == NULL)
        return NULL;

    switch (expr->tipo) {
        case NODE_NRO:
        case NODE_BOOL:
            /* Las constantes literales devuelven directamente su representacion */
            return copiar_str(expr->simbolo->valor);

        case NODE_ID:
            /* Las variables devuelven su nombre efectivo (o alias si hubo shadowing) */
            return copiar_str(nombre_efectivo(expr));

        case NODE_LLAMADA:
            return gen_llamada(expr, 1);

        case NODE_OP_UNARIA: {
            const char *op   = expr->simbolo->valor;
            const char *tipo = expr->simbolo->tipo_dato;
            char *op1        = gen_expresion(expr->hijo1);
            char *tmp        = nuevo_temporal();

            if (strcmp(op, "-") == 0)
                emitir(IR_NEG, tipo, op1, NULL, tmp);
            else if (strcmp(op, "!") == 0)
                emitir(IR_NOT, tipo, op1, NULL, tmp);

            free(op1);
            return tmp;
        }

        case NODE_OP_BINARIA: {
            const char *op   = expr->simbolo->valor;
            const char *tipo = expr->simbolo->tipo_dato;

            /* Evaluacion de corto circuito para && (AND logico):
             * Si el primer operando es false, no evalua el segundo. */
            if (strcmp(op, "&&") == 0) {
                char *lbl_false = nueva_etiqueta();
                char *lbl_fin   = nueva_etiqueta();
                char *tmp_res   = nuevo_temporal();

                char *izq = gen_expresion(expr->hijo1);
                emitir(IR_JMP_FALSE, "boolean", izq, NULL, lbl_false);
                free(izq);

                char *der = gen_expresion(expr->hijo2);
                emitir(IR_ASN, "boolean", der, NULL, tmp_res);
                free(der);
                emitir(IR_JMP, NULL, NULL, NULL, lbl_fin);

                emitir(IR_LABEL, NULL, NULL, NULL, lbl_false);
                emitir(IR_ASN, "boolean", "false", NULL, tmp_res);

                emitir(IR_LABEL, NULL, NULL, NULL, lbl_fin);

                free(lbl_false);
                free(lbl_fin);
                return tmp_res;
            }

            /* Evaluacion de corto circuito para || (OR logico):
             * Si el primer operando es true, no evalua el segundo. */
            if (strcmp(op, "||") == 0) {
                char *lbl_true = nueva_etiqueta();
                char *lbl_fin  = nueva_etiqueta();
                char *tmp_res  = nuevo_temporal();

                char *izq = gen_expresion(expr->hijo1);
                emitir(IR_JMP_TRUE, "boolean", izq, NULL, lbl_true);
                free(izq);

                char *der = gen_expresion(expr->hijo2);
                emitir(IR_ASN, "boolean", der, NULL, tmp_res);
                free(der);
                emitir(IR_JMP, NULL, NULL, NULL, lbl_fin);

                emitir(IR_LABEL, NULL, NULL, NULL, lbl_true);
                emitir(IR_ASN, "boolean", "true", NULL, tmp_res);

                emitir(IR_LABEL, NULL, NULL, NULL, lbl_fin);

                free(lbl_true);
                free(lbl_fin);
                return tmp_res;
            }

            /* Operadores aritmeticos y relacionales (+, -, *, /, %, <, >, ==) */
            char *izq = gen_expresion(expr->hijo1);
            char *der = gen_expresion(expr->hijo2);
            char *tmp = nuevo_temporal();

            if      (strcmp(op, "+") == 0)  emitir(IR_ADD, tipo, izq, der, tmp);
            else if (strcmp(op, "-") == 0)  emitir(IR_SUB, tipo, izq, der, tmp);
            else if (strcmp(op, "*") == 0)  emitir(IR_MUL, tipo, izq, der, tmp);
            else if (strcmp(op, "/") == 0)  emitir(IR_DIV, tipo, izq, der, tmp);
            else if (strcmp(op, "%") == 0)  emitir(IR_MOD, tipo, izq, der, tmp);
            else if (strcmp(op, "<") == 0)  emitir(IR_LT,  tipo, izq, der, tmp);
            else if (strcmp(op, ">") == 0)  emitir(IR_GT,  tipo, izq, der, tmp);
            else if (strcmp(op, "==") == 0) emitir(IR_EQ,  tipo, izq, der, tmp);

            free(izq);
            free(der);
            return tmp;
        }

        default:
            return NULL;
    }
}

/* ---------------------------------------------------------------------
 * Generacion de IR para sentencias y bloques
 * --------------------------------------------------------------------- */

static void gen_sentencia(ASTNode *stmt) {
    if (stmt == NULL)
        return;

    switch (stmt->tipo) {
        case NODE_ASIGNACION: {
            const char *destino = nombre_efectivo(stmt);
            const char *t_dest  = stmt->simbolo->tipo_dato;
            char       *val     = gen_expresion(stmt->hijo1);

            emitir(IR_ASN, t_dest, val, NULL, destino);
            free(val);
            break;
        }

        case NODE_LLAMADA:
            gen_llamada(stmt, 0);
            break;

        case NODE_IF: {
            /* Esquema para if (cond) then_block [else else_block] */
            if (stmt->hijo3 == NULL) {
                char *lbl_fin = nueva_etiqueta();
                char *cond    = gen_expresion(stmt->hijo1);

                emitir(IR_JMP_FALSE, "boolean", cond, NULL, lbl_fin);
                free(cond);

                gen_bloque(stmt->hijo2);
                emitir(IR_LABEL, NULL, NULL, NULL, lbl_fin);

                free(lbl_fin);
            } else {
                char *lbl_else = nueva_etiqueta();
                char *lbl_fin  = nueva_etiqueta();
                char *cond     = gen_expresion(stmt->hijo1);

                emitir(IR_JMP_FALSE, "boolean", cond, NULL, lbl_else);
                free(cond);

                gen_bloque(stmt->hijo2);
                emitir(IR_JMP, NULL, NULL, NULL, lbl_fin);

                emitir(IR_LABEL, NULL, NULL, NULL, lbl_else);
                gen_bloque(stmt->hijo3);

                emitir(IR_LABEL, NULL, NULL, NULL, lbl_fin);

                free(lbl_else);
                free(lbl_fin);
            }
            break;
        }

        case NODE_WHILE: {
            /* Esquema para while cond body_block */
            char *lbl_inicio = nueva_etiqueta();
            char *lbl_fin    = nueva_etiqueta();

            emitir(IR_LABEL, NULL, NULL, NULL, lbl_inicio);

            char *cond = gen_expresion(stmt->hijo1);
            emitir(IR_JMP_FALSE, "boolean", cond, NULL, lbl_fin);
            free(cond);

            gen_bloque(stmt->hijo2);
            emitir(IR_JMP, NULL, NULL, NULL, lbl_inicio);

            emitir(IR_LABEL, NULL, NULL, NULL, lbl_fin);

            free(lbl_inicio);
            free(lbl_fin);
            break;
        }

        case NODE_RETORNO: {
            const char *t_metodo = (stmt->simbolo != NULL) ? stmt->simbolo->tipo_dato : "void";
            if (stmt->hijo1 != NULL) {
                char *val = gen_expresion(stmt->hijo1);
                emitir(IR_RET, t_metodo, val, NULL, NULL);
                free(val);
            } else {
                emitir(IR_RET, "void", NULL, NULL, NULL);
            }
            break;
        }

        case NODE_BLOQUE:
            gen_bloque(stmt);
            break;

        default:
            break;
    }
}

static void gen_bloque(ASTNode *bloque) {
    if (bloque == NULL || bloque->tipo != NODE_BLOQUE)
        return;

    /* 1. Emitir las declaraciones de variables locales del bloque */
    for (ASTNode *decl = bloque->hijo1; decl != NULL; decl = decl->siguiente) {
        emitir(IR_LOCAL, decl->simbolo->tipo_dato, NULL, NULL, nombre_efectivo(decl));
    }

    /* 2. Emitir las instrucciones para cada sentencia del bloque */
    for (ASTNode *stmt = bloque->hijo2; stmt != NULL; stmt = stmt->siguiente) {
        gen_sentencia(stmt);
    }
}

/* ---------------------------------------------------------------------
 * Punto de entrada principal del Generador de Codigo Intermedio
 * --------------------------------------------------------------------- */

static void gen_metodo(ASTNode *metodo) {
    if (metodo == NULL || metodo->simbolo == NULL)
        return;

    const char *nombre   = metodo->simbolo->nombre_id;
    const char *tipo_ret = metodo->simbolo->tipo_dato;

    emitir(IR_FUNC_BEGIN, tipo_ret, NULL, NULL, nombre);

    for (ASTNode *param = metodo->hijo1; param != NULL; param = param->siguiente) {
        emitir(IR_PARAM_DECL, param->simbolo->tipo_dato, NULL, NULL, nombre_efectivo(param));
    }

    gen_bloque(metodo->hijo2);

    emitir(IR_FUNC_END, tipo_ret, NULL, NULL, nombre);
}

InstruccionIR *generar_ir(ASTNode *raiz) {
    codigo_ir      = NULL;
    cola_ir        = NULL;
    contador_temp  = 0;
    contador_label = 0;

    if (raiz == NULL || raiz->tipo != NODE_PROGRAMA)
        return NULL;

    for (ASTNode *decl = raiz->hijo1; decl != NULL; decl = decl->siguiente) {
        if (decl->tipo == NODE_DECLARACION) {
            emitir(IR_GLOBAL, decl->simbolo->tipo_dato, NULL, NULL, decl->simbolo->nombre_id);
        } else if (decl->tipo == NODE_METODO) {
            gen_metodo(decl);
        }
    }

    return codigo_ir;
}

/* ---------------------------------------------------------------------
 * Impresion y liberacion de la lista de instrucciones IR
 * --------------------------------------------------------------------- */

void imprimir_ir(InstruccionIR *lista, FILE *out) {
    if (out == NULL)
        out = stdout;

    for (InstruccionIR *i = lista; i != NULL; i = i->siguiente) {
        switch (i->op) {
            case IR_GLOBAL:
                fprintf(out, "global %s %s\n", i->tipo, i->res);
                break;
            case IR_FUNC_BEGIN:
                fprintf(out, "\nfunc %s %s:\n", i->tipo, i->res);
                break;
            case IR_PARAM_DECL:
                fprintf(out, "    formal %s %s\n", i->tipo, i->res);
                break;
            case IR_LOCAL:
                fprintf(out, "    local %s %s\n", i->tipo, i->res);
                break;
            case IR_FUNC_END:
                fprintf(out, "endfunc %s\n", i->res);
                break;
            case IR_ASN:
                fprintf(out, "    %s = %s\n", i->res, i->arg1);
                break;
            case IR_ADD:
                fprintf(out, "    %s = %s + %s\n", i->res, i->arg1, i->arg2);
                break;
            case IR_SUB:
                fprintf(out, "    %s = %s - %s\n", i->res, i->arg1, i->arg2);
                break;
            case IR_MUL:
                fprintf(out, "    %s = %s * %s\n", i->res, i->arg1, i->arg2);
                break;
            case IR_DIV:
                fprintf(out, "    %s = %s / %s\n", i->res, i->arg1, i->arg2);
                break;
            case IR_MOD:
                fprintf(out, "    %s = %s %% %s\n", i->res, i->arg1, i->arg2);
                break;
            case IR_NEG:
                fprintf(out, "    %s = - %s\n", i->res, i->arg1);
                break;
            case IR_NOT:
                fprintf(out, "    %s = ! %s\n", i->res, i->arg1);
                break;
            case IR_LT:
                fprintf(out, "    %s = %s < %s\n", i->res, i->arg1, i->arg2);
                break;
            case IR_GT:
                fprintf(out, "    %s = %s > %s\n", i->res, i->arg1, i->arg2);
                break;
            case IR_EQ:
                fprintf(out, "    %s = %s == %s\n", i->res, i->arg1, i->arg2);
                break;
            case IR_LABEL:
                fprintf(out, "%s:\n", i->res);
                break;
            case IR_JMP:
                fprintf(out, "    goto %s\n", i->res);
                break;
            case IR_JMP_FALSE:
                fprintf(out, "    ifFalse %s goto %s\n", i->arg1, i->res);
                break;
            case IR_JMP_TRUE:
                fprintf(out, "    ifTrue %s goto %s\n", i->arg1, i->res);
                break;
            case IR_ARG:
                fprintf(out, "    param %s\n", i->arg1);
                break;
            case IR_CALL:
                if (i->res != NULL)
                    fprintf(out, "    %s = call %s, %s\n", i->res, i->arg1, i->arg2);
                else
                    fprintf(out, "    call %s, %s\n", i->arg1, i->arg2);
                break;
            case IR_RET:
                if (i->arg1 != NULL)
                    fprintf(out, "    return %s\n", i->arg1);
                else
                    fprintf(out, "    return\n");
                break;
        }
    }
}

void liberar_ir(InstruccionIR *lista) {
    while (lista != NULL) {
        InstruccionIR *sig = lista->siguiente;
        free(lista->tipo);
        free(lista->arg1);
        free(lista->arg2);
        free(lista->res);
        free(lista);
        lista = sig;
    }
}
