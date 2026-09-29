%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
%}

%code requires {
    #include "../../include/ast.h"
}

%code {
    int yylex(void);
    extern int yylineno;
    extern char *yytext;
    extern FILE *yyin;
    void yyerror(const char *s);

    int errores_lexicos = 0;
    int errores_sintacticos = 0;
}

%locations

%union {
    int    ival;
    double fval;
    char  *sval;
    ASTNode *nodo;
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

/* ---------- Tipos de los no terminales ---------- */
%type <sval> Tipo
%type <nodo> Programa ListaDeclaraciones Declaracion
%type <nodo> VarDecl ListaId MethodDecl Parametros ListaParametros
%type <nodo> Bloque ListaVarDeclLocal ListaSentencia Sentencia
%type <nodo> LlamadaMetodo ListaArgs ListaExpr Expr Literal

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
        {
            raiz_ast = crear_nodo(NODE_PROGRAMA, @1.first_line, NULL, NULL, NULL, $1, NULL, NULL);
            $$ = raiz_ast;
        }
    ;

/* ---------- Declaraciones globales (variables y metodos) ---------- */

ListaDeclaraciones
    : ListaDeclaraciones Declaracion   { $$ = agregar_a_lista($1, $2); }
    | /* lambda */                     { $$ = NULL; }
    ;

Declaracion
    : VarDecl       { $$ = $1; }
    | MethodDecl    { $$ = $1; }
    ;

/* ---------- Declaraciones de variables ---------- */

/* "int a, b;" arma una lista de nodos NODE_DECLARACION (uno por
 * variable) sin tipo todavia, y recien al cerrar la sentencia (cuando
 * se conoce $1) se lo asigna a toda la lista de una vez. */
VarDecl
    : Tipo ListaId PUNTO_Y_COMA
        {
            asignar_tipo_lista($2, $1);   /* copia $1 a cada nodo y lo libera */
            $$ = $2;
        }
    ;

ListaId
    : ID
        { $$ = crear_nodo(NODE_DECLARACION, @1.first_line, NULL, $1, NULL, NULL, NULL, NULL); }
    | ListaId COMA ID
        { $$ = agregar_a_lista($1, crear_nodo(NODE_DECLARACION, @3.first_line, NULL, $3, NULL, NULL, NULL, NULL)); }
    ;

Tipo
    : TIPO_INT      { $$ = strdup("int"); }
    | TIPO_BOOLEAN  { $$ = strdup("boolean"); }
    | TIPO_FLOAT    { $$ = strdup("float"); }
    ;

/* ---------- Declaraciones de metodos ---------- */

MethodDecl
    : Tipo ID PAR_IZQ Parametros PAR_DER Bloque
        { $$ = crear_nodo(NODE_METODO, @2.first_line, $1, $2, NULL, $4, $6, NULL); }
    | TIPO_VOID ID PAR_IZQ Parametros PAR_DER Bloque
        { $$ = crear_nodo(NODE_METODO, @2.first_line, strdup("void"), $2, NULL, $4, $6, NULL); }
    ;

Parametros
    : /* lambda */        { $$ = NULL; }
    | ListaParametros     { $$ = $1; }
    ;

/* A diferencia de ListaId, cada parametro repite su propio Tipo en el
 * texto fuente ("int x, int y"), asi que cada uno ya tiene su tipo
 * conocido en el momento de crear el nodo: no hace falta asignar_tipo_lista. */
ListaParametros
    : Tipo ID
        { $$ = crear_nodo(NODE_DECLARACION, @2.first_line, $1, $2, NULL, NULL, NULL, NULL); }
    | ListaParametros COMA Tipo ID
        { $$ = agregar_a_lista($1, crear_nodo(NODE_DECLARACION, @4.first_line, $3, $4, NULL, NULL, NULL, NULL)); }
    ;

/* ---------- Bloques ---------- */

Bloque
    : LLAVE_IZQ ListaVarDeclLocal ListaSentencia LLAVE_DER
        { $$ = crear_nodo(NODE_BLOQUE, @1.first_line, NULL, NULL, NULL, $2, $3, NULL); }
    ;

ListaVarDeclLocal
    : ListaVarDeclLocal VarDecl   { $$ = agregar_a_lista($1, $2); }
    | /* lambda */                { $$ = NULL; }
    ;

ListaSentencia
    : ListaSentencia Sentencia    { $$ = agregar_a_lista($1, $2); }
    | /* lambda */                { $$ = NULL; }
    ;

/* ---------- Sentencias ---------- */

Sentencia
    : ID ASIGNACION Expr PUNTO_Y_COMA
        { $$ = crear_nodo(NODE_ASIGNACION, @1.first_line, NULL, $1, NULL, $3, NULL, NULL); }
    | LlamadaMetodo PUNTO_Y_COMA
        { $$ = $1; }
    | IF PAR_IZQ Expr PAR_DER Bloque
        { $$ = crear_nodo(NODE_IF, @1.first_line, NULL, NULL, NULL, $3, $5, NULL); }
    | IF PAR_IZQ Expr PAR_DER Bloque ELSE Bloque
        { $$ = crear_nodo(NODE_IF, @1.first_line, NULL, NULL, NULL, $3, $5, $7); }
    | WHILE Expr Bloque
        { $$ = crear_nodo(NODE_WHILE, @1.first_line, NULL, NULL, NULL, $2, $3, NULL); }
    | RETURN Expr PUNTO_Y_COMA
        { $$ = crear_nodo(NODE_RETORNO, @1.first_line, NULL, NULL, NULL, $2, NULL, NULL); }
    | RETURN PUNTO_Y_COMA
        { $$ = crear_nodo(NODE_RETORNO, @1.first_line, NULL, NULL, NULL, NULL, NULL, NULL); }
    | PUNTO_Y_COMA
        { $$ = NULL; }
    | Bloque
        { $$ = $1; }
    | error PUNTO_Y_COMA
        {
            fprintf(stderr, "Se descarta la sentencia con error, se continua luego de la linea %d\n", yylineno);
            yyerrok;
            $$ = NULL;
        }
    ;

LlamadaMetodo
    : ID PAR_IZQ ListaArgs PAR_DER
        { $$ = crear_nodo(NODE_LLAMADA, @1.first_line, NULL, $1, NULL, $3, NULL, NULL); }
    ;

ListaArgs
    : /* lambda */    { $$ = NULL; }
    | ListaExpr       { $$ = $1; }
    ;

ListaExpr
    : Expr                    { $$ = $1; }
    | ListaExpr COMA Expr     { $$ = agregar_a_lista($1, $3); }
    ;

/* ---------- Expresiones ---------- */

Expr
    : ID                      { $$ = crear_nodo(NODE_ID, @1.first_line, NULL, $1, NULL, NULL, NULL, NULL); }
    | LlamadaMetodo           { $$ = $1; }
    | Literal                 { $$ = $1; }
    | Expr OP_SUMA Expr       { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("+"),  $1, $3, NULL); }
    | Expr OP_RESTA Expr      { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("-"),  $1, $3, NULL); }
    | Expr OP_MULT Expr       { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("*"),  $1, $3, NULL); }
    | Expr OP_DIV Expr        { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("/"),  $1, $3, NULL); }
    | Expr OP_MOD Expr        { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("%"),  $1, $3, NULL); }
    | Expr OP_MENOR Expr      { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("<"),  $1, $3, NULL); }
    | Expr OP_MAYOR Expr      { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup(">"),  $1, $3, NULL); }
    | Expr OP_IGUAL Expr      { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("=="), $1, $3, NULL); }
    | Expr OP_AND Expr        { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("&&"), $1, $3, NULL); }
    | Expr OP_OR Expr         { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("||"), $1, $3, NULL); }
    | OP_RESTA Expr %prec UMENOS   { $$ = crear_nodo(NODE_OP_UNARIA, @1.first_line, NULL, NULL, strdup("-"), $2, NULL, NULL); }
    | OP_NOT Expr                  { $$ = crear_nodo(NODE_OP_UNARIA, @1.first_line, NULL, NULL, strdup("!"), $2, NULL, NULL); }
    | PAR_IZQ Expr PAR_DER         { $$ = $2; }
    ;

