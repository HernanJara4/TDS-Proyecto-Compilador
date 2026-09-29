%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Gramatica.tab.h"

extern int errores_lexicos; /* Contador global de errores lexicos, definido en Gramatica.y */

/* Con %locations en Gramatica.y, bison expone yylloc y el tipo YYLTYPE
 * (ambos declarados en Gramatica.tab.h). YY_USER_ACTION se ejecuta antes
 * de la accion de CADA regla reconocida, asi que con esto cada token
 * queda con su propia linea en yylloc, y las acciones del parser pueden
 * usar @1, @2, etc. para saber en que linea del fuente ocurrio cada
 * construccion (se usa para que los nodos del AST guarden su "linea"). */
#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno;

%}

%option noyywrap
%option yylineno

/* ---------- Definiciones Regulares --------- */
LETRA        [a-zA-Z]
DIGITO       [0-9]
ID           {LETRA}({LETRA}|{DIGITO})*
NRO          {DIGITO}+
FLOTANTE     {DIGITO}+"."{DIGITO}+

%%

"//".*                                  { /* Ignorar comentarios de una linea */ }
"/*"([^*]|(\*+[^*/]))*\*+"/"            { /* Ignorar comentarios multilinea */ }
[ \t\n\r\v\f]+                          { /* Ignorar espacios, tabulaciones y saltos de linea */ }

    /* ---------- Palabras reservadas ---------- */
"boolean"   { return TIPO_BOOLEAN; }
"else"      { return ELSE; }
"false"     { return CONST_FALSE; }
"if"        { return IF; }
"int"       { return TIPO_INT; }
"return"    { return RETURN; }
"true"      { return CONST_TRUE; }
"void"      { return TIPO_VOID; }
"while"     { return WHILE; }
"float"     { return TIPO_FLOAT; }

    /* ---------- Literales e identificadores ---------- */
{FLOTANTE}  { yylval.fval = atof(yytext); return FLOTANTE; }
{NRO}       { yylval.ival = atoi(yytext); return NRO; }
{ID}        { yylval.sval = strdup(yytext); return ID; }

    /* ---------- Operadores ---------- */
"&&"        { return OP_AND; }
"||"        { return OP_OR; }
"=="        { return OP_IGUAL; }
"<"         { return OP_MENOR; }
">"         { return OP_MAYOR; }
"!"         { return OP_NOT; }
"+"         { return OP_SUMA; }
"-"         { return OP_RESTA; }
"*"         { return OP_MULT; }
"/"         { return OP_DIV; }
"%"         { return OP_MOD; }
"="         { return ASIGNACION; }

    /* ---------- Delimitadores ---------- */
";"         { return PUNTO_Y_COMA; }
","         { return COMA; }
"("         { return PAR_IZQ; }
")"         { return PAR_DER; }
"{"         { return LLAVE_IZQ; }
"}"         { return LLAVE_DER; }

    /* ---------- Cualquier otro caracter es un error lexico ---------- */
.           {
                fprintf(stderr, "Error lexico en linea %d: caracter no reconocido '%s'\n", yylineno, yytext);
                errores_lexicos++;
            }

%%