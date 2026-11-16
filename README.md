# Taller-Diseño-De-Software---Proyecto-Compilador
Desarrollo de un compilador incremental para el lenguaje de programación C-TDS. Proyecto académico realizado para la materia Taller de Diseño de Software de la Universidad Nacional de Río Cuarto.


### Compilar el proyecto

Con `make`:
```bash
make
```

O manualmente paso a paso:
1. Ejecutar flex
```bash
flex -o build/lex.yy.c src/scanner/Gramatica.l
```
2. Ejecutar bison
```bash
bison -d -o build/Gramatica.tab.c src/parser/Gramatica.y
```
3. Compilar con gcc
```bash
gcc -I build/ -I include/ -o build/c-tds build/Gramatica.tab.c build/lex.yy.c src/main.c src/ast/ast.c src/semantic/tabla.c src/semantic/typecheck.c src/ir/ir.c src/asm/codegen.c src/opt/optimizer.c
```
### Ejecutar tests

#### Con `Makefile` (todos los tests de una etapa):
* Ejecutar **todos los tests del Optimizador** (`tests/05_opt/public/` — muestra IR antes y después de optimizar, ensambla y ejecuta el binario):
  ```bash
  make test-opt
  ```
* Ejecutar **todos los tests de Assembly x86-64** (`tests/04_asm/public/` — compila a binario nativo, lo ejecuta y muestra el código de retorno):
  ```bash
  make test-asm
  ```
* Ejecutar **todos los tests de Código Intermedio** (`tests/03_ir/public/`):
  ```bash
  make test-ir
  ```
* Ejecutar **todos los tests del Análisis Semántico** (`tests/02_semantic/public/`):
  ```bash
  make test-semantic
  ```
* Ejecutar **todos los tests del Scanner y Parser** (`tests/01_scanner_parser/public/`):
  ```bash
  make test-parser
  ```
* Ejecutar **toda la suite completa** (Etapas 1 a 5):
  ```bash
  make test
  ```
* Limpiar los archivos generados en `build/`:
  ```bash
  make clean
  ```

#### Interfaz de Línea de Comandos (`c-tds`)
```bash
./build/c-tds [opciones] archivo.ctds
```
* `-target <etapa>`: ejecuta hasta `scan`, `parse`, `sem`, `codinter` o `assembly`.
* `-opt [all|cf|cp|dce]`: activa las optimizaciones de código intermedio.
* `-o <salida>`: define el nombre del archivo de salida (`.ci`, `.s`/`.ass` o ejecutable `.out`).
* `-debug`: imprime el AST anotado, el Código Intermedio (original y optimizado) y el Assembly x86-64.
