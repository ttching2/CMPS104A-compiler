// $Id: astree.cpp,v 1.8 2016-09-21 17:13:03-07 - - $

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include "astree.h"
#include "string_set.h"
#include "lyutils.h"

using namespace std;
using symbol_table = unordered_map<const string*,symbol*>;
using symbol_entry = symbol_table::value_type;
vector<int> block_stack;
symbol_table global_table;
symbol_table struct_table;
astree* current_function;
int block_nr = 1;
int string_num = 0;
int vregcounter = 0;
symbol_stack stack;


astree::astree (int symbol_, const location& lloc_, const char* info) {
   symbol = symbol_;
   lloc = lloc_;
   lexinfo = string_set::intern (info);
   fprintf(tok_file, "%2zd %-zd.%-5zd %-5d %-15s (%-s)\n",
                 lloc.filenr,
                 lloc.linenr,
                 lloc.offset,
                 symbol,
                 parser::get_tname(symbol),
                 lexinfo->c_str());
   //tokens.push_back(*this);
   // vector defaults to empty -- no children
}

astree::~astree() {
   while (not children.empty()) {
      astree* child = children.back();
      children.pop_back();
      delete child;
   }
   if (yydebug) {
      fprintf (stderr, "Deleting astree (");
      astree::dump (stderr, this);
      fprintf (stderr, ")\n");
   }
}

astree* astree::adopt (astree* child1, astree* child2) {
   if (child1 != nullptr) children.push_back (child1);
   if (child2 != nullptr) children.push_back (child2);
   return this;
}

astree* astree::adopt_sym (astree* child, int symbol_) {
   symbol = symbol_;
   return adopt (child);
}


void astree::dump_node (FILE* outfile) {
   fprintf (outfile, "%p->{%s %zd.%zd.%zd \"%s\":",
            this, parser::get_tname (symbol),
            lloc.filenr, lloc.linenr, lloc.offset,
            lexinfo->c_str());
   for (size_t child = 0; child < children.size(); ++child) {
      fprintf (outfile, " %p", children.at(child));
   }
}

void astree::dump_tree (FILE* outfile, int depth) {
   fprintf (outfile, "%*s", depth * 3, "");
   dump_node (outfile);
   fprintf (outfile, "\n");
   for (astree* child: children) child->dump_tree (outfile, depth + 1);
   fflush (NULL);
}

void astree::dump (FILE* outfile, astree* tree) {
   if (tree == nullptr) fprintf (outfile, "nullptr");
                   else tree->dump_node (outfile);
}

void astree::print (FILE* outfile, astree* tree, int depth) {
   for(int i = 0; i < depth; i++){
   	fprintf (outfile, "| %*s", 2 , "");
   }
   fprintf (outfile, "%s \"%s\" (%zd.%zd.%zd) {%lu}",
            parser::get_tname (tree->symbol), tree->lexinfo->c_str(),
            tree->lloc.filenr, tree->lloc.linenr, tree->lloc.offset,
			tree->block_nr);
   if(tree->attributes[ATTR_int])
	   fprintf(outfile," int ");
   else if(tree->attributes[ATTR_void])
	   fprintf(outfile," void ");
   else if(tree->attributes[ATTR_null])
	   fprintf(outfile," null ");
   else if(tree->attributes[ATTR_string])
	   fprintf(outfile," string ");
   else if(tree->attributes[ATTR_struct])
	   fprintf(outfile," struct ");
   
   if(tree->attributes[ATTR_array])
	   fprintf(outfile," array ");
   
   if(tree->attributes[ATTR_function])
	   fprintf(outfile," function ");
   else if(tree->attributes[ATTR_variable])
	   fprintf(outfile," variable ");
   else if(tree->attributes[ATTR_field])
	   fprintf(outfile," field ");
   else if(tree->attributes[ATTR_typeid])
	   fprintf(outfile," struct ");
   
   if(tree->attributes[ATTR_lval])
	   fprintf(outfile," lval ");
   else if(tree->attributes[ATTR_const])
	   fprintf(outfile," const ");
   
   if(tree->attributes[ATTR_vreg])
	   fprintf(outfile," vreg ");
   else if(tree->attributes[ATTR_vaddr])
	   fprintf(outfile," vaddr ");
   
   if(tree->symbol == TOK_IDENT){
	   fprintf(outfile,"(%zd.%zd.%zd)",
	   tree->ref_loc.filenr,
	   tree->ref_loc.linenr,
	   tree->ref_loc.offset);
   }
   fprintf(outfile,"\n");
   for (astree* child: tree->children) {
      astree::print (outfile, child, depth + 1);
   }
}

void astree::setFile(string name){
	tok_file = fopen(name.c_str(), "w");
}

void astree::closeFile(){
	pclose(tok_file);
}

void destroy (astree* tree1, astree* tree2) {
   if (tree1 != nullptr) delete tree1;
   if (tree2 != nullptr) delete tree2;
}

