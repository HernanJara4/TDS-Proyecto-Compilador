# Resumen Paso a Paso para el Examen Escrito Individual

En el examen escrito, el profesor suele dar un programa chiquito (de unas 6 a 10 líneas) y pedirte que expliques **paso a paso qué hace tu compilador desde que lee el archivo hasta que genera el código intermedio y el assembly**, dibujando las estructuras de datos que usaste.

Acá tenés la traza completa con un ejemplo concreto para que puedas reproducir exactamente la lógica de tu código en hoja y papel.

---

## El Programa de Ejemplo (supongamos que te dan este en el pizarrón/hoja)

```c
int g;
int inc(int x) {
    return x + 1;
}
void main() {
    int a;
    a = inc(5);
    g = a * 2;
}
```

---

## ETAPA 1a: Analizador Léxico (`src/scanner/Gramatica.l` - Flex)

1. **Cómo arranca:** `main()` abre el archivo y se lo asigna a `yyin`. Cuando Bison arranca `yyparse()`, cada vez que necesita el próximo símbolo llama a `yylex()`.
2. **Qué hace `Gramatica.l`:**
   * Ignora espacios, tabulaciones, saltos de línea y comentarios (`//` y `/* */`), pero gracias a `%option yylineno` y a la macro `YY_USER_ACTION`, antes de devolver cada token guarda la línea actual en `yylloc.first_line`.
   * Cuando lee una palabra reservada (`int`, `return`, `void`), devuelve el código numérico del token (`TIPO_INT`, `RETURN`, `TIPO_VOID`) sin usar `yylval`.
   * Cuando lee un identificador (`g`, `inc`, `x`, `main`, `a`), hace `yylval.sval = strdup(yytext);` (copia el nombre en memoria dinámica en el `%union` de Bison) y devuelve `ID`.
   * Cuando lee un número entero (`1`, `5`, `2`), hace `yylval.ival = atoi(yytext);` y devuelve `NRO`.
   * Si encontrara un símbolo inválido (como `@` o `$`), entra por la regla `.` del final, imprime el error léxico e incrementa `errores_lexicos`.

**Secuencia de tokens que salen para la primera línea (`int g;`):**
`TIPO_INT` $\rightarrow$ `ID (sval="g")` $\rightarrow$ `PUNTO_Y_COMA`.

---

## ETAPA 1b y 2a: Analizador Sintáctico y Construcción del AST (`Gramatica.y` + `ast.c`)

1. **Cómo funciona Bison:** Hace parsing ascendente **LALR(1)** (Shift/Reduce). Va apilando tokens (*shift*) y cuando el tope de la pila coincide con el lado derecho de una regla gramatical, aplica una reducción (*reduce*) ejecutando la acción `{ ... }` en C.
2. **Cómo está representado un nodo de nuestro AST (`include/ast.h`):**
   Usamos un **Nodo Genérico Ternario (`struct ASTNode`)** con el patrón **Primer Hijo / Siguiente Hermano**:
   ```c
   struct ASTNode {
       TipoNodo tipo;       // NODE_PROGRAMA, NODE_METODO, NODE_ASIGNACION, etc.
       int linea;           // linea del fuente (@1.first_line)
       Simbolo *simbolo;    // struct { char *tipo_dato; char *nombre_id; char *valor; }
       ASTNode *hijo1;
       ASTNode *hijo2;
       ASTNode *hijo3;      // usado solo en el else del NODE_IF
       ASTNode *siguiente;  // encadena listas (declaraciones, parametros, sentencias, args)
   };
   ```
3. **Cómo se va armando el árbol del ejemplo de abajo hacia arriba:**
   * En `int g;`: primero reduce `ListaId -> ID`, que llama a `crear_nodo(NODE_DECLARACION, linea, NULL, "g", NULL, ...)`. Después reduce `VarDecl -> Tipo ListaId ';'`, que llama a `asignar_tipo_lista()` y le pone `tipo_dato = "int"` al nodo `g`.
   * En `int inc(int x) { return x + 1; }`:
     * El parámetro `int x` crea un `NODE_DECLARACION` (`tipo="int"`, `nombre="x"`).
     * `x + 1` crea un `NODE_OP_BINARIA` (`valor="+"`) cuyo `hijo1` es `NODE_ID("x")` y `hijo2` es `NODE_NRO(tipo="int", valor="1")`.
     * `return x + 1;` crea un `NODE_RETORNO` cuyo `hijo1` es ese `NODE_OP_BINARIA("+")`.
     * Las llaves `{ ... }` crean un `NODE_BLOQUE` (`hijo1 = NULL` porque no hay variables locales en `inc`, `hijo2 = NODE_RETORNO`).
     * Finalmente `MethodDecl` crea un `NODE_METODO` (`tipo="int"`, `nombre="inc"`, `hijo1 = parametro x`, `hijo2 = NODE_BLOQUE`).
   * Las tres declaraciones globales (`g`, `inc`, `main`) quedan encadenadas entre sí por el puntero **`siguiente`**, y `raiz_ast` (`NODE_PROGRAMA`) apunta con su `hijo1` a la primera (`g`).

