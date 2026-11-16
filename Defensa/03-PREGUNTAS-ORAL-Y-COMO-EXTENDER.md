# Guía para la Defensa Oral: Preguntas Clave del Profe y Cómo Extender el Compilador en Vivo

Este documento tiene dos partes:
1. **Las preguntas teóricas/técnicas más probables** que te puede hacer el profesor sobre tu código y cómo responderlas con seguridad.
2. **Recetas paso a paso ("machete de extensión")** por si en la defensa oral te pide agregar un operador o una sentencia nueva en el momento.

---

# PARTE 1: Preguntas Clave que te puede hacer el Profesor

### 1. "¿Tuviste algún conflicto en Bison al escribir la gramática? ¿Cómo lo arreglaste?"
* **Respuesta:** Sí, cuando escribí `program -> var_decl* method_decl*` como dos listas separadas tuve **3 conflictos shift/reduce** (en `int`, `boolean` y `float`). Pasaba porque al terminar las variables globales y ver un tipo como `int`, con un solo token de lookahead (LALR(1)) Bison no sabe si hacer *shift* para otra variable global o *reduce* para empezar la lista de métodos.
* **Solución:** Unifiqué ambas listas en `ListaDeclaraciones` y eliminé el no terminal intermedio `TipoRetorno` en `MethodDecl`. Así Bison siempre hace *shift* del `ID` después del `Tipo`, y con el siguiente token (`,`, `;` o `(`) distingue sin ambigüedad si es variable o método. Y para que no puedan declarar una variable global después de un método, controlo ese orden en `typecheck.c` con el flag `ya_hubo_metodo`.

### 2. "¿Cómo manejás la precedencia del menos unario (`-x`) si usa el mismo token que la resta (`x - y`)?"
* **Respuesta:** En `Gramatica.y` declaré un token ficticio `%right UMENOS` en el nivel más alto de la tabla de precedencias (abajo de todo), y en la regla de la expresión unaria puse `OP_RESTA Expr %prec UMENOS`. Eso le indica a Bison que esa regla tiene la precedencia de `UMENOS` y no la de `OP_RESTA`.

### 3. "¿Cómo evitás el doble `free` si tanto el AST como la Tabla de Símbolos apuntan al mismo `Simbolo *`?"
* **Respuesta:** Decidí que el **único dueño** de la memoria de cada `Simbolo *` sea el nodo del AST. La Tabla de Símbolos guarda en `EntradaTS` un puntero prestado (`entrada->simbolo = nodo->simbolo`). Cuando termina un bloque y `ts_cerrar_nivel()` destruye ese nivel de la TS, hace `free(actual)` únicamente sobre el nodo `EntradaTS` (el envoltorio de la lista), pero **nunca** hace `free(actual->simbolo)`. Los `Simbolo *` se liberan una sola vez al final del programa cuando `main()` llama a `liberar_ast(raiz_ast)`.

### 4. "¿Cómo hacés para que un parámetro y una variable local del cuerpo del método no se puedan llamar igual, pero adentro de un `if` sí se pueda repetir el nombre?"
* **Respuesta:** En `typecheck.c`, la función `chequear_bloque(ASTNode *bloque, int abrir_scope)` recibe una bandera `abrir_scope`. Cuando analizo un `NODE_METODO`, abro el Nivel 1 (`ts_abrir_nivel()`), inserto ahí los parámetros formales y llamo al bloque principal del método con `abrir_scope = 0`. Así, las variables locales del cuerpo se insertan en ese **mismo Nivel 1** y si repiten el nombre de un parámetro salta error de redeclaración. En cambio, para los bloques de `if`, `else`, `while` o bloques internos llamo a `chequear_bloque(bloque, 1)`, que abre el Nivel 2 (o superior) permitiendo hacer *shadowing*.

### 5. "¿Cómo implementaste el corto circuito de `&&` y `||` en el Código Intermedio?"
* **Respuesta:** En `src/ir/ir.c` (`gen_expresion`), no los traduzco como operadores binarios comunes sino con **saltos condicionales y etiquetas**. Para `E1 && E2`, evalúo primero `E1` y emito `ifFalse izq goto L_false`. Si `E1` dio falso, salta directo a `L_false` donde asigna `t_res = false`, salteándose por completo todas las instrucciones de `E2`.

