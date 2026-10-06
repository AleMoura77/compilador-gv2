%{
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "semantic.h"
#include "codagen.h"

extern int yylex();
extern int yylineno;
extern char *yytext;
extern FILE *yyin;

void yyerror(const char *s);

ASTNode *raiz = NULL;

static ASTNode* criar_constante_car_de_lexema(char *lex, int linha) {
    char valor;

    if (lex[1] == '\\') {
        switch (lex[2]) {
            case 'n': valor = '\n'; break;
            case 't': valor = '\t'; break;
            case '\\': valor = '\\'; break;
            case '\'': valor = '\''; break;
            default: valor = lex[2]; break;
        }
    } else {
        valor = lex[1];
    }

    return criar_no_car(valor, linha);
}

static void aplicar_tipo(ASTNode *lista, ASTNode *tipo) {
    ASTNode *p = lista;

    while (p != NULL) {
        p->esq = criar_no_tipo(tipo->value_type, p->linha);
        p->value_type = tipo->value_type;
        p = p->proximo;
    }
}
%}

%union {
    int iValue;
    char *sIndex;
    struct ast_node *nPtr;
}

%token GLOBAL FUNCAO PRINCIPAL
%token INT CAR
%token RETORNE LEIA ESCREVA NOVALINHA
%token SE ENTAO SENAO FIMSE ENQUANTO
%token OU E IGUAL DIFERENTE MAIORIGUAL MENORIGUAL

%token <sIndex> IDENTIFICADOR
%token <iValue> INTCONST
%token <sIndex> CARCONST
%token <sIndex> CADEIACARACTERES

%type <nPtr> Programa DeclVarGlobais DeclFunc ListaFuncoes DeclPrograma
%type <nPtr> Bloco VarSection ListaDeclVar ListVar Tipo
%type <nPtr> ListaParametros ListaParametrosTail
%type <nPtr> ListaComando Comando Expr LValueExpr
%type <nPtr> OrExpr AndExpr EqExpr DesigExpr AddExpr MulExpr UnExpr PrimExpr
%type <nPtr> ListExpr

%start Programa

%%

Programa
    : DeclVarGlobais DeclFunc DeclPrograma
      {
          ASTNode *lista = NULL;

          lista = anexar_no(lista, $1);
          lista = anexar_no(lista, $2);
          lista = anexar_no(lista, $3);

          raiz = criar_no_programa(lista, yylineno);
          $$ = raiz;
      }
    ;

DeclVarGlobais
    : GLOBAL VarSection
      {
          $$ = $2;
      }
    | %empty
      {
          $$ = NULL;
      }
    ;

DeclFunc
    : FUNCAO '[' IDENTIFICADOR '(' ListaParametros ')' ':' Tipo Bloco ListaFuncoes ']'
      {
          ASTNode *func = criar_no_funcao($3, $5, $8, $9, yylineno);
          $$ = anexar_no(func, $10);
      }
    | %empty
      {
          $$ = NULL;
      }
    ;

ListaFuncoes
    : IDENTIFICADOR '(' ListaParametros ')' ':' Tipo Bloco ListaFuncoes
      {
          ASTNode *func = criar_no_funcao($1, $3, $6, $7, yylineno);
          $$ = anexar_no(func, $8);
      }
    | %empty
      {
          $$ = NULL;
      }
    ;

ListaParametros
    : ListaParametrosTail
      {
          $$ = $1;
      }
    | %empty
      {
          $$ = NULL;
      }
    ;

ListaParametrosTail
    : IDENTIFICADOR ':' Tipo
      {
          $$ = criar_no_parametro($1, $3, 0, yylineno);
      }
    | IDENTIFICADOR '[' ']' ':' Tipo
      {
          $$ = criar_no_parametro($1, $5, 1, yylineno);
      }
    | IDENTIFICADOR ':' Tipo ',' ListaParametrosTail
      {
          ASTNode *param = criar_no_parametro($1, $3, 0, yylineno);
          $$ = anexar_no(param, $5);
      }
    | IDENTIFICADOR '[' ']' ':' Tipo ',' ListaParametrosTail
      {
          ASTNode *param = criar_no_parametro($1, $5, 1, yylineno);
          $$ = anexar_no(param, $7);
      }
    ;

