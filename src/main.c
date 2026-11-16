#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ast.h"
#include "../include/typecheck.h"
#include "../include/ir.h"
#include "../include/codegen.h"
#include "../include/optimizer.h"

extern int   yyparse(void);
extern int   yylex(void);
extern FILE *yyin;
extern int   errores_lexicos;
extern int   errores_sintacticos;

static int termina_con(const char *str, const char *sufijo) {
    if (str == NULL || sufijo == NULL)
        return 0;
    size_t len_s = strlen(str);
    size_t len_f = strlen(sufijo);
    if (len_f > len_s)
        return 0;
    return strcmp(str + len_s - len_f, sufijo) == 0;
}

/* Reemplaza la extension de "archivo" por "nueva_ext" y la guarda en "dest" */
static void construir_nombre_salida(const char *archivo, const char *nueva_ext,
                                    char *dest, size_t tam) {
    snprintf(dest, tam, "%s", archivo);
    char *punto = strrchr(dest, '.');
    if (punto != NULL)
        *punto = '\0';
    strncat(dest, nueva_ext, tam - strlen(dest) - 1);
}

static int es_modo_opt_valido(const char *s) {
    return s != NULL &&
           (strcmp(s, "all") == 0 || strcmp(s, "cf") == 0 ||
            strcmp(s, "cp") == 0  || strcmp(s, "dce") == 0);
}