### 6. "¿Qué pasa en el Código Intermedio y en Assembly si hay dos variables locales llamadas `x` en la misma función (una afuera y otra adentro de un `if`)?"
* **Respuesta:** Como en el Código Intermedio desaparecen las llaves `{ }` y la función se aplana, si ambas se llamaran `"x"` se pisarían en el mismo slot de la pila. Para evitarlo, en `registrar_declaracion()` (`typecheck.c`), si estoy en `nivel_actual >= 2` y veo que el nombre ya existe en un nivel local superior de la misma función (`1 .. nivel_actual - 1`), le genero un alias único (`"x.1"`, `"x.2"`) en `simbolo->valor` y se lo copio a los `NODE_ID` y `NODE_ASIGNACION` de ese bloque. Así en el IR y en el Stack Frame cada una tiene su propio lugar (`x` y `x.1`).

---

# PARTE 2: Cómo Extender el Compilador en Vivo en 5 Minutos

En un compilador modular como el nuestro, agregar cualquier cosa nueva sigue siempre **la misma ruta de 5 estaciones**:
1. **Scanner** (`src/scanner/Gramatica.l`)
2. **Parser** (`src/parser/Gramatica.y`)
3. **AST** (`include/ast.h` / `src/ast/ast.c` — *solo si es un tipo de nodo nuevo*)
4. **Semántico** (`src/semantic/typecheck.c`)
5. **IR y Assembly** (`include/ir.h`, `src/ir/ir.c`, `src/asm/codegen.c`)

Acá tenés las **4 extensiones más clásicas** que suelen pedir en la defensa oral de TDS:

---

## Caso A: Agregar nuevos operadores de comparación (`!=`, `<=`, `>=`)
*(Tip: Como usan `NODE_OP_BINARIA`, **NO hace falta tocar `ast.h` ni `ast.c`**).*

1. **`src/scanner/Gramatica.l`** (en la sección de Operadores):
   ```c
   "!="        { return OP_DISTINTO; }
   "<="        { return OP_MENOR_IGUAL; }
   ">="        { return OP_MAYOR_IGUAL; }
   ```
2. **`src/parser/Gramatica.y`**:
   * En `%token`: agregar `OP_DISTINTO OP_MENOR_IGUAL OP_MAYOR_IGUAL`
   * En precedencias:
     ```c
     %nonassoc OP_IGUAL OP_DISTINTO
     %nonassoc OP_MENOR OP_MAYOR OP_MENOR_IGUAL OP_MAYOR_IGUAL
     ```
   * En la regla `Expr`:
     ```c
     | Expr OP_DISTINTO Expr    { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("!="), $1, $3, NULL); }
     | Expr OP_MENOR_IGUAL Expr { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("<="), $1, $3, NULL); }
     | Expr OP_MAYOR_IGUAL Expr { $$ = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup(">="), $1, $3, NULL); }
     ```
3. **`src/semantic/typecheck.c`** (adentro de `case NODE_OP_BINARIA:`):
   * En el `if` de `<` y `>`: agregar `|| strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0`
   * En el `if` de `==`: agregar `|| strcmp(op, "!=") == 0`
4. **`include/ir.h` y `src/ir/ir.c`**:
   * En `OpIR` (`ir.h`): agregar `IR_NEQ, IR_LE, IR_GE`.
   * En `gen_expresion()` (`ir.c`):
     ```c
     else if (strcmp(op, "!=") == 0) emitir(IR_NEQ, tipo, izq, der, tmp);
     else if (strcmp(op, "<=") == 0) emitir(IR_LE,  tipo, izq, der, tmp);
     else if (strcmp(op, ">=") == 0) emitir(IR_GE,  tipo, izq, der, tmp);
     ```
5. **`src/asm/codegen.c`** (en el `case IR_LT: case IR_GT: case IR_EQ:`):
   * Agregar `case IR_NEQ: case IR_LE: case IR_GE:` y usar `setne %al` (`!=`), `setle %al` (`<=`) y `setge %al` (`>=`).

---

## Caso B: Agregar asignación compuesta (`x += expr;` o `x -= expr;`) o incremento (`x++;`)
*(El truco más limpio y rápido: **desazucararlo en `Gramatica.y`** armando los nodos que ya existen, sin tener que tocar `typecheck.c`, `ir.c` ni `codegen.c`!)*

