%{
#include <stdio.h>
#include <stdlib.h>

int yylex(void);
extern int yylineno;
extern char *yytext;
extern FILE *yyin;
void yyerror(const char *s);

int errores_lexicos = 0;
int errores_sintacticos = 0;
%}

%union {
    int    ival;
    double fval;
    char  *sval;
}

/* ---------- Palabras reservadas ---------- */
%token TIPO_INT TIPO_BOOLEAN TIPO_FLOAT TIPO_VOID
%token IF ELSE WHILE RETURN
%token CONST_TRUE CONST_FALSE

/* ---------- Identificadores y literales ---------- */
%token <sval> ID
%token <ival> NRO
%token <fval> FLOTANTE

/* ---------- Operadores ---------- */
%token OP_SUMA OP_RESTA OP_MULT OP_DIV OP_MOD
%token OP_MENOR OP_MAYOR OP_IGUAL
%token OP_AND OP_OR OP_NOT
%token ASIGNACION

/* ---------- Delimitadores ---------- */
%token COMA PUNTO_Y_COMA PAR_IZQ PAR_DER LLAVE_IZQ LLAVE_DER

/* ---------- Precedencia (de menor a mayor), segun la especificacion ---------- */
%left OP_OR
%left OP_AND
%nonassoc OP_IGUAL
%nonassoc OP_MENOR OP_MAYOR
%left OP_SUMA OP_RESTA
%left OP_MULT OP_DIV OP_MOD
%right OP_NOT
%right UMENOS

%define parse.error verbose

%start Programa

%%

Programa
    : ListaDeclaraciones
    ;

/* ---------- Declaraciones de variables globales y metodos ---------- */

ListaDeclaraciones
    : ListaDeclaraciones Declaracion
    | /* lambda */
    ;

Declaracion
    : VarDecl
    | MethodDecl
    ;

VarDecl
    : Tipo ID RestoVarDecl   { free($2); }
    ;

RestoVarDecl
    : PUNTO_Y_COMA
    | COMA ListaId PUNTO_Y_COMA
    ;

ListaId
    : ID                     { free($1); }
    | ListaId COMA ID        { free($3); }
    ;

Tipo
    : TIPO_INT
    | TIPO_BOOLEAN
    | TIPO_FLOAT
    ;

MethodDecl
    : Tipo ID PAR_IZQ Parametros PAR_DER Bloque        { free($2); }
    | TIPO_VOID ID PAR_IZQ Parametros PAR_DER Bloque   { free($2); }
    ;

Parametros
    : /* lambda */
    | ListaParametros
    ;

ListaParametros
    : Tipo ID                          { free($2); }
    | ListaParametros COMA Tipo ID     { free($4); }
    ;

/* ---------- Bloques ---------- */

Bloque
    : LLAVE_IZQ ListaVarDeclLocal ListaSentencia LLAVE_DER
    ;

ListaVarDeclLocal
    : ListaVarDeclLocal VarDecl
    | /* lambda */
    ;

ListaSentencia
    : ListaSentencia Sentencia
    | /* lambda */
    ;

/* ---------- Sentencias ---------- */

Sentencia
    : ID ASIGNACION Expr PUNTO_Y_COMA          { free($1); }
    | LlamadaMetodo PUNTO_Y_COMA
    | IF PAR_IZQ Expr PAR_DER Bloque
    | IF PAR_IZQ Expr PAR_DER Bloque ELSE Bloque
    | WHILE Expr Bloque
    | RETURN Expr PUNTO_Y_COMA
    | RETURN PUNTO_Y_COMA
    | PUNTO_Y_COMA
    | Bloque
    | error PUNTO_Y_COMA
        {
            fprintf(stderr, "Se descarta la sentencia con error, se continua luego de la linea %d\n", yylineno);
            yyerrok;
        }
    ;

LlamadaMetodo
    : ID PAR_IZQ ListaArgs PAR_DER   { free($1); }
    ;

ListaArgs
    : /* lambda */
    | ListaExpr
    ;

ListaExpr
    : Expr
    | ListaExpr COMA Expr
    ;

/* ---------- Expresiones ---------- */

Expr
    : ID                            { free($1); }
    | LlamadaMetodo
    | Literal
    | Expr OP_SUMA Expr
    | Expr OP_RESTA Expr
    | Expr OP_MULT Expr
    | Expr OP_DIV Expr
    | Expr OP_MOD Expr
    | Expr OP_MENOR Expr
    | Expr OP_MAYOR Expr
    | Expr OP_IGUAL Expr
    | Expr OP_AND Expr
    | Expr OP_OR Expr
    | OP_RESTA Expr %prec UMENOS
    | OP_NOT Expr
    | PAR_IZQ Expr PAR_DER
    ;

Literal
    : NRO
    | FLOTANTE
    | CONST_TRUE
    | CONST_FALSE
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error sintactico en linea %d: %s (cerca de '%s')\n", yylineno, s, yytext);
    errores_sintacticos++;
}

int main(int argc, char **argv) {
    if (argc > 1) {
        FILE *file = fopen(argv[1], "r");
        if (!file) {
            fprintf(stderr, "No se pudo abrir el archivo '%s'.\n", argv[1]);
            return 1;
        }
        yyin = file;
    }

    yyparse();

    if (errores_lexicos == 0 && errores_sintacticos == 0) {
        printf("Compilacion exitosa: no se encontraron errores lexicos ni sintacticos.\n");
        return 0;
    } else {
        printf("\nSe encontraron %d error/es lexico/s y %d error/es sintactico/s.\n",
               errores_lexicos, errores_sintacticos);
        return 1;
    }
}