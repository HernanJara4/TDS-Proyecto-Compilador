#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/codegen.h"

#define MAX_SLOTS_FRAME 512

/* Tabla de ubicaciones en el Stack Frame de la funcion actual.
 * Cada parametro, variable local y temporal recibe 8 bytes en -offset(%rbp). */
typedef struct {
    char *nombre;
    int   offset; /* positivo aqui; se emite como -offset(%rbp) */
} SlotLocal;

static SlotLocal tabla_frame[MAX_SLOTS_FRAME];
static int       cant_slots          = 0;
static int       total_params_actual = 0;

/* Registros para los primeros 6 argumentos segun System V AMD64 ABI */
static const char *REGS_ARGS[6] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"
};

/* ---------------------------------------------------------------------
 * Manejo de la tabla de Stack Frame por funcion
 * --------------------------------------------------------------------- */

static void limpiar_frame(void) {
    for (int i = 0; i < cant_slots; i++)
        free(tabla_frame[i].nombre);
    cant_slots = 0;
}

static SlotLocal *buscar_slot(const char *nombre) {
    if (nombre == NULL)
        return NULL;
    for (int i = 0; i < cant_slots; i++) {
        if (strcmp(tabla_frame[i].nombre, nombre) == 0)
            return &tabla_frame[i];
    }
    return NULL;
}

static int registrar_slot(const char *nombre) {
    if (nombre == NULL)
        return 0;

    SlotLocal *existente = buscar_slot(nombre);
    if (existente != NULL)
        return existente->offset;

    if (cant_slots >= MAX_SLOTS_FRAME) {
        fprintf(stderr, "Error interno: se supero el limite de variables/temporales por funcion (%d)\n",
                MAX_SLOTS_FRAME);
        exit(1);
    }

    int offset = (cant_slots + 1) * 8;
    tabla_frame[cant_slots].nombre = strdup(nombre);
    tabla_frame[cant_slots].offset = offset;
    cant_slots++;
    return offset;
}

/* Recorre las instrucciones desde IR_FUNC_BEGIN hasta IR_FUNC_END para
 * reservar lugar en el Stack Frame a todos los parametros, locales y
 * temporales de la funcion, devolviendo el tamano total alineado a 16 bytes. */
static int construir_frame_funcion(InstruccionIR *inicio) {
    limpiar_frame();
    total_params_actual = 0;

    for (InstruccionIR *i = inicio->siguiente;
         i != NULL && i->op != IR_FUNC_END;
         i = i->siguiente) {
        switch (i->op) {
            case IR_PARAM_DECL:
                total_params_actual++;
                registrar_slot(i->res);
                break;

            case IR_LOCAL:
                registrar_slot(i->res);
                break;

            case IR_ASN:
            case IR_ADD:
            case IR_SUB:
            case IR_MUL:
            case IR_DIV:
            case IR_MOD:
            case IR_NEG:
            case IR_NOT:
            case IR_LT:
            case IR_GT:
            case IR_EQ:
            case IR_CALL:
                /* Si el destino es un temporal (t0, t1, ...), reservarle slot */
                if (i->res != NULL && i->res[0] == 't' && isdigit((unsigned char)i->res[1]))
                    registrar_slot(i->res);
                break;

            default:
                break;
        }
    }

    int bytes_brutos = cant_slots * 8;
    /* Redondear a multiplo de 16 bytes (exigencia de alineacion de System V ABI) */
    int bytes_alineados = ((bytes_brutos + 15) / 16) * 16;
    return bytes_alineados;
}

/* ---------------------------------------------------------------------
 * Clasificacion de operandos y carga/almacenamiento en registros
 * --------------------------------------------------------------------- */