DeclPrograma
    : PRINCIPAL Bloco
      {
          $$ = $2;
      }
    ;

Bloco
    : '{' ListaComando '}'
      {
          $$ = criar_no_bloco(NULL, $2, yylineno);
      }
    | VarSection '{' ListaComando '}'
      {
          $$ = criar_no_bloco($1, $3, yylineno);
      }
    ;

VarSection
    : '[' ListaDeclVar ']'
      {
          $$ = $2;
      }
    ;

ListaDeclVar
    : ListVar ':' Tipo ';'
      {
          aplicar_tipo($1, $3);
          $$ = $1;
      }
    | ListVar ':' Tipo ';' ListaDeclVar
      {
          aplicar_tipo($1, $3);
          $$ = anexar_no($1, $5);
      }
    ;

ListVar
    : IDENTIFICADOR
      {
          $$ = criar_no_declaracao($1, NULL, yylineno);
      }
    | IDENTIFICADOR '[' INTCONST ']'
      {
          $$ = criar_no_declaracao_vetor($1, $3, NULL, yylineno);
      }
    | IDENTIFICADOR ',' ListVar
      {
          ASTNode *v = criar_no_declaracao($1, NULL, yylineno);
          $$ = anexar_no(v, $3);
      }
    | IDENTIFICADOR '[' INTCONST ']' ',' ListVar
      {
          ASTNode *v = criar_no_declaracao_vetor($1, $3, NULL, yylineno);
          $$ = anexar_no(v, $6);
      }
    ;

Tipo
    : INT
      {
          $$ = criar_no_tipo(TIPO_INT, yylineno);
      }
    | CAR
      {
          $$ = criar_no_tipo(TIPO_CAR, yylineno);
      }
    ;

ListaComando
    : Comando
      {
          $$ = $1;
      }
    | Comando ListaComando
      {
          if ($1 == NULL) {
              $$ = $2;
          } else {
              $$ = anexar_no($1, $2);
          }
      }
    ;

Comando
    : ';'
      {
          $$ = NULL;
      }
    | Expr ';'
      {
          $$ = $1;
      }
    | RETORNE Expr ';'
      {
          $$ = criar_no_retorne($2, yylineno);
      }
    | LEIA LValueExpr ';'
      {
          $$ = criar_no_leia($2->id_name, yylineno);
          $$->esq = $2;
      }
    | ESCREVA Expr ';'
      {
          $$ = criar_no_escreva($2, yylineno);
      }
    | ESCREVA CADEIACARACTERES ';'
      {
          $$ = criar_no_escreva(criar_no_string($2, yylineno), yylineno);
      }
    | NOVALINHA ';'
      {
          $$ = criar_no_novalinha(yylineno);
      }
    | SE '(' Expr ')' ENTAO Comando FIMSE
      {
          $$ = criar_no_if($3, $6, NULL, yylineno);
      }
    | SE '(' Expr ')' ENTAO Comando SENAO Comando FIMSE
      {
          $$ = criar_no_if($3, $6, $8, yylineno);
      }
    | ENQUANTO '(' Expr ')' Comando
      {
          $$ = criar_no_while($3, $5, yylineno);
      }
    | Bloco
      {
          $$ = $1;
      }
    ;

Expr
    : LValueExpr '=' Expr
      {
          $$ = criar_no_atribuicao($1, $3, yylineno);
      }
    | OrExpr
      {
          $$ = $1;
      }
    ;

LValueExpr
    : IDENTIFICADOR
      {
          $$ = criar_no_id($1, yylineno);
      }
    | IDENTIFICADOR '[' Expr ']'
      {
          $$ = criar_no_acesso_vetor($1, $3, yylineno);
      }
    ;

OrExpr
    : OrExpr OU AndExpr
      {
          $$ = criar_no_op(OU, $1, $3, yylineno);
      }
    | AndExpr
      {
          $$ = $1;
      }
    ;