void errllocprintf (const location& lloc, const char* format,
                    const char* arg) {
   static char buffer[0x1000];
   assert (sizeof buffer > strlen (format) + strlen (arg));
   snprintf (buffer, sizeof buffer, format, arg);
   errprintf ("%s:%zd.%zd: %s", 
              lexer::filename (lloc.filenr), lloc.linenr, lloc.offset,
              buffer);
}

string make_oil_field(bitset<ATTR_bitset_size> bits, const string* type){
	if(bits[ATTR_array]){
		if(bits[ATTR_int])
			return "int*";
		else if(bits[ATTR_string])
			return "char**";
		else if(bits[ATTR_typeid])
			return string("struct s_")+type->c_str()+string("**");
	}
	else{
		if(bits[ATTR_int])
			return "int";
		else if(bits[ATTR_string])
			return "char*";
		else if(bits[ATTR_typeid])
			return string("struct s_")+type->c_str()+string("*");
	}
	return "Not supposed to be here?";
}

void print_string_cons(astree* node){
	for(astree* child: node->children){
		print_string_cons(child);
	}
	if(node->symbol == TOK_STRINGCON){
		fprintf(oil_file,"%s\n",node->string_con.c_str());
	}
}

string vreg (astree* node) { // e . g . i23, a53, p69
  string typechar;
  if (node->attributes[ATTR_int]) { typechar = "i"; }
  else if(node->attributes[ATTR_string]){ typechar = "p"; }
  node->string_con = typechar + to_string(++vregcounter);
  return typechar + to_string(++vregcounter);
}

string func_codegen(astree* node){
	switch(node->symbol){
		case TOK_BLOCK :{
			for (astree* child : node->children) func_codegen(child);
			break;
		}
		case TOK_RETURN:{
			string target = func_codegen(node->children[0]);
			fprintf(oil_file,"return %s;\n", target.c_str());
			break;
		}
		case '+':{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s + %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}
		case '-':{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s - %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}
		case '*':{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s * %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}
		case '/':{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s / %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}
		case TOK_LE:{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s <= %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}
		case TOK_LT:{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s < %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}
		case TOK_GE:{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s >= %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}
		case TOK_GT:{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s > %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}
		case TOK_NE:{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s != %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}/*
		case '!':{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s != %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}*/
		case TOK_EQ:{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s %s = %s == %s;\n",
								make_oil_field(node->attributes,node->lexinfo).c_str(),
								target.c_str(),
								leftv.c_str(),
								rightv.c_str());
			return target;
		}
		case TOK_WHILE:{
			fprintf(oil_file,"while_%lu_%lu_%lu:;\n",node->lloc.filenr,node->lloc.linenr,node->lloc.offset);
			string target = func_codegen(node->children[0]);
			fprintf(oil_file,"if(!%s) goto break_%lu_%lu_%lu;\n",
				target.c_str(),
				node->lloc.filenr,
				node->lloc.linenr,
				node->lloc.offset);
			func_codegen(node->children[1]);
			fprintf(oil_file,"goto while_%lu_%lu_%lu;\n",
				node->lloc.filenr,
				node->lloc.linenr,
				node->lloc.offset);
			fprintf(oil_file,"break_%lu_%lu_%lu:;\n",
				node->lloc.filenr,
				node->lloc.linenr,
				node->lloc.offset);
			break;
		}
		case TOK_IF:{
			string target = func_codegen(node->children[0]);
			fprintf(oil_file,"if(!%s) goto fi_%lu_%lu_%lu;\n",
				target.c_str(),
				node->lloc.filenr,
				node->lloc.linenr,
				node->lloc.offset);
			func_codegen(node->children[1]);	
			fprintf(oil_file,"fi_%lu_%lu_%lu:;\n",
				node->lloc.filenr,
				node->lloc.linenr,
				node->lloc.offset);
			break;
		}
		case TOK_IFELSE:{
			string target = func_codegen(node->children[0]);
			fprintf(oil_file,"if(!%s) goto else_%lu_%lu_%lu;\n",
				target.c_str(),
				node->lloc.filenr,
				node->lloc.linenr,
				node->lloc.offset);
			func_codegen(node->children[1]);
			fprintf(oil_file,"goto fi_%lu_%lu_%lu;\n",
				node->lloc.filenr,
				node->lloc.linenr,
				node->lloc.offset);
			fprintf(oil_file,"else_%lu_%lu_%lu;\n",
				node->lloc.filenr,
				node->lloc.linenr,
				node->lloc.offset);
			func_codegen(node->children[2]);
			fprintf(oil_file,"fi_%lu_%lu_%lu;\n",
				node->lloc.filenr,
				node->lloc.linenr,
				node->lloc.offset);
			break;
		}
		case TOK_INTCON : {// no vreg , return constant itself
			return *(node->lexinfo);
		}
		case TOK_NULL:{
			return string("0");
		}
		case TOK_CALL:{
			fprintf(oil_file,"        __%s(",node->children[0]->lexinfo->c_str());
			string target = "";
			for(size_t i = 1; i < node->children.size(); i++){
				string target = func_codegen(node->children[i]);
			}
			fprintf(oil_file,"%s);\n",target.c_str());
			break;
		}
		case '=':{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s = %s;\n",
								target.c_str(),
								leftv.c_str());
			return target;
		}
		case '.':{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s = %s;\n",
								target.c_str(),
								leftv.c_str());
			return target;
		}
		case TOK_ARRAY:{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s = %s;\n",
								target.c_str(),
								leftv.c_str());
			return target;
		}
		case TOK_INDEX:{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s = %s;\n",
								target.c_str(),
								leftv.c_str());
			return target;
		}
		case TOK_IDENT:{
			return "__"+*(node->lexinfo);
		}
		case TOK_VARDECL:{
			string leftv = func_codegen(node->children[0]);
			string rightv = func_codegen(node->children[1]);
			string target = vreg(node);
			fprintf(oil_file, "        %s = %s;\n",
								target.c_str(),
								leftv.c_str());
			return target;
		}
		case TOK_FIELD:{
			return "__"+*(node->lexinfo);
		}
	}
	return *(node->lexinfo);
}

