%{
//Trevor Ching
//ttching
//Scanner.l
#include "lyutils.h"
#include "astree.h"
#include "yyparse.h"
#include <stdio.h>

#define YY_USER_ACTION { lexer::advance(); }

int yylval_token(int symbol){
	yylval = new astree(symbol, lexer::lloc, yytext);
	return symbol;
}

%}

%option 8bit
%option debug
%option nodefault
%option nounput
%option noyywrap
%option verbose
%option warn


LETTER    [a-zA-Z_]
DIGIT     [0-9]*
IDENT     ({LETTER}({LETTER}|{DIGIT}|"_")*)
CHARCON   ('([^\\'\n]|\\[\\'"0nt])')
STRCON    (\"([^\\"\n]|\\[\\'"0nt])*\")

%%
"+"       {return yylval_token('+');}
"-"       {return yylval_token('-');}
"*"       {return yylval_token('*');}
"/"       {return yylval_token('/');}
"%"       {return yylval_token('%');}
"!"       {return yylval_token('!');}
"="       {return yylval_token('=');}
"=="      {return yylval_token(TOK_EQ);}
"!="      {return yylval_token(TOK_NE);}
"<"       {return yylval_token(TOK_LT);}
"<="      {return yylval_token(TOK_LE);}
">"       {return yylval_token(TOK_GT);}
">="      {return yylval_token(TOK_GE);}
","       {return yylval_token(',');}
";"       {return yylval_token(';');}
"("       {return yylval_token('(');}
")"       {return yylval_token(')');}
"["       {return yylval_token('[');}
"]"       {return yylval_token(']');}
"{"       {return yylval_token('{');}
"}"       {return yylval_token('}');}
"."	  {return yylval_token('.');}
"[]"      {return yylval_token(TOK_ARRAY);}
"if"      {return yylval_token(TOK_IF);}
"else"    {return yylval_token(TOK_ELSE);}
"while"   {return yylval_token(TOK_WHILE);}
"return"  {return yylval_token(TOK_RETURN);}
"int"     {return yylval_token(TOK_INT);}
"string"  {return yylval_token(TOK_STRING);}
"struct"  {return yylval_token(TOK_STRUCT);}
"void"    {return yylval_token(TOK_VOID);}
"new"     {return yylval_token(TOK_NEW);}
"null"    {return yylval_token(TOK_NULL);}
{IDENT}   {return yylval_token(TOK_IDENT);}
{DIGIT}   {return yylval_token(TOK_INTCON);}
{CHARCON} {return yylval_token(TOK_CHARCON);}
{STRCON}  {return yylval_token(TOK_STRINGCON);}
"#".*     { lexer::include();}
[ \t]+    { }
\n        { lexer::newline();}
.         { lexer::badchar(*yytext);}
%%