AndExpr
    : AndExpr E EqExpr
      {
          $$ = criar_no_op(E, $1, $3, yylineno);
      }
    | EqExpr
      {
          $$ = $1;
      }
    ;

EqExpr
    : EqExpr IGUAL DesigExpr
      {
          $$ = criar_no_op(IGUAL, $1, $3, yylineno);
      }
    | EqExpr DIFERENTE DesigExpr
      {
          $$ = criar_no_op(DIFERENTE, $1, $3, yylineno);
      }
    | DesigExpr
      {
          $$ = $1;
      }
    ;

DesigExpr
    : DesigExpr '<' AddExpr
      {
          $$ = criar_no_op('<', $1, $3, yylineno);
      }
    | DesigExpr '>' AddExpr
      {
          $$ = criar_no_op('>', $1, $3, yylineno);
      }
    | DesigExpr MAIORIGUAL AddExpr
      {
          $$ = criar_no_op(MAIORIGUAL, $1, $3, yylineno);
      }
    | DesigExpr MENORIGUAL AddExpr
      {
          $$ = criar_no_op(MENORIGUAL, $1, $3, yylineno);
      }
    | AddExpr
      {
          $$ = $1;
      }
    ;

AddExpr
    : AddExpr '+' MulExpr
      {
          $$ = criar_no_op('+', $1, $3, yylineno);
      }
    | AddExpr '-' MulExpr
      {
          $$ = criar_no_op('-', $1, $3, yylineno);
      }
    | MulExpr
      {
          $$ = $1;
      }
    ;

MulExpr
    : MulExpr '*' UnExpr
      {
          $$ = criar_no_op('*', $1, $3, yylineno);
      }
    | MulExpr '/' UnExpr
      {
          $$ = criar_no_op('/', $1, $3, yylineno);
      }
    | UnExpr
      {
          $$ = $1;
      }
    ;

UnExpr
    : '-' PrimExpr
      {
          $$ = criar_no_op('-', NULL, $2, yylineno);
      }
    | '!' PrimExpr
      {
          $$ = criar_no_op('!', NULL, $2, yylineno);
      }
    | PrimExpr
      {
          $$ = $1;
      }
    ;

PrimExpr
    : IDENTIFICADOR '(' ListExpr ')'
      {
          $$ = criar_no_chamada_funcao($1, $3, yylineno);
      }
    | IDENTIFICADOR '(' ')'
      {
          $$ = criar_no_chamada_funcao($1, NULL, yylineno);
      }
    | IDENTIFICADOR '[' Expr ']'
      {
          $$ = criar_no_acesso_vetor($1, $3, yylineno);
      }
    | IDENTIFICADOR
      {
          $$ = criar_no_id($1, yylineno);
      }
    | CARCONST
      {
          $$ = criar_constante_car_de_lexema($1, yylineno);
      }
    | INTCONST
      {
          $$ = criar_no_int($1, yylineno);
      }
    | '(' Expr ')'
      {
          $$ = $2;
      }
    ;

ListExpr
    : Expr
      {
          $$ = $1;
      }
    | ListExpr ',' Expr
      {
          $$ = anexar_no($1, $3);
      }
    ;

%%

void yyerror(const char *s) {
    (void)s;
    printf("ERRO SINTATICO: linha %d proximo a '%s'\n", yylineno, yytext);
}

int main(int argc, char **argv) {
    int parse_result;
    char arquivo_saida[1024];

    if (argc < 2) {
        fprintf(stderr, "Uso: %s arquivo.g\n", argv[0]);
        return 1;
    }

    yyin = fopen(argv[1], "r");

    if (!yyin) {
        perror("Erro ao abrir arquivo");
        return 1;
    }

    parse_result = yyparse();

    if (parse_result == 0 && raiz != NULL) {
        analisar_semantica(raiz);
        imprimir_ast(raiz, 0);

        snprintf(arquivo_saida, sizeof(arquivo_saida), "%s.s", argv[1]);
        gerar_codigo(raiz, arquivo_saida);

        liberar_ast(raiz);
    }

    fclose(yyin);

    return parse_result;
}