int main(int argc, char **argv) {
    const char *archivo   = NULL;
    const char *salida    = NULL;
    const char *target    = NULL; /* NULL indica compilacion completa hasta .out */
    const char *modo_opt  = NULL; /* Por defecto no se realiza ninguna optimizacion */
    int debug = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-debug") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "-target") == 0 && i + 1 < argc) {
            target = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            salida = argv[++i];
        } else if (strcmp(argv[i], "-opt") == 0) {
            if (i + 1 < argc && es_modo_opt_valido(argv[i + 1]))
                modo_opt = argv[++i];
            else
                modo_opt = "all"; /* -opt sin argumento aplica todas las optimizaciones */
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Opcion desconocida o invalida: '%s'\n", argv[i]);
            return 1;
        } else {
            archivo = argv[i];
        }
    }

    int es_ctds = 0;
    if (archivo != NULL) {
        es_ctds = termina_con(archivo, ".ctds");
        if (!es_ctds && !termina_con(archivo, ".txt")) {
            fprintf(stderr,
                    "Error: el archivo de entrada '%s' debe tener extension '.ctds'\n",
                    archivo);
            return 1;
        }

        FILE *file = fopen(archivo, "r");
        if (!file) {
            fprintf(stderr, "No se pudo abrir el archivo '%s'.\n", archivo);
            return 1;
        }
        yyin = file;
    }

    char ruta_defecto[512];

    /* 1. Etapa de Scanner (-target scan) */
    if (target != NULL && strcmp(target, "scan") == 0) {
        while (yylex() != 0) {
            /* consume todos los tokens */
        }
        if (yyin != NULL && yyin != stdin)
            fclose(yyin);
        if (errores_lexicos > 0) {
            fprintf(stderr, "\nSe encontraron %d error/es lexico/s.\n", errores_lexicos);
            return 1;
        }
        if (es_ctds || salida != NULL) {
            const char *out_lex = salida;
            if (out_lex == NULL) {
                construir_nombre_salida(archivo, ".lex", ruta_defecto, sizeof ruta_defecto);
                out_lex = ruta_defecto;
            }
            FILE *f_lex = fopen(out_lex, "w");
            if (f_lex != NULL) {
                fprintf(f_lex, "Analisis lexico exitoso.\n");
                fclose(f_lex);
            }
        }
        if (debug)
            printf("Analisis lexico exitoso.\n");
        return 0;
    }

    /* 2. Etapa de Parsing (-target parse) */
    yyparse();

    if (yyin != NULL && yyin != stdin)
        fclose(yyin);

    if (errores_lexicos > 0 || errores_sintacticos > 0) {
        fprintf(stderr, "\nSe encontraron %d error/es lexico/s y %d error/es sintactico/s.\n",
                errores_lexicos, errores_sintacticos);
        liberar_ast(raiz_ast);
        return 1;
    }

    if (target != NULL && strcmp(target, "parse") == 0) {
        if (es_ctds || salida != NULL) {
            const char *out_sint = salida;
            if (out_sint == NULL) {
                construir_nombre_salida(archivo, ".sint", ruta_defecto, sizeof ruta_defecto);
                out_sint = ruta_defecto;
            }
            FILE *f_sint = fopen(out_sint, "w");
            if (f_sint != NULL) {
                fprintf(f_sint, "Analisis sintactico exitoso.\n");
                fclose(f_sint);
            }
        }
        if (debug) {
            printf("Analisis lexico y sintactico exitoso. AST:\n\n");
            imprimir_ast(raiz_ast, 0);
        }
        liberar_ast(raiz_ast);
        return 0;
    }

    /* 3. Etapa de Analisis Semantico (-target sem) */
    if (analizar_semantica(raiz_ast) > 0) {
        fprintf(stderr, "\nSe encontraron %d error/es semantico/s.\n",
                errores_semanticos);
        liberar_ast(raiz_ast);
        return 1;
    }

    if (target != NULL && strcmp(target, "sem") == 0) {
        if (es_ctds || salida != NULL) {
            const char *out_sem = salida;
            if (out_sem == NULL) {
                construir_nombre_salida(archivo, ".sem", ruta_defecto, sizeof ruta_defecto);
                out_sem = ruta_defecto;
            }
            FILE *f_sem = fopen(out_sem, "w");
            if (f_sem != NULL) {
                fprintf(f_sem, "Analisis semantico exitoso.\n");
                fclose(f_sem);
            }
        }
        if (debug) {
            printf("Analisis lexico, sintactico y semantico exitoso. AST:\n\n");
            imprimir_ast(raiz_ast, 0);
        }
        liberar_ast(raiz_ast);
        return 0;
    }

    /* 4. Etapa de Codigo Intermedio (-target codinter) y Optimizador (-opt) */
    InstruccionIR *ir = generar_ir(raiz_ast);

    if (debug) {
        printf("Analisis exitoso. AST anotado:\n\n");
        imprimir_ast(raiz_ast, 0);
        printf("\n--- Codigo Intermedio de Tres Direcciones (IR) ---\n");
        imprimir_ir(ir, stdout);
    }

    if (modo_opt != NULL) {
        int cambios = optimizar_ir(ir, modo_opt);
        if (debug) {
            printf("\n--- Codigo Intermedio Optimizado (-opt %s, %d transformaciones) ---\n",
                   modo_opt, cambios);
            imprimir_ir(ir, stdout);
        }
        if (es_ctds && archivo != NULL) {
            char ruta_opt[512];
            construir_nombre_salida(archivo, ".opt", ruta_opt, sizeof ruta_opt);
            FILE *f_opt = fopen(ruta_opt, "w");
            if (f_opt != NULL) {
                imprimir_ir(ir, f_opt);
                fclose(f_opt);
            }
        }
    }

    if (target != NULL && strcmp(target, "codinter") == 0) {
        if (es_ctds || salida != NULL) {
            const char *out_ci = salida;
            if (out_ci == NULL) {
                construir_nombre_salida(archivo, ".ci", ruta_defecto, sizeof ruta_defecto);
                out_ci = ruta_defecto;
            }
            FILE *f_out = fopen(out_ci, "w");
            if (!f_out) {
                fprintf(stderr, "No se pudo crear el archivo de salida '%s'.\n", out_ci);
                liberar_ir(ir);
                liberar_ast(raiz_ast);
                return 1;
            }
            imprimir_ir(ir, f_out);
            fclose(f_out);
        }
        liberar_ir(ir);
        liberar_ast(raiz_ast);
        return 0;
    }

    /* 5. Etapa de Generacion de Codigo Objeto x86-64 (-target assembly o compilacion completa) */
    if (debug) {
        printf("\n--- Codigo Objeto Assembly x86-64 ---\n");
        generar_asm(ir, stdout);
    }

    if (target != NULL && strcmp(target, "assembly") == 0 && salida == NULL && es_ctds) {
        /* Si se pidio -target assembly sobre un .ctds sin -o, generar nombreArchivo.ass */
        construir_nombre_salida(archivo, ".ass", ruta_defecto, sizeof ruta_defecto);
        salida = ruta_defecto;
    } else if (target == NULL && salida == NULL && es_ctds) {
        /* Si no se indico -target ni -o sobre un .ctds, generar el ejecutable nombreArchivo.out */
        construir_nombre_salida(archivo, ".out", ruta_defecto, sizeof ruta_defecto);
        salida = ruta_defecto;
    }

    if (salida != NULL) {
        if (termina_con(salida, ".s") || termina_con(salida, ".ass")) {
            FILE *f_asm = fopen(salida, "w");
            if (!f_asm) {
                fprintf(stderr, "No se pudo crear el archivo '%s'.\n", salida);
                liberar_ir(ir);
                liberar_ast(raiz_ast);
                return 1;
            }
            generar_asm(ir, f_asm);
            fclose(f_asm);
        } else {
            char tmp_asm[520];
            snprintf(tmp_asm, sizeof tmp_asm, "%s.s", salida);
            FILE *f_asm = fopen(tmp_asm, "w");
            if (!f_asm) {
                fprintf(stderr, "No se pudo crear el archivo temporal '%s'.\n", tmp_asm);
                liberar_ir(ir);
                liberar_ast(raiz_ast);
                return 1;
            }
            generar_asm(ir, f_asm);
            fclose(f_asm);

            char cmd[1200];
            snprintf(cmd, sizeof cmd, "gcc -Wl,-z,noexecstack -x assembler %s -o %s", tmp_asm, salida);
            int rc = system(cmd);
            remove(tmp_asm);
            if (rc != 0) {
                fprintf(stderr, "Error al ensamblar y linkear con gcc.\n");
                liberar_ir(ir);
                liberar_ast(raiz_ast);
                return 1;
            }
        }
    }

    liberar_ir(ir);
    liberar_ast(raiz_ast);
    return 0;
}
