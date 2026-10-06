%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
%}

%code requires {
    #include "ast.h"
}

%code {
    int yylex(void);
    extern int yylineno;
    extern char *yytext;
    extern FILE *yyin;
    void yyerror(const char *s);

    #include "semantica.h"

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

/* ---------------------------------------------------------------------
 * Linea de comandos (Docs/00-TDS-proyecto.pdf, Table 1)
 *
 *   c-tds [opcion] nombreArchivo.ctds
 *
 * -o <salida>     renombra el archivo de salida
 * -target <etapa> compila hasta la etapa indicada
 * -debug          imprime informacion de debugging (la traza de la TS
 *                 y el volcado del resultado); sin esta opcion, una
 *                 compilacion exitosa no imprime nada por consola
 *
 * Etapas todavia no implementadas (codinter, assembly) se rechazan con
 * un mensaje claro en lugar de fallar en silencio.
 * --------------------------------------------------------------------- */

typedef enum {
    ETAPA_SCAN,
    ETAPA_PARSE,
    ETAPA_SEMANTICA
} EtapaCompilacion;

static void imprimir_uso(const char *programa) {
    fprintf(stderr, "Uso: %s [opcion] nombreArchivo.ctds\n", programa);
    fprintf(stderr, "  -o <salida>      Renombra el archivo de salida\n");
    fprintf(stderr, "  -target <etapa>  scan | parse | codinter | assembly\n");
    fprintf(stderr, "  -debug           Imprime informacion de debugging\n");
}

/* Ruta del archivo de salida: si se paso -o se usa ese nombre tal cual,
 * si no se reemplaza la extension del fuente por "ext" (.lex, .sint,
 * .sem). El puntero devuelto hay que liberarlo con free(). */
static char *ruta_salida(const char *archivo, const char *salida, const char *ext) {
    if (salida != NULL) {
        char *ruta = (char *)malloc(strlen(salida) + 1);
        if (ruta == NULL) {
            fprintf(stderr, "Error interno: sin memoria para la ruta de salida\n");
            exit(1);
        }
        strcpy(ruta, salida);
        return ruta;
    }

    char *ruta = (char *)malloc(strlen(archivo) + strlen(ext) + 4);
    if (ruta == NULL) {
        fprintf(stderr, "Error interno: sin memoria para la ruta de salida\n");
        exit(1);
    }
    strcpy(ruta, archivo);

    char *punto = strrchr(ruta, '.');
    char *slash = strrchr(ruta, '/');
    if (punto != NULL && (slash == NULL || punto > slash))
        *punto = '\0';          /* corta la extension vieja */
    strcat(ruta, ext);

    return ruta;
}

/* Nombre legible de cada token, para el volcado de -target scan (.lex). */
static const char *nombre_token(int token) {
    switch (token) {
        case TIPO_INT:      return "TIPO_INT";
        case TIPO_BOOLEAN:  return "TIPO_BOOLEAN";
        case TIPO_FLOAT:    return "TIPO_FLOAT";
        case TIPO_VOID:     return "TIPO_VOID";
        case IF:            return "IF";
        case ELSE:          return "ELSE";
        case WHILE:         return "WHILE";
        case RETURN:        return "RETURN";
        case CONST_TRUE:    return "CONST_TRUE";
        case CONST_FALSE:   return "CONST_FALSE";
        case ID:            return "ID";
        case NRO:           return "NRO";
        case FLOTANTE:      return "FLOTANTE";
        case OP_SUMA:       return "OP_SUMA";
        case OP_RESTA:      return "OP_RESTA";
        case OP_MULT:       return "OP_MULT";
        case OP_DIV:        return "OP_DIV";
        case OP_MOD:        return "OP_MOD";
        case OP_MENOR:      return "OP_MENOR";
        case OP_MAYOR:      return "OP_MAYOR";
        case OP_IGUAL:      return "OP_IGUAL";
        case OP_AND:        return "OP_AND";
        case OP_OR:         return "OP_OR";
        case OP_NOT:        return "OP_NOT";
        case ASIGNACION:    return "ASIGNACION";
        case COMA:          return "COMA";
        case PUNTO_Y_COMA:  return "PUNTO_Y_COMA";
        case PAR_IZQ:       return "PAR_IZQ";
        case PAR_DER:       return "PAR_DER";
        case LLAVE_IZQ:     return "LLAVE_IZQ";
        case LLAVE_DER:     return "LLAVE_DER";
        default:            return "??";
    }
}

int main(int argc, char **argv) {
    const char *archivo = NULL;
    const char *salida = NULL;
    int debug = 0;
    EtapaCompilacion etapa = ETAPA_SEMANTICA;   /* etapa corriente */

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-debug") == 0) {
            debug = 1;
        } else if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "La opcion -o necesita un argumento.\n");
                imprimir_uso(argv[0]);
                return 1;
            }
            salida = argv[++i];
        } else if (strcmp(arg, "-target") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "La opcion -target necesita un argumento.\n");
                imprimir_uso(argv[0]);
                return 1;
            }
            const char *objetivo = argv[++i];

            if (strcmp(objetivo, "scan") == 0)
                etapa = ETAPA_SCAN;
            else if (strcmp(objetivo, "parse") == 0)
                etapa = ETAPA_PARSE;
            else if (strcmp(objetivo, "semantic") == 0)
                etapa = ETAPA_SEMANTICA;   /* extension de la tabla de la spec */
            else if (strcmp(objetivo, "codinter") == 0 ||
                     strcmp(objetivo, "assembly") == 0) {
                fprintf(stderr, "La etapa '%s' todavia no esta implementada.\n", objetivo);
                return 1;
            } else {
                fprintf(stderr, "Etapa desconocida '%s'. Etapas validas: "
                                "scan, parse, codinter, assembly.\n", objetivo);
                return 1;
            }
        } else if (strncmp(arg, "-", 1) == 0) {
            /* el nombre del archivo no puede empezar con '-' */
            fprintf(stderr, "Opcion desconocida '%s'.\n", arg);
            imprimir_uso(argv[0]);
            return 1;
        } else if (archivo != NULL) {
            fprintf(stderr, "Solo se admite un archivo fuente.\n");
            return 1;
        } else {
            archivo = arg;
        }
    }

    if (archivo == NULL) {
        fprintf(stderr, "Falta el archivo fuente (.ctds).\n");
        imprimir_uso(argv[0]);
        return 1;
    }

    FILE *file = fopen(archivo, "r");
    if (!file) {
        fprintf(stderr, "No se pudo abrir el archivo '%s'.\n", archivo);
        return 1;
    }
    yyin = file;

    /* ---------- Hasta la etapa scan ---------- */
    if (etapa == ETAPA_SCAN) {
        char *ruta = ruta_salida(archivo, salida, ".lex");
        FILE *out = fopen(ruta, "w");
        if (out == NULL) {
            fprintf(stderr, "No se pudo escribir el archivo '%s'.\n", ruta);
            free(ruta);
            fclose(file);
            return 1;
        }

        int token;
        int cantidad = 0;
        while ((token = yylex()) != 0) {
            fprintf(out, "%d\t%s\t%s\n", yylineno, nombre_token(token), yytext);
            cantidad++;
        }
        fclose(out);
        fclose(file);

        if (errores_lexicos > 0) {
            remove(ruta);           /* la etapa no termino bien: sin salida */
            fprintf(stderr, "\nSe encontraron %d error/es lexico/s.\n", errores_lexicos);
            free(ruta);
            return 1;
        }
        if (debug)
            printf("Analisis lexico exitoso: %d token/s.\n", cantidad);
        free(ruta);
        return 0;
    }

    /* ---------- Hasta la etapa parse (o mas alla) ---------- */
    yyparse();
    fclose(file);

    if (errores_lexicos > 0 || errores_sintacticos > 0) {
        fprintf(stderr, "\nSe encontraron %d error/es lexico/s y %d error/es sintactico/s.\n",
                errores_lexicos, errores_sintacticos);
        return 1;
    }

    if (etapa == ETAPA_PARSE) {
        if (debug) {
            printf("Analisis lexico y sintactico exitoso. AST:\n\n");
            imprimir_ast(raiz_ast, 0);
        }

        char *ruta = ruta_salida(archivo, salida, ".sint");
        FILE *out = fopen(ruta, "w");
        if (out == NULL) {
            fprintf(stderr, "No se pudo escribir el archivo '%s'.\n", ruta);
            free(ruta);
            liberar_ast(raiz_ast);
            return 1;
        }
        fprintf(out, "=== Arbol de Sintaxis Abstracta ===\n");
        imprimir_ast_en(out, raiz_ast, 0);
        fclose(out);

        free(ruta);
        liberar_ast(raiz_ast);
        return 0;
    }

    /* ---------- Etapa semantica (la corriente) ---------- */
    sem_set_debug(debug);
    int errores = analizar_semantica(raiz_ast);

    if (debug)
        volcar_resultado_semantico(stdout, raiz_ast);

    if (errores > 0) {
        fprintf(stderr, "\nSe encontraron %d error/es semanticos.\n", errores);
        liberar_ast(raiz_ast);
        return 1;
    }

    char *ruta = ruta_salida(archivo, salida, ".sem");
    FILE *out = fopen(ruta, "w");
    if (out == NULL) {
        fprintf(stderr, "No se pudo escribir el archivo '%s'.\n", ruta);
        free(ruta);
        liberar_ast(raiz_ast);
        return 1;
    }
    volcar_resultado_semantico(out, raiz_ast);
    fclose(out);

    free(ruta);
    liberar_ast(raiz_ast);
    return 0;
}
