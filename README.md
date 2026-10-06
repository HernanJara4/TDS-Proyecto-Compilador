# Taller-Diseño-De-Software---Proyecto-Compilador
Desarrollo de un compilador incremental para el lenguaje de programación C-TDS. Proyecto académico realizado para la materia Taller de Diseño de Software de la Universidad Nacional de Río Cuarto.

### Compilar
```sh
make            # genera build/c-tds (flex + bison + gcc)
make clean      # borra los archivos generados en build/
```

Compilación manual equivalente:
1. Ejecutar flex
```sh
flex -o build/lex.yy.c src/scanner/Gramatica.l
```
2. Ejecutar bison
```sh
bison -d -o build/Gramatica.tab.c src/parser/Gramatica.y
```
3. Compilar
```sh
gcc -I build/ -I include/ -o build/c-tds build/Gramatica.tab.c build/lex.yy.c src/ast/ast.c src/semantic/tabla.c src/semantic/typecheck.c
```

### Ejecutar el compilador
```sh
build/c-tds [opcion] nombreArchivo.ctds
```

| Opción | Acción |
| --- | --- |
| `-o <salida>` | Renombra el archivo de salida |
| `-target <etapa>` | `scan` (salida `.lex`), `parse` (salida `.sint`), `codinter`, `assembly` (todavía no implementadas) |
| `-debug` | Imprime la traza de la Tabla de Símbolos y el volcado del resultado |

Por defecto compila hasta la etapa corriente (**análisis semántico**) y
genera el archivo `<archivo>.sem` con el AST anotado con tipos y la
Tabla de Símbolos. Sin `-debug`, una compilación exitosa no imprime
nada por consola (los errores van a `stderr`).

### Ejecutar tests
```sh
make test                        # tests de la etapa 2 (análisis semántico)
./tests/run_tests.sh <directorio>  # cualquier directorio con tests .txt + .expected
```
> build/c-tds tests/01_scanner_parser/public/0...

Y luego se selecciona el número de test que se desee ejecutar (0, 1, 2, etc), apretando `tab` se completa automáticamente.

Los tests de la etapa 1 (scanner/parser) deben correrse con
`-target parse`, ya que algunos incumplen reglas semánticas a propósito:

> build/c-tds -target parse tests/01_scanner_parser/public/00_test_valido.txt
