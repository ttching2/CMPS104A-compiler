%{
// Dummy parser for scanner project.

#include <cassert>

#include "lyutils.h"
#include "astree.h"

%}

%debug
%defines
%error-verbose
%token-table
%verbose

%destructor{ destroy($$); }<>
%printer { astree::dump(yyoutput, $$); }<>
%initial-action{
	parser::root = new astree(TOK_ROOT, {0,0,0}, "<<TOK_ROOT>>");
}

%token TOK_VOID TOK_CHAR TOK_INT TOK_STRING
%token TOK_IF TOK_ELSE TOK_WHILE TOK_RETURN TOK_STRUCT
%token TOK_NULL TOK_NEW TOK_ARRAY
%token TOK_EQ TOK_NE TOK_LT TOK_LE TOK_GT TOK_GE
%token TOK_IDENT TOK_INTCON TOK_CHARCON TOK_STRINGCON

%token TOK_BLOCK TOK_CALL TOK_IFELSE TOK_INITDECL
%token TOK_POS TOK_NEG TOK_NEWARRAY TOK_TYPEID TOK_FIELD
%token TOK_ORD TOK_CHR TOK_ROOT TOK_DECLID
%token TOK_VARDECL TOK_PARAMLIST TOK_RETURNVOID
%token TOK_FUNC TOK_PROTO TOK_INDEX TOK_NEWSTRING

%right TOK_IF TOK_ELSE
%right '=' 
%left  TOK_EQ TOK_NE TOK_LT TOK_LE TOK_GT TOK_GE
%left  '+' '-'
%left  '*' '/' '%'
%right TOK_POS TOK_NEG '!' TOK_NEW
%left  '[' '.' TOK_PARAMLIST

%start start

%%

start	 : program		{ $$ = $1= nullptr; }
	 ;
program	 : program structdef	{ $$ = $1->adopt($2); }
	 | program function	{ $$ = $1->adopt($2); }
	 | program statement	{ $$ = $1->adopt($2); }
	 | program error '}'	{ destroy($3); $$ = $1; }
	 | program error ';'	{ destroy($3); $$ = $1; }
	 |			{ $$ =parser::root;}
	 ;
structdef: TOK_STRUCT TOK_IDENT '{' fielddec ';' '}'	
	{$$ = $1->adopt($2,$4);$2->symbol=TOK_TYPEID;
	destroy($3);
if($4->symbol == TOK_INT || $4->symbol == TOK_STRING || $4->symbol == TOK_IDENT){
	if($4->children[0]->symbol == TOK_NEWARRAY){
		$4->children[0]->adopt($4->children[1]);
		for(size_t i = 2; i < $4->children.size();i++){
			if($4->children[i]->symbol == TOK_NEWARRAY){
				$4->children[i]->adopt($4->children[i+1]);
				$1->adopt($4->children[i]);
				i++;
			}
			else{
				$1->adopt($4->children[i]);
			}
		}
	}
	else{
		for(size_t i = 1; i < $4->children.size();i++){
			if($4->children[i]->symbol == TOK_NEWARRAY){
				$4->children[i]->adopt($4->children[i+1]);
				$1->adopt($4->children[i]);
				i++;
			}
			else{
				$1->adopt($4->children[i]);
			}
		}
	}

$4->children.erase($4->children.begin()+1,$4->children.end());
}}
	 | TOK_STRUCT TOK_IDENT '{' '}'				
	{$$ = $1->adopt($2);$2->symbol=TOK_TYPEID;
	destroy($3);destroy($4);}
	 ;
fielddec : fielddec ';' fielddecl	{$$=$1->adopt($3);}
	 | fielddecl 			{$$=$1;}
	 ;
fielddecl: basetype TOK_IDENT			{$$ = $1->adopt($2);$2->symbol=TOK_FIELD; }
	 | basetype TOK_ARRAY TOK_IDENT		{$$ = $1->adopt($2,$3);$2->symbol=TOK_NEWARRAY;$3->symbol=TOK_FIELD;}
	 ;
