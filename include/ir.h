#ifndef IR_H
#define IR_H

#include <stdio.h>
#include "ast.h"

/* Codigos de operacion del Codigo de Tres Direcciones (Cuadruplas) */
typedef enum {
    /* Declaraciones y estructura de funciones */
    IR_GLOBAL,      /* global <tipo> <res>                  */
    IR_FUNC_BEGIN,  /* func <tipo> <res>:                   */
    IR_PARAM_DECL,  /*   formal <tipo> <res>                */
    IR_LOCAL,       /*   local <tipo> <res>                 */
    IR_FUNC_END,    /* endfunc <res>                        */

    /* Transferencia / asignacion */
    IR_ASN,         /*   <res> = <arg1>                     */

    /* Operaciones aritmeticas binarias */
    IR_ADD,         /*   <res> = <arg1> + <arg2>            */
    IR_SUB,         /*   <res> = <arg1> - <arg2>            */
    IR_MUL,         /*   <res> = <arg1> * <arg2>            */
    IR_DIV,         /*   <res> = <arg1> / <arg2>            */
    IR_MOD,         /*   <res> = <arg1> % <arg2>            */

    /* Operaciones unarias */
    IR_NEG,         /*   <res> = - <arg1>                   */
    IR_NOT,         /*   <res> = ! <arg1>                   */

    /* Operaciones relacionales y de igualdad */
    IR_LT,          /*   <res> = <arg1> < <arg2>            */
    IR_GT,          /*   <res> = <arg1> > <arg2>            */
    IR_EQ,          /*   <res> = <arg1> == <arg2>           */

    /* Saltos y etiquetas (control de flujo y corto circuito) */
    IR_LABEL,       /* <res>:                               */
    IR_JMP,         /*   goto <res>                         */
    IR_JMP_FALSE,   /*   ifFalse <arg1> goto <res>          */
    IR_JMP_TRUE,    /*   ifTrue <arg1> goto <res>           */

    /* Invocacion y retorno de metodos */
    IR_ARG,         /*   param <arg1>                       */
    IR_CALL,        /*   <res> = call <arg1>, <arg2>        */
    IR_RET          /*   return [<arg1>]                    */
} OpIR;

typedef struct InstruccionIR InstruccionIR;

/* Representacion en memoria de una instruccion de Tres Direcciones (Cuadrupla) */
struct InstruccionIR {
    OpIR           op;
    char          *tipo;       /* Tipo del dato ("int", "boolean", "void") */
    char          *arg1;       /* Primer operando (variable, constante, temporal o funcion) */
    char          *arg2;       /* Segundo operando (o cantidad de argumentos en IR_CALL) */
    char          *res;        /* Destino (temporal, variable o etiqueta de salto) */
    InstruccionIR *siguiente;  /* Siguiente instruccion en la secuencia lineal */
};

/* Puntero global a la lista de instrucciones IR generadas */
extern InstruccionIR *codigo_ir;

/* Recorre el AST anotado a partir de "raiz" y construye la secuencia lineal
 * de instrucciones de Tres Direcciones. Devuelve la cabeza de la lista. */
InstruccionIR *generar_ir(ASTNode *raiz);

/* Vuelca la secuencia de instrucciones de Tres Direcciones en formato legible
 * sobre el flujo "out" (por ejemplo, stdout o un archivo .ci). */
void imprimir_ir(InstruccionIR *lista, FILE *out);

/* Libera toda la memoria asociada a la lista de instrucciones IR. */
void liberar_ir(InstruccionIR *lista);

#endif
