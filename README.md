# Taller-Diseño-De-Software---Proyecto-Compilador
Desarrollo de un compilador incremental para el lenguaje de programación C-TDS. Proyecto académico realizado para la materia Taller de Diseño de Software de la Universidad Nacional de Río Cuarto.


### Compilar parser
1. Ejecutar flex
flex -o src/build/lex.yy.c src/scanner/Gramatica.l

2. Ejecutar bison
bison -d -o src/build/Gramatica.tab.c src/parser/Gramatica.y

3. Compilar
gcc -I src/build -o bin/compilador src/build/Gramatica.tab.c src/build/lex.yy.c

### Ejecutar tests

> bin/c-tds tests/01_scanner_parser/public/0...

Y luego se selecciona el número de test que se desee ejecutar (0, 1, 2, etc), apretando `tab` se completa automáticamente