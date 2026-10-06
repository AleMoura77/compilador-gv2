#include "semantic.h"
#include "symtab.h"
#include "g-v2.tab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SymbolTableStack stack;

static ValueType tipo_retorno_funcao_atual = TIPO_INVALIDO;
static int dentro_de_funcao = 0;

static void erro_semantico(const char *msg, int linha) {
    printf("ERRO SEMANTICO: %s NA LINHA %d\n", msg, linha);
    symtab_free(&stack);
    exit(1);
}

static int eh_no_vetor(ASTNode *no);

static ValueType tipo_do_no(ASTNode *no);

static int contar_lista(ASTNode *lista) {
    int n = 0;

    while (lista != NULL) {
        n++;
        lista = lista->proximo;
    }

    return n;
}

static ParamInfo *criar_lista_param_info(ASTNode *parametros) {
    ParamInfo *lista = NULL;
    int pos = 1;

    while (parametros != NULL) {
        if (parametros->type == NODE_PARAMETRO) {
            ParamInfo *p = symtab_create_param_info(
                parametros->id_name,
                parametros->value_type,
                parametros->eh_vetor,
                pos
            );

            lista = symtab_append_param_info(lista, p);
            pos++;
        }

        parametros = parametros->proximo;
    }

    return lista;
}

static void inserir_declaracao_variavel(ASTNode *decl, int *posicao) {
    ValueType tipo;

    if (decl == NULL || decl->type != NODE_DECLARACAO) {
        return;
    }

    tipo = decl->value_type;

    if (decl->esq != NULL && decl->esq->type == NODE_TIPO) {
        tipo = decl->esq->value_type;
    }

    if (tipo == TIPO_INVALIDO) {
        erro_semantico("DECLARACAO COM TIPO INVALIDO", decl->linha);
    }

    if (decl->eh_vetor && decl->tamanho_vetor <= 0) {
        erro_semantico("VETOR DECLARADO COM TAMANHO INVALIDO", decl->linha);
    }

    if (!symtab_insert_variable(
            &stack,
            decl->id_name,
            tipo,
            *posicao,
            decl->linha,
            decl->eh_vetor,
            decl->tamanho_vetor
        )) {
        erro_semantico("IDENTIFICADOR JA DECLARADO NO ESCOPO", decl->linha);
    }

    (*posicao)++;
}

static void inserir_lista_declaracoes(ASTNode *decls, int *posicao) {
    while (decls != NULL) {
        if (decls->type == NODE_DECLARACAO) {
            inserir_declaracao_variavel(decls, posicao);
        }

        decls = decls->proximo;
    }
}

static void inserir_parametros_no_escopo(ASTNode *parametros, int *posicao) {
    while (parametros != NULL) {
        if (parametros->type == NODE_PARAMETRO) {
            if (parametros->value_type == TIPO_INVALIDO) {
                erro_semantico("PARAMETRO COM TIPO INVALIDO", parametros->linha);
            }

            if (!symtab_insert_parameter(
                    &stack,
                    parametros->id_name,
                    parametros->value_type,
                    *posicao,
                    parametros->linha,
                    parametros->eh_vetor
                )) {
                erro_semantico("PARAMETRO JA DECLARADO NO ESCOPO DA FUNCAO", parametros->linha);
            }

            (*posicao)++;
        }

        parametros = parametros->proximo;
    }
}

static void inserir_cabecalho_funcao(ASTNode *funcao) {
    ParamInfo *params;
    int nparams;

    if (funcao == NULL || funcao->type != NODE_FUNCAO) {
        return;
    }

    params = criar_lista_param_info(funcao->esq);
    nparams = contar_lista(funcao->esq);

    if (!symtab_insert_function(
            &stack,
            funcao->id_name,
            funcao->value_type,
            nparams,
            params,
            funcao->linha
        )) {
        erro_semantico("FUNCAO OU IDENTIFICADOR GLOBAL JA DECLARADO", funcao->linha);
    }
}