**Dibujo del AST que podés hacer en la hoja:**
```text
NODE_PROGRAMA (raiz_ast)
  └── hijo1: NODE_DECLARACION (nombre="g", tipo="int")
               │
               ▼ (siguiente)
             NODE_METODO (nombre="inc", tipo="int")
               ├── hijo1 (params): NODE_DECLARACION (nombre="x", tipo="int")
               ├── hijo2 (cuerpo): NODE_BLOQUE
               │                     └── hijo2 (stmts): NODE_RETORNO
               │                                          └── hijo1: NODE_OP_BINARIA (valor="+")
               │                                                       ├── hijo1: NODE_ID ("x")
               │                                                       └── hijo2: NODE_NRO ("1", tipo="int")
               ▼ (siguiente)
             NODE_METODO (nombre="main", tipo="void")
               ├── hijo1 (params): NULL
               └── hijo2 (cuerpo): NODE_BLOQUE
                     ├── hijo1 (locales): NODE_DECLARACION (nombre="a", tipo="int")
                     └── hijo2 (stmts):   NODE_ASIGNACION (nombre="a")
                                            ├── hijo1: NODE_LLAMADA (nombre="inc")
                                            │            └── hijo1 (args): NODE_NRO ("5", tipo="int")
                                            ▼ (siguiente)
                                          NODE_ASIGNACION (nombre="g")
                                            └── hijo1: NODE_OP_BINARIA (valor="*")
                                                         ├── hijo1: NODE_ID ("a")
                                                         └── hijo2: NODE_NRO ("2", tipo="int")
```

---

## ETAPA 2b: Tabla de Símbolos y Análisis Semántico (`tabla.c` + `typecheck.c`)

1. **Cómo funciona la Tabla de Símbolos (`tabla.h` / `tabla.c`):**
   * Es una **Pila de Niveles**: un arreglo `EntradaTS *TS[MAX_NIVELES]` más un entero `nivel_actual`.
   * Cada nivel es una lista enlazada de `EntradaTS`.
   * Cada `EntradaTS` **NO duplica** el nombre ni el tipo: guarda un puntero al **mismo `Simbolo *` del nodo del AST** (`entrada->simbolo = nodo->simbolo`), más `categoria` (`CAT_VAR_GLOBAL`, `CAT_VAR_LOCAL`, `CAT_PARAMETRO`, `CAT_FUNCION`), `cantidad_parametros` y `tipos_parametros[]`.

2. **Traza paso a paso de `analizar_semantica(raiz_ast)` sobre el ejemplo:**
   * **Paso 1:** Llama a `ts_inicializar()` (`nivel_actual = 0`, ámbito global).
   * **Paso 2 (`int g;`):** Visita `NODE_DECLARACION("g")`. Llama a `ts_insertar()` en `TS[0]` con `CAT_VAR_GLOBAL`. Como `"g"` no estaba en `TS[0]`, entra bien (Regla 1).
   * **Paso 3 (`int inc(int x)`):**
     * Crea la entrada de `"inc"` con `CAT_FUNCION`, `cantidad_parametros = 1`, `tipos_parametros[0] = "int"`, y la inserta en `TS[0]`.
     * Guarda `metodo_actual = nodo_inc`.
     * Llama a **`ts_abrir_nivel()`** $\rightarrow$ `nivel_actual` pasa a **`1`**.
     * Inserta el parámetro `"x"` (`CAT_PARAMETRO`, `tipo="int"`) en `TS[1]`.
     * Recorre el `NODE_BLOQUE` de `inc` con `abrir_scope = 0` (para que las variables locales del cuerpo compartan el Nivel 1 con los parámetros).
     * Entra a `NODE_RETORNO`: evalúa `x + 1`. Busca `"x"` con `ts_buscar("x")`: empieza buscando en `TS[1]`, lo encuentra (`tipo="int"`), y **anota** `tipo_dato = "int"` en `NODE_ID("x")`. Como `"1"` también es `"int"`, anota `tipo_dato = "int"` en `NODE_OP_BINARIA("+")` (Regla 10). Compara ese `"int"` contra el retorno de `metodo_actual` (`"int"`): coinciden (Reglas 6 y 7).
     * Sale de `inc` y llama a **`ts_cerrar_nivel()`**: hace `free()` de los nodos `EntradaTS` de `TS[1]` (sin liberar `simbolo`, que queda en el AST), y `nivel_actual` vuelve a **`0`**.
   * **Paso 4 (`void main()`):**
     * Inserta `"main"` (`CAT_FUNCION`, `0` parámetros) en `TS[0]`.
     * Abre el Nivel 1 (`ts_abrir_nivel()`).
     * Inserta la variable local `"a"` (`CAT_VAR_LOCAL`, `tipo="int"`) en `TS[1]`.
     * En `a = inc(5);`:
       * Chequea la llamada `inc(5)`: busca `"inc"` desde `TS[1]` hacia `TS[0]`, la encuentra en `TS[0]` (`CAT_FUNCION`). Verifica que tiene 1 argumento y que `"5"` (`int`) es compatible con `tipos_parametros[0]` (`int`) (Regla 4). Como `inc` retorna `"int"` (no es `void`), es válido usarla en una expresión (Regla 5). Anota `tipo="int"` en `NODE_LLAMADA`.
       * Busca `"a"` en la TS: la encuentra en `TS[1]` (`CAT_VAR_LOCAL`, `tipo="int"`). Como ambos lados del `=` son `"int"`, la asignación es válida (Reglas 8 y 13).
     * En `g = a * 2;`: busca `"a"` (lo halla en `TS[1]`) y `"g"` (no está en `TS[1]`, baja a `TS[0]` y lo halla ahí). Ambos son `"int"`, todo válido.
     * Cierra el Nivel 1 (`ts_cerrar_nivel()`).
   * **Paso 5 (Fin del programa):** Busca `"main"` en `TS[0]` con `ts_buscar_en_nivel("main", 0)` y verifica que sea `CAT_FUNCION` con `0` parámetros (Regla 3).