basetype : TOK_VOID		{ $$ = $1;}
	 | TOK_INT		{ $$ = $1;}
	 | TOK_STRING		{ $$ = $1;}
	 | TOK_IDENT		{ $$ = $1;}
	 ;
function : identdecl '(' identdec ')' block
	{$$ = new astree(TOK_FUNC,$1->lloc,"");$$->adopt($1,$2);$$->adopt($5);$2->adopt_sym($3, TOK_PARAMLIST);destroy($4);
{if($3->symbol == TOK_INT || $3->symbol == TOK_STRING || $3->symbol == TOK_IDENT)
	for(size_t i = 1; i < $3->children.size();i++){
		if($3->children[i]->symbol == TOK_NEWARRAY){
			$3->children[i]->adopt($3->children[i+1]);
			i++;
		}
		else{
			$2->adopt($3->children[i]);
		}
	}
$3->children.erase($3->children.begin()+1,$3->children.end());
}
}
	 | identdecl '(' ')' block 
	{$$ = new astree(TOK_FUNC,$1->lloc,"");$$->adopt($1,$4);destroy($2,$3);}
	 | identdecl '(' identdec ')' ';'
	{ $$ = new astree(TOK_PROTO,$1->lloc,"");$$->adopt($1,$3);
{if($3->symbol == TOK_INT || $3->symbol == TOK_STRING || $3->symbol == TOK_IDENT)
	for(size_t i = 1; i < $3->children.size();i++){
		if($3->children[i]->symbol == TOK_NEWARRAY){
			$3->children[i]->adopt($3->children[i+1]);
			i++;
		}
		else{
			$$->adopt($3->children[i]);
		}
	}
$3->children.erase($3->children.begin()+1,$3->children.end());

}
}
	 | identdecl '(' ')' ';'
	{ $$ = new astree(TOK_PROTO,$1->lloc,"");$$->adopt($1);}
	 ;
identdec : identdecl			{$$ = $1;}
	 | identdec ',' identdecl	{$$ = $1->adopt($3); destroy($2);}
	 ;
identdecl: basetype TOK_ARRAY TOK_IDENT	{ $$ = $2->adopt($1, $3);$3->symbol=TOK_DECLID;}
	 | basetype TOK_IDENT		{ $$ = $1->adopt($2);$2->symbol=TOK_DECLID;}
	 ;
block	 :  statements '}'		{ $$ = $1;destroy($2);}
	 |  statements '}' ';'		{ $$ = $1; destroy($2,$3);}
	 ;
statements: statements statement	{ $$ = $1->adopt($2);}
	 | '{'				{$$ = $1;$1->symbol = TOK_BLOCK;}
	 ;
statement: block		{ $$ = $1;}
	 | vardecl		{ $$ = $1;}
	 | while		{ $$ = $1;}
	 | ifelse		{ $$ = $1;}
	 | return		{ $$ = $1;}
	 | expr ';'		{ $$ = $1; destroy($2);}
	 ;
vardecl  : identdecl '=' expr ';'{$$ = $2->adopt_sym($1,TOK_VARDECL);$2->adopt($3);destroy($4);}
	 ;
while	 : TOK_WHILE '(' expr ')' statement	{$$ = $1->adopt($3, $5);destroy($2);destroy($4);}
	 ;
ifelse	 : TOK_IF '(' expr ')' statement TOK_ELSE statement	
	{$$= $1->adopt_sym($3, TOK_IFELSE); $$ = $1->adopt($5,$7);destroy($2);destroy($4);destroy($6);}
	 | TOK_IF '(' expr ')' statement %prec TOK_IF		
	{$$= $1->adopt($3, $5);destroy($2);destroy($4);}
	 ;
return	 : TOK_RETURN expr ';'	{ $$ = $1->adopt($2);destroy($3);}
	 | TOK_RETURN ';'	{ $$ = $1;$1->symbol= TOK_RETURNVOID;destroy($2);}
	 ;