static void primeira_passagem_programa(ASTNode *lista) {
    int posicao_global = 1;

    while (lista != NULL) {
        if (lista->type == NODE_DECLARACAO) {
            inserir_declaracao_variavel(lista, &posicao_global);
        } else if (lista->type == NODE_FUNCAO) {
            inserir_cabecalho_funcao(lista);
        }

        lista = lista->proximo;
    }
}

static int eh_no_vetor(ASTNode *no) {
    Symbol *s;

    if (no == NULL) {
        return 0;
    }

    switch (no->type) {
        case NODE_IDENTIFICADOR:
            s = symtab_lookup(&stack, no->id_name);

            if (s == NULL) {
                erro_semantico("IDENTIFICADOR NAO DECLARADO", no->linha);
            }

            return s->eh_vetor;

        case NODE_ACESSO_VETOR:
            return 0;

        case NODE_CHAMADA_FUNCAO:
            return 0;

        default:
            return 0;
    }
}

static void verificar_argumentos_funcao(Symbol *func, ASTNode *args, int linha) {
    ParamInfo *formal = func->parametros;
    ASTNode *real = args;
    int pos = 1;

    while (formal != NULL && real != NULL) {
        ValueType tipo_real = tipo_do_no(real);
        int real_eh_vetor = eh_no_vetor(real);

        if (tipo_real != formal->tipo) {
            erro_semantico("TIPO DE ARGUMENTO DIFERENTE DO PARAMETRO FORMAL", linha);
        }

        if (real_eh_vetor != formal->eh_vetor) {
            erro_semantico("USO INCORRETO DE VETOR EM CHAMADA DE FUNCAO", linha);
        }

        formal = formal->prox;
        real = real->proximo;
        pos++;
    }

    if (formal != NULL || real != NULL) {
        erro_semantico("NUMERO DE ARGUMENTOS DIFERENTE DO NUMERO DE PARAMETROS", linha);
    }
}