void make_oil_file(){
	//Prints structs
	string type = "";
	for(auto s: struct_table){
		fprintf(oil_file,"struct s_%s {\n",s.first->c_str());
		for(auto f: *s.second->fields){
			type = make_oil_field(f.second->attributes, s.first);
			fprintf(oil_file,"        %s f_%s_%s\n",type.c_str(),s.first->c_str(),f.first->c_str());
		}
		fprintf(oil_file,"};\n");
	}
	
	//Print string constants
	print_string_cons(parser::root);
	//Global variables
	if(stack.symbol_stack[0] != nullptr){
		for(auto s: *stack.symbol_stack[0]){
			type = make_oil_field(s.second->attributes, s.first);
			fprintf(oil_file,"%s __%s",type.c_str(),s.first->c_str());
		}
	}
	fprintf(oil_file,"\n");
	//Go through all the functions
	for(astree* child: parser::root->children){
		if(child->symbol == TOK_FUNC){
			if(child->attributes[ATTR_int])
				fprintf(oil_file,"int ");
			if(child->attributes[ATTR_string])
				fprintf(oil_file,"char* ");
			fprintf(oil_file,"__%s (",child->children[0]->children[0]->lexinfo->c_str());
			
			if(child->children[1]->symbol == TOK_PARAMLIST){
				for(astree* node: child->children[1]->children){
					switch(node->symbol){
						case TOK_INT:{
							fprintf(oil_file,"\n        int _%lu_%s",node->block_nr,node->children[0]->lexinfo->c_str());
						}
						case TOK_STRING:{
							fprintf(oil_file,"\n        char* _%lu_%s",node->block_nr,node->children[0]->lexinfo->c_str());
						}
						case TOK_IDENT:{
							fprintf(oil_file,"\n        %s _%lu_%s",node->lexinfo->c_str(),node->block_nr,node->children[0]->lexinfo->c_str());
						}
					}
				}
			}
			fprintf(oil_file,")\n{\n");
			func_codegen(child->children.back());
			fprintf(oil_file,"}\n");
		}
	}
	fprintf(oil_file,"void __ocmain (void)\n{\n");
	for(astree* child: parser::root->children){
		if(child->symbol != TOK_FUNC ||child->symbol != TOK_STRUCT || child->symbol != TOK_PROTO){
			func_codegen(child);
		}
	}
	fprintf(oil_file,"}\n");
}

symbol_table* symbol_stack::pop(){
	symbol_table* sym;
	sym = symbol_stack.back();
	symbol_stack.pop_back();
	return sym;
}

void symbol_stack::push(symbol_table* table){
	symbol_stack.push_back(table);
}

void symbol_stack::push(symbol* symbol,const string* key){
	if(symbol_stack.back() == nullptr){
		symbol_stack.back() = new symbol_table;
	}
	symbol_table* table = symbol_stack.back();
	(*table)[key] = symbol;
}

symbol* type_to_symbol(astree* node, bool array){
	symbol* a = new symbol();
	
	if(array){ 
		a->attributes[ATTR_array] = true;
		switch(node->children[0]->symbol){
		case TOK_INT:{
			a->attributes[ATTR_int] = true;
			break;
		}
		case TOK_VOID:{
			a->attributes[ATTR_void] = true;
			break;
		}
		case TOK_NULL:{
			a->attributes[ATTR_null] = true;
			break;
		}
		case TOK_STRING:{
			a->attributes[ATTR_string] = true;
			break;
		}
		case TOK_IDENT:{
			a = lookup(node->lexinfo);
			break;
		}
		}
	}
	else{
		switch(node->symbol){
			case TOK_INT:{
				a->attributes[ATTR_int] = true;
				break;
			}
			case TOK_VOID:{
				a->attributes[ATTR_void] = true;
				break;
			}
			case TOK_NULL:{
				a->attributes[ATTR_null] = true;
				break;
			}
			case TOK_STRING:{
				a->attributes[ATTR_string] = true;
				break;
			}
			case TOK_IDENT:{
				a = lookup(node->lexinfo);
				break;
			}
		}
	}
	if(a != nullptr){	
		a->lloc = node->lloc;
	}
	return a;

}