expr	 : expr '+' expr		{ $$ = $2->adopt($1, $3); }
	 | expr '-' expr		{ $$ = $2->adopt($1, $3); }
	 | expr '*' expr		{ $$ = $2->adopt($1, $3); }
	 | expr '/' expr		{ $$ = $2->adopt($1, $3); }
	 | expr '%' expr		{ $$ = $2->adopt($1, $3); }
	 | expr '=' expr		{ $$ = $2->adopt($1, $3); }
	 | expr TOK_EQ expr		{ $$ = $2->adopt($1, $3); }
	 | expr TOK_NE expr		{ $$ = $2->adopt($1, $3); }
	 | expr TOK_LT expr		{ $$ = $2->adopt($1, $3); }
	 | expr TOK_LE expr		{ $$ = $2->adopt($1, $3); }
	 | expr TOK_GT expr		{ $$ = $2->adopt($1, $3); }
	 | expr TOK_GE expr		{ $$ = $2->adopt($1, $3); }
	 | '!' expr			{ $$ = $1->adopt($2); }
	 | '-' expr %prec TOK_NEG	{ $$ = $1->adopt_sym($2, TOK_NEG); }
	 | '+' expr %prec TOK_POS	{ $$ = $1->adopt_sym($2, TOK_POS); }
	 | allocator			{ $$ = $1;}
	 | call				{ $$ = $1;}
	 | '(' expr ')'			{ $$ = $2;destroy($1);destroy($3);}
	 | variable			{ $$ = $1;}
	 | constant			{ $$ = $1;}
	 ;
allocator: TOK_NEW TOK_IDENT '(' ')'
	{ $$ = $1->adopt($2);$2->symbol=TOK_TYPEID;destroy($3);destroy($4);}
	 | TOK_NEW TOK_STRING '(' expr ')'	
	{ $$ = $2->adopt_sym($4,TOK_NEWSTRING);destroy($1);destroy($3);destroy($5); }
	 | TOK_NEW basetype '[' expr ']'	
	{ $$ = $1->adopt_sym($2, TOK_NEWARRAY);$1->adopt($4); destroy($3);destroy($5);}
	 ;
call	 : TOK_IDENT '(' exprs ')'	{ $$ = $2->adopt_sym($1,TOK_CALL);$2->adopt($3);destroy($4);
if($3->symbol == TOK_INT || $3->symbol == TOK_STRING || $3->symbol == TOK_IDENT
|| $3->symbol == TOK_STRINGCON || $3->symbol == TOK_INTCON || $3->symbol == TOK_CHARCON
|| $3->symbol == TOK_NULL){
	for(size_t i = 0; i < $3->children.size();i++){
		$2->adopt($3->children[i]);
	}

	$3->children.clear();
}

}
	 | TOK_IDENT '(' ')'		{ $$ = $2->adopt_sym($1,TOK_CALL); destroy($3);}
	 ;
exprs    : expr			{ $$ = $1;}
	 | exprs ',' expr	{ $$ = $1->adopt($3);}
variable : TOK_IDENT		{ $$ = $1;}
	 | expr '[' expr ']'	{ $$ = $2->adopt_sym($1,TOK_INDEX);$2->adopt($3);destroy($4);}
	 | expr '.' TOK_IDENT	{ $$ = $2->adopt($1,$3);$3->symbol=TOK_FIELD;}
	 ;
constant : TOK_INTCON		{ $$ = $1;}
	 | TOK_CHARCON		{ $$ = $1;}
	 | TOK_STRINGCON	{ $$ = $1;}
	 | TOK_NULL		{ $$ = $1;}
	 ;
%%

const char *parser::get_tname (int symbol) {
   return yytname [YYTRANSLATE (symbol)];
}


bool is_defined_token (int symbol) {
   return YYTRANSLATE (symbol) > YYUNDEFTOK;
}

/*
static void* yycalloc (size_t size) {
   void* result = calloc (1, size);
   assert (result != nullptr);
   return result;
}
*/