---

## ETAPA 3: Generación de Código Intermedio (`src/ir/ir.c`)

`generar_ir(raiz_ast)` recorre el **AST Anotado** y emite la lista enlazada de cuádruplas (`InstruccionIR`):
* Las hojas (`NODE_NRO` y `NODE_ID`) devuelven su nombre directo (`"1"`, `"x"`, `"a"`, `"5"`).
* Cada operación o llamada con retorno pide un `nuevo_temporal()` (`t0`, `t1`, `t2`...).

**Código de Tres Direcciones que se genera para el ejemplo:**
```text
global int g

func int inc:
    formal int x
    t0 = x + 1
    return t0
endfunc inc

func void main:
    local int a
    param 5
    t1 = call inc, 1
    a = t1
    t2 = a * 2
    g = t2
endfunc main
```
*(Si se corre con `-opt all`, el optimizador fusiona las copias de `t1` y `t2`, dejando `a = call inc, 1` y `g = a * 2`).*

---

## ETAPA 4: Generación de Código Objeto Assembly x86-64 (`src/asm/codegen.c`)

1. **Variables globales:** `global int g` se emite en `.bss` (`g: .zero 8`) y se accede como `g(%rip)`.
2. **Construcción del Stack Frame por función (`construir_frame_funcion`):**
   * En `inc`: tiene `x` (`-8(%rbp)`) y `t0` (`-16(%rbp)`). Total = 16 bytes (ya es múltiplo de 16) $\rightarrow$ `subq $16, %rsp`.
   * En `main`: tiene `a` (`-8(%rbp)`), `t1` (`-16(%rbp)`) y `t2` (`-24(%rbp)`). Total = 24 bytes $\rightarrow$ redondeado a múltiplo de 16 da **`subq $32, %rsp`**.
3. **Traducción de las instrucciones de `inc` y `main`:**
   ```asm
   inc:
       pushq %rbp
       movq %rsp, %rbp
       subq $16, %rsp
       movq %rdi, -8(%rbp)      # guarda el 1er parametro x (que vino en %rdi)
       movq -8(%rbp), %rax
       movq $1, %rcx
       addq %rcx, %rax          # x + 1
       movq %rax, -16(%rbp)     # t0 = x + 1
       movq -16(%rbp), %rax     # valor de retorno en %rax
       leave
       ret

   main:
       pushq %rbp
       movq %rsp, %rbp
       subq $32, %rsp
       movq $5, %rdi            # param 5 -> 1er argumento en %rdi
       call inc
       movq %rax, -16(%rbp)     # t1 = retorno de inc
       movq -16(%rbp), %rax
       movq %rax, -8(%rbp)      # a = t1
       movq -8(%rbp), %rax
       movq $2, %rcx
       imulq %rcx, %rax         # a * 2
       movq %rax, -24(%rbp)     # t2 = a * 2
       movq -24(%rbp), %rax
       movq %rax, g(%rip)       # g = t2 (variable global)
       movq $0, %rax
       leave
       ret
   ```