symbol* lookup(const string* name){
	if(struct_table.find(name) != struct_table.end())
		return struct_table[name];
	if(global_table.find(name) != global_table.end()){
		return global_table[name];
	}
	
	for(int i = stack.symbol_stack.size()-1; i>=0; i--){
		if(stack.symbol_stack[i] == nullptr) continue;
		if(stack.symbol_stack[i]->find(name) != stack.symbol_stack[i]->end()){
			return (*stack.symbol_stack[i])[name];
		}
	}
	

	return nullptr;
}

symbol_table* create_field_table(astree* node){
	symbol_table* table = new symbol_table();
	astree* child = nullptr;
	for(size_t i = 1; i< node->children.size();i++){
		child = node->children[i];
		symbol* s = new symbol();
		const string* key = new string();
		if(child->children[0]->symbol == TOK_NEWARRAY){
			switch(child->symbol){
				case TOK_INT:{
						s->attributes[ATTR_int] = true;
						break;
				}
				case TOK_STRING:{
					s->attributes[ATTR_string] = true;
					break;
				}
				case TOK_CHAR:{
					s->attributes[ATTR_int] = true;
					break;
				}
				case TOK_IDENT:{
					if(lookup(node->children[0]->lexinfo) != nullptr){
						s->attributes[ATTR_typeid] = true;
						//s->ref = lookup(node->children[0]->lexinfo);
					}
					break;
				}
			}
			s->lloc = child->lloc;
			s->attributes[ATTR_array] = true;
			key = child->children[0]->children[0]->lexinfo;
		}
		else{
			switch(child->symbol){
				case TOK_INT:{
					s->attributes[ATTR_int] = true;
					break;
				}
				case TOK_STRING:{
					s->attributes[ATTR_string] = true;
					break;
				}
				case TOK_CHAR:{
					s->attributes[ATTR_int] = true;
					break;
				}
				case TOK_IDENT:{
					if(lookup(node->children[0]->lexinfo) != nullptr){
						s->attributes[ATTR_typeid] = true;
						//s->ref = lookup(node->children[0]->lexinfo);
					}
					break;
				}
			}
			s->lloc = child->lloc;
			key = child->children[0]->lexinfo;
		}
		s->attributes[ATTR_field] = true;
		(*table)[key] = s;
		
		fprintf (sym_file, "   %s (%zd.%zd.%zd) field {%s}",
            key->c_str(), s->lloc.filenr,s->lloc.linenr,s->lloc.offset,
			node->children[0]->lexinfo->c_str());
		switch(child->symbol){
			case TOK_INT:{
				fprintf(sym_file," int ");
				break;
			}
			case TOK_STRING:{
				fprintf(sym_file," string ");
				break;
			}
			case TOK_CHAR:{
				fprintf(sym_file," char ");
				break;
			}
			case TOK_IDENT:{
				fprintf(sym_file," struct \"%s\" ", child->lexinfo->c_str());
				break;
			}
		}
		fprintf(sym_file,"\n");	
	}
	return table;
}

void fill_paramlist(astree* node, symbol* a){
	const string* key = new string();
	symbol* b = new symbol();
	symbol_table* table = stack.symbol_stack.back();
	if(table == nullptr){
		table = new symbol_table;
	}
	for(size_t i = 0; i < node->children.size();i++){
		if(node->children[i]->symbol == TOK_ARRAY){
			b = type_to_symbol(node->children[i],true);
			key = node->children[i]->children[1]->lexinfo;
		}
		else{
			b = type_to_symbol(node->children[i],false);
			key = node->children[i]->children[0]->lexinfo;
		}
		b->attributes[ATTR_param] = true;
		b->attributes[ATTR_lval]  = true;
		a->parameters->push_back(b);
		(*table)[key] = b;
		stack.push(b,key);
		
		for(size_t j = 0; j < block_stack.size()-1; j++){
			fprintf(sym_file,"   ");
		}
		
		fprintf (sym_file,"%s (%zd.%zd.%zd) {%d} ",
            key->c_str(),
			node->children[i]->lloc.filenr,
			node->children[i]->lloc.linenr,
			node->children[i]->lloc.offset,
			block_stack.back());
		switch(node->children[i]->symbol){
			case TOK_INT:{
				fprintf(sym_file," int ");
				break;
			}
			case TOK_STRING:{
				fprintf(sym_file," string ");
				break;
			}
			case TOK_CHAR:{
				fprintf(sym_file," char ");
				break;
			}
			case TOK_IDENT:{
				fprintf(sym_file," struct \"%s\" ", node->children[i]->lexinfo->c_str());
				break;
			}
		}
		fprintf(sym_file,"variable lval param\n");	
	}
}

