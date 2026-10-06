#!/usr/bin/env bash
# =====================================================================
# run_tests.sh - runner de los casos de test del compilador
#
# Uso:
#   ./tests/run_tests.sh                  # tests/02_semantic/public
#   ./tests/run_tests.sh tests/01_.../public
#
# Un "test" es un par:
#   NN_descripcion.txt     codigo fuente C-TDS de entrada
#   NN_descripcion.expected  salida esperada (formato abajo)
#
# Formato del .expected (todas las secciones son opcionales):
#   ### ARGS: <args>     argumentos con que se invoca (default: ninguno)
#   ### EXIT: <n>        codigo de salida esperado (default: 0)
#   ### STDOUT           stdout esperado (default: vacio)
#   ### STDERR           stderr esperado (default: vacio)
#   ### SEM              contenido esperado del archivo .sem
#                       (si la seccion falta, el .sem NO debe existir)
#
# El test se corre en un directorio temporal (para no ensuciar la
# carpeta de tests con los archivos que genera el compilador).
# =====================================================================

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TESTS_DIR="${1:-$ROOT/tests/02_semantic/public}"
BIN="$ROOT/build/c-tds"

if [ ! -x "$BIN" ]; then
    echo "No existe $BIN. Compilar primero con: make" >&2
    exit 2
fi

total=0
fallos=0

for entrada in "$TESTS_DIR"/*.txt; do
    [ -e "$entrada" ] || continue
    esperado="${entrada%.txt}.expected"
    [ -f "$esperado" ] || continue      # sin .expected no es test

    total=$((total + 1))
    nombre=$(basename "$entrada")

    tmp=$(mktemp -d)
    cp "$entrada" "$tmp/programa.txt"

    # partir el .expected en sus secciones
    : > "$tmp/exp_args"
    : > "$tmp/exp_exit"
    : > "$tmp/exp_stdout"
    : > "$tmp/exp_stderr"
    : > "$tmp/exp_sem"
    awk -v out="$tmp" '
        /^### ARGS: /  { print substr($0, 11) > (out "/exp_args");  next }
        /^### EXIT: /  { print substr($0, 11) > (out "/exp_exit");  next }
        /^### STDOUT$/ { sec = "stdout"; next }
        /^### STDERR$/ { sec = "stderr"; next }
        /^### SEM$/    { sec = "sem";    next }
        /^### /        { sec = "";       next }
        {
            if (sec == "stdout") print > (out "/exp_stdout")
            else if (sec == "stderr") print > (out "/exp_stderr")
            else if (sec == "sem") print > (out "/exp_sem")
        }
    ' "$esperado"

    args=$(cat "$tmp/exp_args")
    esperado_exit=$(cat "$tmp/exp_exit")
    [ -n "$esperado_exit" ] || esperado_exit=0

    (cd "$tmp" && "$BIN" $args programa.txt > stdout 2> stderr)
    codigo=$?

    errores=""
    if [ "$codigo" != "$esperado_exit" ]; then
        errores="$errores\n  codigo de salida: esperado $esperado_exit, obtenido $codigo"
    fi
    if ! diff -q "$tmp/exp_stdout" "$tmp/stdout" > /dev/null 2>&1; then
        errores="$errores\n  stdout distinto del esperado"
    fi
    if ! diff -q "$tmp/exp_stderr" "$tmp/stderr" > /dev/null 2>&1; then
        errores="$errores\n  stderr distinto del esperado"
    fi
    if [ -s "$tmp/exp_sem" ]; then
        if [ ! -f "$tmp/programa.sem" ]; then
            errores="$errores\n  no se genero el archivo .sem"
        elif ! diff -q "$tmp/exp_sem" "$tmp/programa.sem" > /dev/null 2>&1; then
            errores="$errores\n  contenido de .sem distinto del esperado"
        fi
    elif [ -f "$tmp/programa.sem" ]; then
        errores="$errores\n  se genero un .sem que no deberia existir"
    fi

    if [ -z "$errores" ]; then
        printf '  PASS  %s\n' "$nombre"
    else
        printf '  FAIL  %s%b\n' "$nombre" "$errores"
        fallos=$((fallos + 1))
        printf '        --- detalle (esperado vs obtenido) ---\n'
        diff -u "$tmp/exp_stdout" "$tmp/stdout" | sed 's/^/        stdout /' | head -40
        diff -u "$tmp/exp_stderr" "$tmp/stderr" | sed 's/^/        stderr /' | head -40
        diff -u "$tmp/exp_sem" "$tmp/programa.sem" 2>/dev/null | sed 's/^/        .sem   /' | head -40
    fi

    rm -rf "$tmp"
done

echo
echo "Tests: $total, fallos: $fallos"
[ "$fallos" -eq 0 ] || exit 1
exit 0