static ValueType tipo_do_no(ASTNode *no) {
    ValueType t1;
    ValueType t2;
    Symbol *s;

    if (no == NULL) {
        return TIPO_INVALIDO;
    }

    switch (no->type) {
        case NODE_CONST_INT:
            no->value_type = TIPO_INT;
            return TIPO_INT;

        case NODE_CONST_CAR:
            no->value_type = TIPO_CAR;
            return TIPO_CAR;

        case NODE_CONST_STRING:
            no->value_type = TIPO_INVALIDO;
            return TIPO_INVALIDO;

        case NODE_TIPO:
            return no->value_type;

        case NODE_IDENTIFICADOR:
            s = symtab_lookup(&stack, no->id_name);

            if (s == NULL) {
                erro_semantico("IDENTIFICADOR NAO DECLARADO", no->linha);
            }

            if (s->categoria == SYM_FUNCAO) {
                erro_semantico("NOME DE FUNCAO USADO COMO VARIAVEL", no->linha);
            }

            no->value_type = s->tipo;
            no->eh_vetor = s->eh_vetor;
            return s->tipo;

        case NODE_ACESSO_VETOR:
            s = symtab_lookup(&stack, no->id_name);

            if (s == NULL) {
                erro_semantico("VETOR NAO DECLARADO", no->linha);
            }

            if (s->categoria == SYM_FUNCAO) {
                erro_semantico("FUNCAO USADA COMO VETOR", no->linha);
            }

            if (!s->eh_vetor) {
                erro_semantico("IDENTIFICADOR NAO E VETOR", no->linha);
            }

            t1 = tipo_do_no(no->esq);

            if (t1 != TIPO_INT || eh_no_vetor(no->esq)) {
                erro_semantico("INDICE DE VETOR DEVE SER INT", no->linha);
            }

            no->value_type = s->tipo;
            no->eh_vetor = 0;
            return s->tipo;

        case NODE_CHAMADA_FUNCAO:
            s = symtab_lookup(&stack, no->id_name);

            if (s == NULL) {
                erro_semantico("FUNCAO NAO DECLARADA", no->linha);
            }

            if (s->categoria != SYM_FUNCAO) {
                erro_semantico("IDENTIFICADOR CHAMADO NAO E FUNCAO", no->linha);
            }

            verificar_argumentos_funcao(s, no->esq, no->linha);

            no->value_type = s->tipo;
            return s->tipo;

        case NODE_OPERACAO:
            if (no->op == '!') {
                t1 = tipo_do_no(no->dir);

                if (t1 != TIPO_INT || eh_no_vetor(no->dir)) {
                    erro_semantico("OPERACAO LOGICA INVALIDA", no->linha);
                }

                no->value_type = TIPO_INT;
                return TIPO_INT;
            }

            if (no->op == '-' && no->esq == NULL) {
                t1 = tipo_do_no(no->dir);

                if (t1 != TIPO_INT || eh_no_vetor(no->dir)) {
                    erro_semantico("OPERACAO ARITMETICA INVALIDA", no->linha);
                }

                no->value_type = TIPO_INT;
                return TIPO_INT;
            }

            t1 = tipo_do_no(no->esq);
            t2 = tipo_do_no(no->dir);

            switch (no->op) {
                case '+':
                case '-':
                case '*':
                case '/':
                    if (t1 != TIPO_INT || t2 != TIPO_INT) {
                        erro_semantico("OPERACAO ARITMETICA EXIGE INT", no->linha);
                    }

                    if (eh_no_vetor(no->esq) || eh_no_vetor(no->dir)) {
                        erro_semantico("OPERACAO ARITMETICA NAO ACEITA VETOR", no->linha);
                    }

                    no->value_type = TIPO_INT;
                    return TIPO_INT;

                case '<':
                case '>':
                case MAIORIGUAL:
                case MENORIGUAL:
                case IGUAL:
                case DIFERENTE:
                    if (t1 != t2) {
                        erro_semantico("OPERACAO RELACIONAL ENTRE TIPOS DIFERENTES", no->linha);
                    }

                    if (eh_no_vetor(no->esq) || eh_no_vetor(no->dir)) {
                        erro_semantico("OPERACAO RELACIONAL NAO ACEITA VETOR", no->linha);
                    }

                    no->value_type = TIPO_INT;
                    return TIPO_INT;

                case OU:
                case E:
                    if (t1 != TIPO_INT || t2 != TIPO_INT) {
                        erro_semantico("OPERACAO LOGICA EXIGE INT", no->linha);
                    }

                    if (eh_no_vetor(no->esq) || eh_no_vetor(no->dir)) {
                        erro_semantico("OPERACAO LOGICA NAO ACEITA VETOR", no->linha);
                    }

                    no->value_type = TIPO_INT;
                    return TIPO_INT;

                default:
                    return TIPO_INVALIDO;
            }

        case NODE_ATRIBUICAO:
            t1 = tipo_do_no(no->esq);
            t2 = tipo_do_no(no->dir);

            if (t1 != t2) {
                erro_semantico("ATRIBUICAO ENTRE TIPOS DIFERENTES", no->linha);
            }

            if (eh_no_vetor(no->esq) != eh_no_vetor(no->dir)) {
                erro_semantico("ATRIBUICAO INVALIDA ENVOLVENDO VETOR", no->linha);
            }

            no->value_type = t1;
            return t1;

        default:
            return TIPO_INVALIDO;
    }
}

static void analisar_no(ASTNode *no);

static void analisar_bloco(ASTNode *bloco) {
    int criou_escopo = 0;
    int posicao = 1;

    if (bloco == NULL || bloco->type != NODE_BLOCO) {
        return;
    }

    if (bloco->esq != NULL) {
        symtab_push_scope(&stack);
        criou_escopo = 1;

        inserir_lista_declaracoes(bloco->esq, &posicao);
    }

    analisar_no(bloco->dir);

    if (criou_escopo) {
        symtab_pop_scope(&stack);
    }
}