bool compatible(bitset<ATTR_bitset_size> left, bitset<ATTR_bitset_size> right){
	return ((left[ATTR_array] && right[ATTR_null])||
		(left[ATTR_null] && right[ATTR_array])||
		(left[ATTR_string] && right[ATTR_null])||
		(left[ATTR_null] && right[ATTR_string])||
		(left[ATTR_struct] && right[ATTR_null])||
		(left[ATTR_null] && right[ATTR_struct])||
		(left[ATTR_typeid] && right[ATTR_null])||
		(left[ATTR_null] && right[ATTR_typeid])||
		(left[ATTR_int] && right[ATTR_int])||
		(left[ATTR_string] && right[ATTR_string])||
		(left[ATTR_array] && right[ATTR_array])||
		(left[ATTR_struct] && right[ATTR_struct])||
		(left[ATTR_typeid] && right[ATTR_typeid]));
}

symbol* lookup(astree* node){
	symbol* a = lookup(node->lexinfo);
	if(a == nullptr){
		a = new symbol();
		switch(node->symbol){
			case TOK_INTCON:{
				a->attributes[ATTR_int] = true;
				break;
			}
			case TOK_STRINGCON:{
				a->attributes[ATTR_string] = true;
				break;
			}
			case TOK_CHARCON:{
				a->attributes[ATTR_int] = true;
				break;
			}
			default:{
				a = nullptr;
				break;
			}
		}
	}
	return a;
}

astree* call_lookup(astree* node){
	astree* a = node;
	if(node->symbol != TOK_IDENT)
		a = call_lookup(node->children[0]);
	return a;
}