Literal
    : NRO
        {
            char buf[32];
            snprintf(buf, sizeof buf, "%d", $1);
            $$ = crear_nodo(NODE_NRO, @1.first_line, strdup("int"), NULL, strdup(buf), NULL, NULL, NULL);
        }
    | FLOTANTE
        {
            char buf[64];
            snprintf(buf, sizeof buf, "%g", $1);
            $$ = crear_nodo(NODE_NRO, @1.first_line, strdup("float"), NULL, strdup(buf), NULL, NULL, NULL);
        }
    | CONST_TRUE
        { $$ = crear_nodo(NODE_BOOL, @1.first_line, strdup("boolean"), NULL, strdup("true"), NULL, NULL, NULL); }
    | CONST_FALSE
        { $$ = crear_nodo(NODE_BOOL, @1.first_line, strdup("boolean"), NULL, strdup("false"), NULL, NULL, NULL); }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error sintactico en linea %d: %s (cerca de '%s')\n", yylineno, s, yytext);
    errores_sintacticos++;
}

int main(int argc, char **argv) {
    const char *archivo = NULL;
    int debug = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-debug") == 0)
            debug = 1;
        else
            archivo = argv[i];
    }

    if (archivo != NULL) {
        FILE *file = fopen(archivo, "r");
        if (!file) {
            fprintf(stderr, "No se pudo abrir el archivo '%s'.\n", archivo);
            return 1;
        }
        yyin = file;
    }

    yyparse();

    if (errores_lexicos > 0 || errores_sintacticos > 0) {
        fprintf(stderr, "\nSe encontraron %d error/es lexico/s y %d error/es sintactico/s.\n",
                errores_lexicos, errores_sintacticos);
        return 1;
    }

    /* Sin errores lexicos ni sintacticos, el AST esta completo.
     * Aca se enganchara analizar_semantica(raiz_ast) en la proxima tanda. */
    if (debug) {
        printf("Analisis lexico y sintactico exitoso. AST:\n\n");
        imprimir_ast(raiz_ast, 0);
    }

    liberar_ast(raiz_ast);
    return 0;
}