#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tipo de NODOS en el AST */
typedef enum {
  NODE_PROGRAMA,
  NODE_DECLARACION,
  NODE_METODO,
  NODE_BLOQUE,
  NODE_ASIGNACION,
  NODE_IF,
  NODE_WHILE,
  NODE_RETORNO,
  NODE_LLAMADA,
  NODE_OP_BINARIA,
  NODE_OP_UNARIA,
  NODE_NRO,
  NODE_BOOL,
  NODE_ID
} TipoNodo;

/* Estructura para transportar datos y simbolos */
typedef struct {
  char *tipo_dato;
  char *nombre_id;
  char *valor;
} Simbolo;

typedef struct ASTNode ASTNode;

struct ASTNode {
  TipoNodo tipo;
  int linea;          /* linea del fuente, para mensajes de error */
  Simbolo *simbolo;
  ASTNode *hijo1;
  ASTNode *hijo2;
  ASTNode *hijo3;
  ASTNode *siguiente; /* proximo elemento, si este nodo es parte de una lista */
};

/* Puntero global a la raiz del AST completo */
extern ASTNode *raiz_ast;

ASTNode *crear_nodo(TipoNodo tipoNodo, int linea,
                     char *tipo_dato, char *nombre_id, char *valor,
                     ASTNode *hijo1, ASTNode *hijo2, ASTNode *hijo3);

/* Encadena "nuevo" al final de "lista" usando el puntero siguiente.
 * Acepta NULL en cualquiera de los dos lados. Devuelve la cabeza. */
ASTNode *agregar_a_lista(ASTNode *lista, ASTNode *nuevo);

/* Asigna tipo_dato a todos los nodos de la lista (para "int a, b, c;",
 * donde el tipo se conoce recien despues de parsear los identificadores). */
void asignar_tipo_lista(ASTNode *lista, char *tipo_dato);


void imprimir_ast(ASTNode *nodo, int nivel);
void liberar_ast(ASTNode *nodo);

#endif