void semantic_analysis(astree* node){
	//Pre-order actions for functions
	switch(node->symbol){
		case TOK_ROOT:{
			stack.symbol_stack.push_back(nullptr);
			block_stack.push_back(0);
			break;
		}
		case TOK_FUNC:{
			symbol* a = type_to_symbol(node->children[0]);
			a->parameters = new vector<symbol*>;
			a->attributes[ATTR_function] = true;
			const string* key = node->children[0]->children[0]->lexinfo;
			string type = "Not supposed to be here";
			switch(node->children[0]->symbol){
				case TOK_INT:{
					type = "int";
					break;
				}
				case TOK_STRING:{
					type = "string";
					break;
				}
				case TOK_VOID:{
					type = "void";
					break;
				}
				case TOK_IDENT:{
					type = string("struct \"")+node->children[0]->lexinfo->c_str()+string("\"");
					break;
				}
			}
			
			fprintf (sym_file, "%s (%zd.%zd.%zd) {%d} %s function \n",
            key->c_str(), node->lloc.filenr,node->lloc.linenr,node->lloc.offset,
			block_stack.back(),type.c_str());
			
			block_stack.push_back(block_nr++);
			stack.symbol_stack.push_back(nullptr);
			stack.next_block++;
			
			if(node->children[1]->symbol == TOK_PARAMLIST){
				fill_paramlist(node->children[1],a);
			}
			if(a->attributes[ATTR_int]){
				node->attributes[ATTR_int] = true;
			}
			else if(a->attributes[ATTR_string]){
				node->attributes[ATTR_string] = true;
			}
			else if(a->attributes[ATTR_typeid]){
				node->attributes[ATTR_typeid] = true;
			}
			global_table[key] = a;
			break;
		}
		case TOK_PROTO:{
			block_stack.push_back(block_nr++);
			stack.symbol_stack.push_back(nullptr);
			stack.next_block++;
			symbol* a = new symbol();
			const string*  key = new string();
			if(node->children[0]->symbol == TOK_ARRAY){
				a = type_to_symbol(node->children[0]->children[0]);
				key = node->children[0]->children[1]->lexinfo;
			}
			else{
				a = type_to_symbol(node->children[0]);
				key = node->children[0]->children[0]->lexinfo;
			}
			a->parameters = new vector<symbol*>;
			a->attributes[ATTR_function] = true;
			
			string type = "Not supposed to be here";
			switch(node->children[0]->symbol){
				case TOK_INT:{
					type = "int";
					break;
				}
				case TOK_STRING:{
					type = "string";
					break;
				}
				case TOK_VOID:{
					type = "void";
					break;
				}
				case TOK_IDENT:{
					type = string("struct \"")+node->children[0]->lexinfo->c_str()+string("\"");
					break;
				}
				case TOK_ARRAY:{
					switch(node->children[0]->children[0]->symbol){
						case TOK_INT:{
							type = "array int";
							break;
						}
						case TOK_STRING:{
							type = "array string";
							break;
						}
						case TOK_VOID:{
							type = "array void";
							break;
						}
						case TOK_IDENT:{
							type = string("array struct \"")+node->children[0]->children[0]->lexinfo->c_str()+string("\"");
							break;
						}
					}
					break;
				}
			}
			
			fprintf (sym_file, "%s (%zd.%zd.%zd) {0} %s prototype \n",
            key->c_str(), node->lloc.filenr,node->lloc.linenr,node->lloc.offset,
			type.c_str());
			
			if(node->children.size() > 1){
				symbol* b = new symbol();
				//const string* key = new string();
				for(size_t i = 1; i < node->children.size(); i++){
					if(node->children[i]->symbol == TOK_ARRAY){
						b = type_to_symbol(node->children[i]->children[0], true);
						//key = node->children[i]->children[1]->lexinfo;
					}
					else{
						b = type_to_symbol(node->children[i], false);
						//key = node->children[i]->children[0]->lexinfo;
					}
					b->attributes[ATTR_param] = true;
					b->attributes[ATTR_lval] = true;
					a->parameters->push_back(b);
					
					fprintf (sym_file,"   %s (%zd.%zd.%zd) {%d} ",
						node->children[i]->lexinfo->c_str(),
						node->children[i]->lloc.filenr,
						node->children[i]->lloc.linenr,
						node->children[i]->lloc.offset,
						block_stack.back());
					switch(node->children[i]->symbol){
						case TOK_INT:{
							fprintf(sym_file," int ");
							break;
						}
						case TOK_STRING:{
							fprintf(sym_file," string ");
							break;
						}
						case TOK_CHAR:{
							fprintf(sym_file," char ");
							break;
						}
						case TOK_IDENT:{
							fprintf(sym_file," struct \"%s\" ", node->children[i]->lexinfo->c_str());
							break;
						}
						case TOK_ARRAY:{
							switch(node->children[0]->children[0]->symbol){
								case TOK_INT:{
									fprintf(sym_file, " array int ");
									break;
								}
								case TOK_STRING:{
									fprintf(sym_file," array string ");
									break;
								}
								case TOK_VOID:{
									fprintf(sym_file," array void ");
									break;
								}
								case TOK_IDENT:{
									fprintf(sym_file," array struct \"");
									fprintf(sym_file,node->children[0]->children[0]->lexinfo->c_str());
									fprintf(sym_file,"\"");
									break;
								}
							}
							break;
						}
					}
					fprintf(sym_file,"variable lval param\n");	
				}
			}
			global_table[key] = a;		
			break;
		}
		case TOK_STRUCT:{
			symbol* a = type_to_symbol(node->children[0],false);
			const string* key = node->children[0]->lexinfo;
			a->attributes[ATTR_typeid] = true;
			struct_table[key] = a;
			
			fprintf (sym_file, "\n%s (%zd.%zd.%zd) {0} struct \"%s\" \n",
            key->c_str(), node->lloc.filenr,node->lloc.linenr,node->lloc.offset,
			key->c_str());
			
			a->fields = create_field_table(node);	
			break;
		}
		case TOK_WHILE:{
			block_stack.push_back(block_nr++);
			stack.symbol_stack.push_back(nullptr);
			stack.next_block++;
			break;
		}
		case TOK_IF:{
			block_stack.push_back(block_nr++);
			stack.symbol_stack.push_back(nullptr);
			stack.next_block++;
			break;
		}
		case TOK_IFELSE:{
			block_stack.push_back(block_nr++);
			stack.symbol_stack.push_back(nullptr);
			stack.next_block++;
			break;
		}
		case TOK_BLOCK:{

			break;
		}
	}
	node->block_nr = block_stack.back();
	//Go through all the child nodes first
	for(astree* child: node->children){
		semantic_analysis(child);
	}
	//Post-order actions
	switch(node->symbol){
		case TOK_IDENT:{
			symbol* a = lookup(node->lexinfo);
			if(a == nullptr){
				errllocprintf(node->lloc,
				"Undefined identifier: %s\n",
				node->lexinfo->c_str());
				break;
			}
			else{
				node->attributes = a->attributes;
				if(node->attributes[ATTR_typeid]){
					node->ref = a->fields;
				}
			}
			(*node).ref_loc = (*a).lloc;
			break;
		}
		case TOK_VARDECL:{
			symbol* a = new symbol();
			const string* key = new string();
			if(node->children[0]->symbol == TOK_ARRAY){
				a = type_to_symbol(node->children[0],true);
				key = node->children[0]->children[1]->lexinfo;
			}
			else{
				a = type_to_symbol(node->children[0],false);
				key = node->children[0]->children[0]->lexinfo;
			}
			if(a == nullptr){
				errllocprintf(node->lloc,
				"Undefined identifier: %s\n",
				node->children[0]->lexinfo->c_str());
				break;
			}
			a->attributes[ATTR_variable] = true;
			a->attributes[ATTR_lval] = true;
			a->block_nr = block_stack.back();
			stack.push(a,key);
			
			string type = "Not supposed to be here";
			switch(node->children[0]->symbol){
				case TOK_INT:{
					type = "int";
					break;
				}
				case TOK_STRING:{
					type = "string";
					break;
				}
				case TOK_VOID:{
					type = "void";
					break;
				}
				case TOK_IDENT:{
					type = string("struct \"")+node->children[0]->lexinfo->c_str()+string("\"");
					break;
				}
				case TOK_ARRAY:{
					switch(node->children[0]->children[0]->symbol){
						case TOK_INT:{
							type = "array int";
							break;
						}
						case TOK_STRING:{
							type = "array string";
							break;
						}
						case TOK_VOID:{
							type = "array void";
							break;
						}
						case TOK_IDENT:{
							type = string("array struct \"")+node->children[0]->children[0]->lexinfo->c_str()+string("\"");
							break;
						}
					}
					break;
				}
			}
			for(size_t i = 0; i < block_stack.size()-1; i++){
				fprintf(sym_file,"   ");
			}
			fprintf (sym_file, "%s (%zd.%zd.%zd) {%d} %s variable lval\n",
            key->c_str(), 
			node->lloc.filenr,
			node->lloc.linenr,
			node->lloc.offset,
			block_stack.back(),
			type.c_str());
			
			break;
		}
		case TOK_FUNC:{
			if(node->children[0]->symbol != TOK_VOID && !node->children[0]->attributes[ATTR_void]){
				if(!compatible(node->attributes,
node->children.back()->children.back()->attributes)){
					errllocprintf(node->lloc,
					"Invalid return type.\n",
					"");
				}
			}
			stack.pop();
			block_stack.pop_back();
			fprintf(sym_file,"\n");
			break;
		}
		case TOK_PROTO:{
			stack.pop();
			block_stack.pop_back();
			fprintf(sym_file,"\n");
			break;
		}
		case TOK_WHILE:{
			stack.pop();
			block_stack.pop_back();
			break;
		}
		case TOK_IF:{
			stack.pop();
			block_stack.pop_back();
			break;
		}
		case TOK_IFELSE:{
			stack.pop();
			block_stack.pop_back();
			break;
		}
		case TOK_NEWSTRING:{
			if(!node->children[0]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Invalid string declaration. Expected: new string(int);.\n",
					"");
			}
			break;
		}
		case '=':{
			if(!node->children[0]->attributes[ATTR_lval]){
				errllocprintf(node->lloc,
					"Invalid variable assignment.\n",
					"");
			}
			else if(!compatible(node->children[0]->attributes,
					node->children[1]->attributes)){
				errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
				}
			node->attributes = node->children[0]->attributes;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_INDEX:{
			if(!node->children[1]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Expected int index.\n",
					"");
			}
			if(!node->children[0]->attributes[ATTR_array] && 
			!node->children[0]->attributes[ATTR_string]){
				errllocprintf(node->lloc,
					"Identifier is not an array.\n",
					"");
			}
			else{
				if(node->children[0]->attributes[ATTR_int]){
					node->attributes[ATTR_int] = true;
				}
				else if(node->children[0]->attributes[ATTR_string] && 
				node->children[0]->attributes[ATTR_array]){
					node->attributes[ATTR_string] = true;
					node->attributes[ATTR_array]  = true;
				}
				else if(node->children[0]->attributes[ATTR_string]){
					node->attributes[ATTR_int] = true;
				}
				else if(node->children[0]->attributes[ATTR_typeid]){
					node->attributes[ATTR_typeid] = true;
				}
			}
			node->attributes[ATTR_vaddr] = true;
			node->attributes[ATTR_lval]  = true;
			break;
		}
		case TOK_EQ:{
			if(!compatible(node->children[0]->attributes,
				node->children[1]->attributes)){
					errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
				}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_NE:{
			if(!compatible(node->children[0]->attributes,
				node->children[1]->attributes)){
					errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
				}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_GT:{
			if(!compatible(node->children[0]->attributes,
				node->children[1]->attributes)){
					errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
				}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_GE:{
			if(!compatible(node->children[0]->attributes,
				node->children[1]->attributes)){
					errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
				}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_LT:{
			if(!compatible(node->children[0]->attributes,
				node->children[1]->attributes)){
					errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
				}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_LE:{
			if(!compatible(node->children[0]->attributes,
				node->children[1]->attributes)){
					errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
				}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_NEWARRAY:{
			symbol* a = type_to_symbol(node->children[0]);
			if(a->attributes[ATTR_int])
				node->attributes[ATTR_int] = true;
			else if(a->attributes[ATTR_string])
				node->attributes[ATTR_string] = true;
			if(node->children.size() > 1){
				if(!node->children[1]->attributes[ATTR_int]){
					errllocprintf(node->lloc,
					"Array index requires int.\n",
					"");
				}
			}
			break;
		}
		case '.':{
			//Look up children 0 in struct table
			if(!node->children[0]->attributes[ATTR_typeid]){
				errllocprintf(node->lloc,
					"Identifier is not reference a struct.\n",
					"");
				break;
			}
			astree* s = call_lookup(node->children[0]);
			symbol* a = lookup(s->lexinfo);
			if(a->fields->find(node->children[1]->lexinfo) == a->fields->end()){
				errllocprintf(node->lloc,
					"Struct %s does not have field.\n",
					node->children[0]->lexinfo->c_str());
			}else{
				symbol* b = (*a->fields)[node->children[1]->lexinfo];
				node->attributes = b->attributes;
				node->attributes[ATTR_vaddr] = true;
				node->attributes[ATTR_lval] = true;
				node->ref = a->fields;
			}
			break;
		}
		case TOK_INTCON:{
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_const] = true;
			break;
		}
		case TOK_CHARCON:{
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_const] = true;
			break;
		}
		case TOK_STRINGCON:{
			node->attributes[ATTR_string] = true;
			node->attributes[ATTR_const] = true;
			node->string_con = string("char s")+ to_string(string_num++) +string(" = \"")+node->lexinfo->c_str()+string(";");
			break;
		}
		case TOK_NULL:{
			node->attributes[ATTR_null] = true;
			break;
		}
		case TOK_RETURN:{
			if(node->children[0]->attributes[ATTR_int]){
				node->attributes[ATTR_int] = true;
			}
			else if(node->children[0]->attributes[ATTR_string]){
				node->attributes[ATTR_string] = true;
			}
			else if(node->children[0]->attributes[ATTR_typeid]){
				node->attributes[ATTR_typeid] = true;
			}
			break;
		}
		case TOK_ARRAY:{
			node->attributes[ATTR_array] = true;
			switch(node->children[0]->symbol){
				case TOK_INT:{
					node->attributes[ATTR_int] = true;
					break;
				}
				case TOK_CHAR:{
					node->attributes[ATTR_int] = true;
					break;
				}
				case TOK_STRING:{
					node->attributes[ATTR_string] = true;
					break;
				}
				case TOK_IDENT:{
					symbol* a = lookup(node->children[0]->lexinfo);
					if(a == nullptr){
						errllocprintf(node->lloc,
						"Undefined identifier: %s\n",
						node->lexinfo->c_str());
						break;
					}
					node->attributes = a->attributes;
					break;
				}
				case TOK_VOID:{
					bitset<ATTR_bitset_size> a;
					node->attributes = a;
					errllocprintf(node->lloc,
						"Void is not a valid array type: %s\n",
						node->lexinfo->c_str());
					node->attributes[ATTR_void] = true;
					break;
				}
			}
			break;
		}
		case TOK_NEG:{
			if(node->children[0]->symbol == TOK_IDENT){
				symbol* a = lookup(node->children[0]->lexinfo);
				if(a == nullptr){
					errllocprintf(node->lloc,
					"Undefined identifier: %s\n", 
					node->children[0]->lexinfo->c_str());
					break;
				}
				else if(!(a->attributes[ATTR_int] && 1)){
					errllocprintf(node->lloc,
				"Incompatible type, expected int.\n",
				"");
				}
			}
			else if(node->children[0]->symbol != TOK_INTCON){
				errllocprintf(node->lloc,
				"Incompatible type, expected int.\n",
				"");
			}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_POS:{
			if(node->children[0]->symbol == TOK_IDENT){
				symbol* a = lookup(node->children[0]->lexinfo);
				if(a == nullptr){
					errllocprintf(node->lloc,
					"Undefined identifier: %s\n", 
					node->children[0]->lexinfo->c_str());
					break;
				}
				else if(!(a->attributes[ATTR_int] && 1)){
					errllocprintf(node->lloc,
				"Incompatible type, expected int.\n",
				"");
				}
			}
			else if(node->children[0]->symbol != TOK_INTCON){
				errllocprintf(node->lloc,
				"Incompatible type, expected int.\n",
				"");
			}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case '!':{
			if(!node->children[0]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
				"Undefined identifier: %s\n",
				node->children[0]->lexinfo->c_str());
			}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_CALL:{
			symbol* a = lookup(node->children[0]->lexinfo);
			if(a == nullptr){
				errllocprintf(node->lloc,
				"Undefined identifier: %s\n",
				node->children[0]->lexinfo->c_str());
				break;
			}
			else{
				//Lookup function and set the attribute on this node to the return type
				if(a->attributes[ATTR_int]){
					node->attributes[ATTR_int] = true;
				}
				else if(a->attributes[ATTR_string]){
					node->attributes[ATTR_string] = true;
				}
				else if(a->attributes[ATTR_typeid]){
					node->attributes[ATTR_typeid] = true;
				}
			}
			if(node->children.size() != 1 + a->parameters->size()){
				errllocprintf(node->lloc,
					"Wrong number of arguments!%s\n",
					"");
			}
			else if(a->parameters->size() > 0 && node->children.size() > 1){
				//Cycle through the parameters comparing to the function parameters for type
				for(size_t i = 1; i < node->children.size(); i++){
					if(!compatible(node->children[i]->attributes,(*a->parameters)[i-1]->attributes)){
						errllocprintf(node->lloc,
						"Incompatible types.\n",
						"");
						break;
					}
				}
			}
			
			break;
				
		}
		case '+':{
			if(!node->children[0]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
			}
			else if(!node->children[1]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
			}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case '-':{
			if(!node->children[0]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
			}
			else if(!node->children[1]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
			}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case '/':{
			if(!node->children[0]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
			}
			else if(!node->children[1]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
			}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case '*':{
			if(!node->children[0]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
			}
			else if(!node->children[1]->attributes[ATTR_int]){
				errllocprintf(node->lloc,
					"Incompatible types.\n",
					"");
			}
			node->attributes[ATTR_int] = true;
			node->attributes[ATTR_vreg] = true;
			break;
		}
		case TOK_FIELD:{
			node->attributes[ATTR_field] = true;
			break;
		}
		case TOK_INT:{
			node->attributes[ATTR_int] = true;
			break;
		}
		case TOK_CHAR:{
			node->attributes[ATTR_int] = true;
			break;
		}
		case TOK_STRING:{
			node->attributes[ATTR_string] = true;
			break;
		}
	}	
}
