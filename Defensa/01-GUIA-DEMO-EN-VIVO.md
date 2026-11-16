# Guía Práctica: Cómo Mostrar el Compilador al 100% en la Defensa Oral

Esta guía usa el archivo `Defensa/demo.ctds`, que es corto para que todo entre en una sola pantalla de la terminal, pero usa todas las características del lenguaje (variable global, función con parámetro, llamada, `if-else`, expresión optimizable y `return` con valor `25`).

---

## Paso 0: Compilar el proyecto desde cero
Mostrale al profesor que compila limpio sin ningún warning:
```bash
make clean && make
```

---

## Paso 1: Mostrar hasta el Parser y el AST (`-target parse -debug`)
Ejecutá:
```bash
./build/c-tds -target parse -debug Defensa/demo.ctds
```
**Qué explicarle al profe cuando sale esto en pantalla:**
* Acá corrés solo las **Etapas 1 y 2a (Scanner + Parser + Construcción del AST)**, frenando antes del análisis semántico.
* Señalale en la salida que los nodos `DECLARACION`, `METODO`, `NRO` y `BOOL` ya traen su `tipo=` desde el parser, pero los nodos de expresiones y sentencias (`ASIGNACION`, `OP_BINARIA`, `LLAMADA`, `ID`, `RETORNO`) **todavía NO tienen `tipo=`**, porque el chequeo e inferencia de tipos lo hace recién el Analizador Semántico.

---

## Paso 2: Mostrar el Análisis Semántico y el AST Anotado (`-target sem -debug`)
Ejecutá:
```bash
./build/c-tds -target sem -debug Defensa/demo.ctds
```
**Qué explicarle al profe:**
* Ahora corrió `analizar_semantica(raiz_ast)` (en `src/semantic/typecheck.c`) usando la Tabla de Símbolos por niveles (`src/semantic/tabla.c`).
* Mostrale cómo ahora **todos los nodos del árbol quedaron anotados con su tipo** (`ASIGNACION tipo=int`, `OP_BINARIA tipo=boolean valor=">"`, `LLAMADA tipo=int nombre=duplicar`, `RETORNO tipo=int`). Ese es el **"AST Anotado"** que pide la teoría.

---

## Paso 3: Mostrar el Código Intermedio SIN optimizar (`-target codinter -debug`)
Ejecutá:
```bash
./build/c-tds -target codinter -debug Defensa/demo.ctds
```
**Qué explicarle al profe:**
* Acá `generar_ir(raiz_ast)` (`src/ir/ir.c`) aplanó el AST anotado a **Código de Tres Direcciones (Cuádruplas)**.
* Señalale cómo `base = (2 + 3) * 2;` generó temporales paso a paso:
  ```text
  t1 = 2 + 3
  t2 = t1 * 2
  base = t2
  ```
* Y cómo el `if (base > 5)` se tradujo con `t3 = base > 5`, `ifFalse t3 goto L0`, la llamada con `param base` y `t4 = call duplicar, 1`, y las etiquetas `L0:` y `L1:`.

---

## Paso 4: Mostrar el Optimizador en acción (`-target codinter -opt all -debug`)
Ejecutá:
```bash
./build/c-tds -target codinter -opt all -debug Defensa/demo.ctds
```
**Qué explicarle al profe:**
* Compará en la misma pantalla el bloque `--- Codigo Intermedio de Tres Direcciones (IR) ---` contra `--- Codigo Intermedio Optimizado (-opt all) ---`:
  * Vas a ver que `t1 = 2 + 3`, `t2 = t1 * 2`, `base = t2` se plegó y propagó en compilación quedando una sola línea: **`base = 10`**.
  * Y más abajo, como `base` no es constante pero las operaciones `t4 = call duplicar, 1`, `t5 = t4 + bonus`, `total = t5` usaban un temporal intermedio `t5`, la fusión de copias lo redujo a **`total = t4 + bonus`**.

---

## Paso 5: Compilar hasta Assembly x86-64 y Ejecutar el Binario Real
Ejecutá:
```bash
./build/c-tds -opt all -debug Defensa/demo.ctds
```
**Qué explicarle al profe:**
1. Como no le pasaste `-target` y el archivo es `.ctds`, el compilador hizo todo el recorrido hasta el final y generó automáticamente el ejecutable **`Defensa/demo.out`** (y el archivo `Defensa/demo.opt`).
2. En pantalla te muestra el Assembly x86-64 generado (`pushq %rbp`, `movq %rsp, %rbp`, `subq $32, %rsp`, el pasaje del parámetro por `%rdi`, `call duplicar`, etc.).
3. Ahora ejecutá el binario compilado y mostrá el valor de retorno (`20 + 5 = 25`):
   ```bash
   ./Defensa/demo.out
   echo $?
   ```
   Va a imprimir **`25`**.
4. Para limpiar los archivos generados por la demo:
   ```bash
   rm -f Defensa/demo.out Defensa/demo.opt
   ```
