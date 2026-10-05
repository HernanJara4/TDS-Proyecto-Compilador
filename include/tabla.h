#ifndef TABLA_H
#define TABLA_H

#include "include/ast.h"

typedef struct EntradaTS {

  Simbolo *simbolo;
  int inicializada;
  struct EbtradaTS *siguiente;
} EbtradaTS;

/* Manejo de la pila de niveles */
void InicializarTS(void);
void AbrirNivel(void);
void CerrarNivel(void);

/* Interaccion con el parser: Devuelven el puntero al Simbolo para enlanzar al AST */
Simbolo *InsertarSimbolo(char *nombre, char *tipo);
Simbolo *BuscarSimbolo(char *nombre);

/*Funcion Auxiliiar */
void ActualizarValor(Simbolo *simbolo, char *nuevo_valor);

#endif
