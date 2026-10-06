#include "../../include/ast.h"

ASTNode *raiz_ast = NULL;


ASTNode *crear_nodo( TipoNodo tipoNodo, int linea, char *tipo_dato, char *nombre_id, char *valor, ASTNode *hijo1, ASTNode *hijo2, ASTNode *hijo3) {

    ASTNode *nodo = (ASTNode *)malloc(sizeof(ASTNode));

    if (nodo == NULL) {
        fprintf(stderr, "Error interno: sin memoria para crear un nodo del AST\n");
        exit(1);
    }

    Simbolo *simbolo = (Simbolo *)malloc(sizeof(Simbolo));

    if (simbolo == NULL) {
        fprintf(stderr, "Error interno: sin memoria para crear un simbolo\n");
        exit(1);
    }

    simbolo->tipo_dato = tipo_dato;
    simbolo->nombre_id = nombre_id;
    simbolo->valor     = valor;

    nodo->tipo      = tipoNodo;
    nodo->linea     = linea;
    nodo->simbolo   = simbolo;
    nodo->hijo1     = hijo1;
    nodo->hijo2     = hijo2;
    nodo->hijo3     = hijo3;
    nodo->siguiente = NULL;

    return nodo;
}

/* ---------------------------------------------------------------------
 * Listas (first-child / next-sibling)
 * --------------------------------------------------------------------- */

ASTNode *agregar_a_lista(ASTNode *lista, ASTNode *nuevo) {

    if (lista == NULL) return nuevo;
    if (nuevo == NULL) return lista;

    ASTNode *ultimo = lista;

    while (ultimo->siguiente != NULL)
        ultimo = ultimo->siguiente;
    ultimo->siguiente = nuevo;

    return lista;
}

void asignar_tipo_lista(ASTNode *lista, char *tipo_dato) {

    for (ASTNode *n = lista; n != NULL; n = n->siguiente) {

        if (n->simbolo != NULL) {
            free(n->simbolo->tipo_dato);   /* deberia ser NULL, por las dudas */
            n->simbolo->tipo_dato = strdup(tipo_dato);
        }
    }
    
    free(tipo_dato);   /* ya se copio a cada nodo, no hace falta conservarlo */
}

/* ---------------------------------------------------------------------
 * Impresion del arbol (util para verificar el AST con -debug)
 * --------------------------------------------------------------------- */

static const char *nombre_tipo_nodo(TipoNodo tipo) {
    switch (tipo) {
        case NODE_PROGRAMA:    return "PROGRAMA";
        case NODE_DECLARACION: return "DECLARACION";
        case NODE_METODO:      return "METODO";
        case NODE_BLOQUE:      return "BLOQUE";
        case NODE_ASIGNACION:  return "ASIGNACION";
        case NODE_IF:          return "IF";
        case NODE_WHILE:       return "WHILE";
        case NODE_RETORNO:     return "RETORNO";
        case NODE_LLAMADA:     return "LLAMADA";
        case NODE_OP_BINARIA:  return "OP_BINARIA";
        case NODE_OP_UNARIA:   return "OP_UNARIA";
        case NODE_NRO:         return "NRO";
        case NODE_BOOL:        return "BOOL";
        case NODE_ID:          return "ID";
        default:                return "?";
    }
}

static void indentar(FILE *archivo, int nivel) {
    for (int i = 0; i < nivel * 2; i++)
        fputc(' ', archivo);
}

void imprimir_ast_en(FILE *archivo, ASTNode *nodo, int nivel) {
    while (nodo != NULL) {
        indentar(archivo, nivel);
        fprintf(archivo, "%s", nombre_tipo_nodo(nodo->tipo));

        if (nodo->simbolo != NULL) {
            if (nodo->simbolo->tipo_dato != NULL)
                fprintf(archivo, " tipo=%s", nodo->simbolo->tipo_dato);
            if (nodo->simbolo->nombre_id != NULL)
                fprintf(archivo, " nombre=%s", nodo->simbolo->nombre_id);
            if (nodo->simbolo->valor != NULL)
                fprintf(archivo, " valor=\"%s\"", nodo->simbolo->valor);
        }
        fprintf(archivo, "  [linea %d]\n", nodo->linea);

        imprimir_ast_en(archivo, nodo->hijo1, nivel + 1);
        imprimir_ast_en(archivo, nodo->hijo2, nivel + 1);
        imprimir_ast_en(archivo, nodo->hijo3, nivel + 1);

        nodo = nodo->siguiente;   /* sigue al hermano, mismo nivel */
    }
}

void imprimir_ast(ASTNode *nodo, int nivel) {
    imprimir_ast_en(stdout, nodo, nivel);
}

/* ---------------------------------------------------------------------
 * Liberacion de memoria
 * --------------------------------------------------------------------- */

void liberar_ast(ASTNode *nodo) {
    while (nodo != NULL) {
        ASTNode *siguiente = nodo->siguiente;   /* guardar antes de liberar */

        liberar_ast(nodo->hijo1);
        liberar_ast(nodo->hijo2);
        liberar_ast(nodo->hijo3);

        if (nodo->simbolo != NULL) {
            free(nodo->simbolo->tipo_dato);
            free(nodo->simbolo->nombre_id);
            free(nodo->simbolo->valor);
            free(nodo->simbolo);
        }
        free(nodo);

        nodo = siguiente;
    }
}