* ¿Qué es `x += expr;`? Es exactamente lo mismo que `x = x + (expr);`.
* Entonces, si te piden agregar `+=`:
  1. En **`Gramatica.l`**:
     ```c
     "+="        { return ASIG_SUMA; }
     ```
  2. En **`Gramatica.y`**: declarás `%token ASIG_SUMA` y en `Sentencia` agregás:
     ```c
     | ID ASIG_SUMA Expr PUNTO_Y_COMA
         {
             ASTNode *id_uso = crear_nodo(NODE_ID, @1.first_line, NULL, strdup($1), NULL, NULL, NULL, NULL);
             ASTNode *suma   = crear_nodo(NODE_OP_BINARIA, @2.first_line, NULL, NULL, strdup("+"), id_uso, $3, NULL);
             $$ = crear_nodo(NODE_ASIGNACION, @1.first_line, NULL, $1, NULL, suma, NULL, NULL);
         }
     ```
  **¡Y listo!** Como en el AST ya queda armado como un `NODE_ASIGNACION` con un `NODE_OP_BINARIA("+")` adentro, el analizador semántico, el código intermedio, el optimizador y el assembly x86-64 **ya funcionan automáticamente sin tocar una sola línea más**.
  *(Lo mismo sirve si te piden `x++;` $\rightarrow$ creás la suma con `NODE_NRO("1")` y la metés en `NODE_ASIGNACION`)*.

---

## Caso C: Agregar un ciclo `do <block> while ( <expr> ) ;` o `repeat <block> until ( <expr> ) ;`
Si te piden `do Bloque while ( Expr ) ;`:
1. **`Gramatica.l`**: agregar `"do" { return DO; }`
2. **`Gramatica.y`**: declarar `%token DO` y en `Sentencia`:
   ```c
   | DO Bloque WHILE PAR_IZQ Expr PAR_DER PUNTO_Y_COMA
       { $$ = crear_nodo(NODE_DO_WHILE, @1.first_line, NULL, NULL, NULL, $5, $2, NULL); }
   ```
   *(Fijate que pusimos la condición `$5` en `hijo1` y el `Bloque` `$2` en `hijo2`, igual que en `NODE_WHILE`)*.
3. **`include/ast.h` y `src/ast/ast.c`**: agregar `NODE_DO_WHILE` al `enum TipoNodo` y al `switch` de `nombre_tipo_nodo()`.
4. **`src/semantic/typecheck.c`**: en `chequear_sentencia()`, abajo de `case NODE_WHILE:`, agregás `case NODE_DO_WHILE:` (¡hace exactamente el mismo chequeo: verifica que `hijo1` sea `"boolean"` y llama a `chequear_bloque(stmt->hijo2, 1)`!).
5. **`src/ir/ir.c`**: en `gen_sentencia()` agregás `case NODE_DO_WHILE:`:
   ```c
   case NODE_DO_WHILE: {
       char *lbl_inicio = nueva_etiqueta();
       emitir(IR_LABEL, NULL, NULL, NULL, lbl_inicio);
       gen_bloque(stmt->hijo2);              // 1ro ejecuta el cuerpo
       char *cond = gen_expresion(stmt->hijo1);
       emitir(IR_JMP_TRUE, "boolean", cond, NULL, lbl_inicio); // Si da true, vuelve arriba
       free(cond);
       free(lbl_inicio);
       break;
   }
   ```
   *(Y como usa `IR_LABEL` e `IR_JMP_TRUE` que ya existen, **no tenés que tocar nada en `codegen.c`**)*.

---

## Caso D: Agregar una sentencia `for ( id = expr ; expr ; id = expr ) <block>`
Igual que en el Caso B, un `for (init; cond; step) { cuerpo }` equivale a:
1. Ejecutar `init`
2. Un `while (cond)` cuyo bloque ejecuta el `cuerpo` y al final ejecuta `step`.
Podés desazucararlo directo en `Gramatica.y` encadenando `step` al final de `bloque->hijo2` con `agregar_a_lista(bloque->hijo2, step)` y encadenando `init` con el `NODE_WHILE` usando `agregar_a_lista(init, nodo_while)`!