static int es_literal_entero(const char *s) {
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

/* Carga cualquier operando (literal entero/booleano, variable local/temporal o global) en "reg" */
static void cargar_operando(const char *op, const char *reg, FILE *out) {
    if (op == NULL)
        return;

    if (strcmp(op, "true") == 0) {
        fprintf(out, "    movq $1, %s\n", reg);
        return;
    }
    if (strcmp(op, "false") == 0) {
        fprintf(out, "    movq $0, %s\n", reg);
        return;
    }
    if (es_literal_entero(op)) {
        fprintf(out, "    movq $%s, %s\n", op, reg);
        return;
    }

    SlotLocal *slot = buscar_slot(op);
    if (slot != NULL) {
        fprintf(out, "    movq -%d(%%rbp), %s\n", slot->offset, reg);
    } else {
        fprintf(out, "    movq %s(%%rip), %s\n", op, reg);
    }
}

/* Guarda el contenido de "reg" en la variable local/temporal o global "dest" */
static void guardar_operando(const char *reg, const char *dest, FILE *out) {
    if (dest == NULL)
        return;

    SlotLocal *slot = buscar_slot(dest);
    if (slot != NULL) {
        fprintf(out, "    movq %s, -%d(%%rbp)\n", reg, slot->offset);
    } else {
        fprintf(out, "    movq %s, %s(%%rip)\n", reg, dest);
    }
}

/* ---------------------------------------------------------------------
 * Generador principal de Assembly x86-64 (Enteros y Booleanos)
 * --------------------------------------------------------------------- */

void generar_asm(InstruccionIR *lista, FILE *out) {
    if (out == NULL)
        out = stdout;

    fprintf(out, "    /* Codigo Assembly x86-64 generado por C-TDS */\n");

    /* 1. Seccion de datos para variables globales (.bss) */
    int hay_globales = 0;
    for (InstruccionIR *i = lista; i != NULL; i = i->siguiente) {
        if (i->op == IR_GLOBAL) {
            if (!hay_globales) {
                fprintf(out, "    .bss\n");
                hay_globales = 1;
            }
            fprintf(out, "    .align 8\n");
            fprintf(out, "%s:\n", i->res);
            fprintf(out, "    .zero 8\n");
        }
    }

    /* 2. Seccion de codigo (.text) */
    fprintf(out, "\n    .text\n");

    const char *funcion_actual = NULL;
    int idx_param_formal = 0;
    int idx_arg_actual   = 0;

    for (InstruccionIR *i = lista; i != NULL; i = i->siguiente) {
        switch (i->op) {
            case IR_GLOBAL:
                /* Ya emitidas arriba en la seccion .bss */
                break;

            case IR_FUNC_BEGIN: {
                funcion_actual   = i->res;
                idx_param_formal = 0;
                int tam_frame    = construir_frame_funcion(i);

                fprintf(out, "\n    .globl %s\n", i->res);
                fprintf(out, "    .type %s, @function\n", i->res);
                fprintf(out, "%s:\n", i->res);
                fprintf(out, "    pushq %%rbp\n");
                fprintf(out, "    movq %%rsp, %%rbp\n");
                if (tam_frame > 0)
                    fprintf(out, "    subq $%d, %%rsp\n", tam_frame);
                break;
            }

            case IR_PARAM_DECL: {
                /* Copiar cada parametro formal desde su registro ABI (o stack)
                 * hacia su slot local en el frame de la funcion. */
                SlotLocal *slot = buscar_slot(i->res);
                if (slot != NULL) {
                    if (idx_param_formal < 6) {
                        fprintf(out, "    movq %s, -%d(%%rbp)\n",
                                REGS_ARGS[idx_param_formal], slot->offset);
                    } else {
                        int offset_caller = 16 + (total_params_actual - 1 - idx_param_formal) * 8;
                        fprintf(out, "    movq %d(%%rbp), %%rax\n", offset_caller);
                        fprintf(out, "    movq %%rax, -%d(%%rbp)\n", slot->offset);
                    }
                }
                idx_param_formal++;
                break;
            }

            case IR_LOCAL:
                /* El espacio ya fue reservado en el subq del prologo */
                break;

            case IR_FUNC_END: {
                /* Epilogo de resguardo por si un metodo void termina sin return explicito */
                if (funcion_actual != NULL && strcmp(funcion_actual, "main") == 0)
                    fprintf(out, "    movq $0, %%rax\n");
                fprintf(out, "    leave\n");
                fprintf(out, "    ret\n");
                limpiar_frame();
                funcion_actual = NULL;
                break;
            }

            case IR_ASN:
                cargar_operando(i->arg1, "%rax", out);
                guardar_operando("%rax", i->res, out);
                break;

            case IR_ADD:
                cargar_operando(i->arg1, "%rax", out);
                cargar_operando(i->arg2, "%rcx", out);
                fprintf(out, "    addq %%rcx, %%rax\n");
                guardar_operando("%rax", i->res, out);
                break;

            case IR_SUB:
                cargar_operando(i->arg1, "%rax", out);
                cargar_operando(i->arg2, "%rcx", out);
                fprintf(out, "    subq %%rcx, %%rax\n");
                guardar_operando("%rax", i->res, out);
                break;

            case IR_MUL:
                cargar_operando(i->arg1, "%rax", out);
                cargar_operando(i->arg2, "%rcx", out);
                fprintf(out, "    imulq %%rcx, %%rax\n");
                guardar_operando("%rax", i->res, out);
                break;

            case IR_DIV:
                cargar_operando(i->arg1, "%rax", out);
                cargar_operando(i->arg2, "%rcx", out);
                fprintf(out, "    cqto\n");
                fprintf(out, "    idivq %%rcx\n");
                guardar_operando("%rax", i->res, out);
                break;

            case IR_MOD:
                cargar_operando(i->arg1, "%rax", out);
                cargar_operando(i->arg2, "%rcx", out);
                fprintf(out, "    cqto\n");
                fprintf(out, "    idivq %%rcx\n");
                fprintf(out, "    movq %%rdx, %%rax\n"); /* El resto queda en %rdx */
                guardar_operando("%rax", i->res, out);
                break;

            case IR_NEG:
                cargar_operando(i->arg1, "%rax", out);
                fprintf(out, "    negq %%rax\n");
                guardar_operando("%rax", i->res, out);
                break;

            case IR_NOT:
                cargar_operando(i->arg1, "%rax", out);
                fprintf(out, "    cmpq $0, %%rax\n");
                fprintf(out, "    movq $0, %%rax\n");
                fprintf(out, "    sete %%al\n");
                guardar_operando("%rax", i->res, out);
                break;

            case IR_LT:
            case IR_GT:
            case IR_EQ:
                cargar_operando(i->arg1, "%rax", out);
                cargar_operando(i->arg2, "%rcx", out);
                fprintf(out, "    cmpq %%rcx, %%rax\n");
                fprintf(out, "    movq $0, %%rax\n");
                if      (i->op == IR_LT) fprintf(out, "    setl %%al\n");
                else if (i->op == IR_GT) fprintf(out, "    setg %%al\n");
                else if (i->op == IR_EQ) fprintf(out, "    sete %%al\n");
                guardar_operando("%rax", i->res, out);
                break;

            case IR_LABEL:
                fprintf(out, ".%s:\n", i->res);
                break;

            case IR_JMP:
                fprintf(out, "    jmp .%s\n", i->res);
                break;

            case IR_JMP_FALSE:
                cargar_operando(i->arg1, "%rax", out);
                fprintf(out, "    cmpq $0, %%rax\n");
                fprintf(out, "    je .%s\n", i->res);
                break;

            case IR_JMP_TRUE:
                cargar_operando(i->arg1, "%rax", out);
                fprintf(out, "    cmpq $0, %%rax\n");
                fprintf(out, "    jne .%s\n", i->res);
                break;

            case IR_ARG:
                if (idx_arg_actual < 6) {
                    cargar_operando(i->arg1, REGS_ARGS[idx_arg_actual], out);
                } else {
                    cargar_operando(i->arg1, "%rax", out);
                    fprintf(out, "    pushq %%rax\n");
                }
                idx_arg_actual++;
                break;

            case IR_CALL: {
                int cant_args = (i->arg2 != NULL) ? atoi(i->arg2) : 0;
                fprintf(out, "    call %s\n", i->arg1);
                if (cant_args > 6) {
                    fprintf(out, "    addq $%d, %%rsp\n", (cant_args - 6) * 8);
                }
                if (i->res != NULL) {
                    guardar_operando("%rax", i->res, out);
                }
                idx_arg_actual = 0;
                break;
            }

            case IR_RET:
                if (i->arg1 != NULL) {
                    cargar_operando(i->arg1, "%rax", out);
                } else if (funcion_actual != NULL && strcmp(funcion_actual, "main") == 0) {
                    fprintf(out, "    movq $0, %%rax\n");
                }
                fprintf(out, "    leave\n");
                fprintf(out, "    ret\n");
                break;
        }
    }
}
