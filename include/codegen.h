#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "ir.h"

/* Traduce la secuencia lineal de instrucciones de Tres Direcciones (lista)
 * a codigo Assembly x86-64 (sintaxis AT&T para Linux / GCC) y lo escribe
 * sobre el flujo "out" (por ejemplo, stdout o un archivo .s). */
void generar_asm(InstruccionIR *lista, FILE *out);

#endif
