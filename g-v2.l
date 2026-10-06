%{
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "g-v2.tab.h"
%}

%option noyywrap
%option yylineno

DIGITO      [0-9]
LETRA       [a-zA-Z]
ID          ({LETRA}|_)({LETRA}|{DIGITO}|_)*
INTNUM      {DIGITO}+
CARACTERE   \'(\\[ntr'\\]|[^\\'\n])\'
CADEIA      \"([^\"\n]*)\"

%%

[ \t\r]+                ;

\n                      ;

"global"		{ return GLOBAL; }
"funcao"		{ return FUNCAO; }
"principal"             { return PRINCIPAL; }

"int"                   { return INT; }
"car"                   { return CAR; }

"retorne"		{ return RETORNE; }
"leia"                  { return LEIA; }
"escreva"               { return ESCREVA; }
"novalinha"             { return NOVALINHA; }

"se"                    { return SE; }
"entao"                 { return ENTAO; }
"senao"                 { return SENAO; }
"fimse"                 { return FIMSE; }
"enquanto"              { return ENQUANTO; }

"||"                    { return OU; }
"&"                     { return E; }
"=="                    { return IGUAL; }
"!="                    { return DIFERENTE; }
">="                    { return MAIORIGUAL; }
"<="                    { return MENORIGUAL; }

{ID}                    {
                            yylval.sIndex = strdup(yytext);
                            return IDENTIFICADOR;
                        }

{INTNUM}                {
                            yylval.iValue = atoi(yytext);
                            return INTCONST;
                        }

{CARACTERE}             {
                            yylval.sIndex = strdup(yytext);
                            return CARCONST;
                        }

{CADEIA}                {
                            yylval.sIndex = strdup(yytext);
                            return CADEIACARACTERES;
                        }

\/\*([^*]|\*+[^*/])*\*+\/    ;

"/*"                    {
                            printf("ERRO LEXICO: COMENTARIO NAO TERMINA %d\n", yylineno);
                            exit(1);
                        }

\"([^\"\n]*)\n          {
                            printf("ERRO LEXICO: CADEIA DE CARACTERES OCUPA MAIS DE UMA LINHA %d\n", yylineno);
                            exit(1);
                        }

\"([^\"\n]*)            {
                            printf("ERRO LEXICO: CADEIA DE CARACTERES OCUPA MAIS DE UMA LINHA %d\n", yylineno);
                            exit(1);
                        }

"{"                     { return '{'; }
"}"                     { return '}'; }

"("                     { return '('; }
")"                     { return ')'; }

"["                     { return '['; }
"]"                     { return ']'; }

","                     { return ','; }
":"                     { return ':'; }
";"                     { return ';'; }

"="                     { return '='; }
"<"                     { return '<'; }
">"                     { return '>'; }

"+"                     { return '+'; }
"-"                     { return '-'; }
"*"                     { return '*'; }
"/"                     { return '/'; }

"!"                     { return '!'; }

.                       {
                            printf("ERRO: CARACTERE INVALIDO %d\n", yylineno);
                            exit(1);
                        }

<<EOF>>                 { return -1; }

%%