static void analisar_bloco_funcao(ASTNode *funcao) {
    int posicao = 1;
    ValueType retorno_anterior;
    int dentro_anterior;

    if (funcao == NULL || funcao->type != NODE_FUNCAO) {
        return;
    }

    retorno_anterior = tipo_retorno_funcao_atual;
    dentro_anterior = dentro_de_funcao;

    tipo_retorno_funcao_atual = funcao->value_type;
    dentro_de_funcao = 1;

    symtab_push_scope(&stack);

    inserir_parametros_no_escopo(funcao->esq, &posicao);

    if (funcao->terceiro != NULL && funcao->terceiro->type == NODE_BLOCO) {
        inserir_lista_declaracoes(funcao->terceiro->esq, &posicao);
        analisar_no(funcao->terceiro->dir);
    }

    symtab_pop_scope(&stack);

    tipo_retorno_funcao_atual = retorno_anterior;
    dentro_de_funcao = dentro_anterior;
}

static void analisar_retorne(ASTNode *no) {
    ValueType tipo_expr;

    if (!dentro_de_funcao) {
        erro_semantico("COMANDO RETORNE FORA DE FUNCAO", no->linha);
    }

    tipo_expr = tipo_do_no(no->esq);

    if (tipo_expr != tipo_retorno_funcao_atual) {
        erro_semantico("TIPO RETORNADO DIFERENTE DO TIPO DA FUNCAO", no->linha);
    }

    if (eh_no_vetor(no->esq)) {
        erro_semantico("FUNCAO NAO PODE RETORNAR VETOR", no->linha);
    }
}

static void analisar_no(ASTNode *no) {
    while (no != NULL) {
        switch (no->type) {
            case NODE_PROGRAMA:
                primeira_passagem_programa(no->esq);
                analisar_no(no->esq);
                break;

            case NODE_DECLARACAO:
            case NODE_PARAMETRO:
            case NODE_TIPO:
            case NODE_CONST_INT:
            case NODE_CONST_CAR:
            case NODE_CONST_STRING:
            case NODE_NOVALINHA:
                break;

            case NODE_FUNCAO:
                analisar_bloco_funcao(no);
                break;

            case NODE_BLOCO:
                analisar_bloco(no);
                break;

            case NODE_ATRIBUICAO:
            case NODE_IDENTIFICADOR:
            case NODE_ACESSO_VETOR:
            case NODE_CHAMADA_FUNCAO:
            case NODE_OPERACAO:
                tipo_do_no(no);
                break;

            case NODE_LEIA:
                tipo_do_no(no->esq);

                if (eh_no_vetor(no->esq)) {
                    erro_semantico("COMANDO LEIA NAO ACEITA VETOR INTEIRO", no->linha);
                }

                break;

            case NODE_ESCREVA:
                if (no->esq != NULL && no->esq->type != NODE_CONST_STRING) {
                    tipo_do_no(no->esq);

                    if (eh_no_vetor(no->esq)) {
                        erro_semantico("COMANDO ESCREVA NAO ACEITA VETOR INTEIRO", no->linha);
                    }
                }

                break;

            case NODE_IF:
                if (tipo_do_no(no->esq) != TIPO_INT || eh_no_vetor(no->esq)) {
                    erro_semantico("CONDICAO DO SE DEVE SER INT", no->linha);
                }

                analisar_no(no->dir);

                if (no->terceiro != NULL) {
                    analisar_no(no->terceiro);
                }

                break;

            case NODE_WHILE:
                if (tipo_do_no(no->esq) != TIPO_INT || eh_no_vetor(no->esq)) {
                    erro_semantico("CONDICAO DO ENQUANTO DEVE SER INT", no->linha);
                }

                analisar_no(no->dir);
                break;

            case NODE_RETORNE:
                analisar_retorne(no);
                break;

            default:
                break;
        }

        no = no->proximo;
    }
}

void analisar_semantica(ASTNode *raiz) {
    symtab_init(&stack);
    symtab_push_scope(&stack);

    analisar_no(raiz);

    symtab_free(&stack);
}
