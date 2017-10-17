// $Id: astree.h,v 1.7 2016-10-06 16:13:39-07 - - $

#ifndef __ASTREE_H__
#define __ASTREE_H__

#include <string>
#include <vector>
#include <bitset>
#include <unordered_map>
using namespace std;
struct symbol;
using symbol_table = unordered_map<const string*,symbol*>;
#include "auxlib.h"

enum { ATTR_void, ATTR_int, ATTR_null, ATTR_string,
       ATTR_struct, ATTR_array, ATTR_function, ATTR_variable,
       ATTR_field, ATTR_typeid, ATTR_param, ATTR_lval, ATTR_const,
       ATTR_vreg, ATTR_vaddr, ATTR_bitset_size
};

extern FILE* sym_file;
extern FILE* oil_file;

struct location {
   size_t filenr;
   size_t linenr;
   size_t offset;
};



struct astree {

   // Fields.
   int symbol;               // token code
   location lloc;            // source location
   const string* lexinfo;    // pointer to lexical information
   vector<astree*> children; // children of this n-way node
   static FILE* tok_file;
   bitset<ATTR_bitset_size> attributes;
   size_t block_nr;
   symbol_table* ref;
   location ref_loc;
   string string_con;
   // Functions.
   static void setFile(string name);
   static void closeFile();
   astree (int symbol, const location&, const char* lexinfo);
   ~astree();
   astree* adopt (astree* child1, astree* child2 = nullptr);
   astree* adopt_sym (astree* child, int symbol);
   void dump_node (FILE*);
   void dump_tree (FILE*, int depth = 0);
   static void dump (FILE* outfile, astree* tree);
   static void print (FILE* outfile, astree* tree, int depth = 0);
};

struct symbol{
   bitset<ATTR_bitset_size> attributes;
   symbol_table* fields;
   symbol_table* ref;
   location lloc;
   size_t block_nr;
   vector<symbol*>* parameters;
   string* ref_name;
};

struct symbol_stack{
   vector<symbol_table*> symbol_stack;
   int next_block = 1;
  
   symbol_table* pop();
   void push(symbol* symbol,const string* key);
   void push(symbol_table* table); 
};

symbol* type_to_symbol(astree* node, bool isArray = false);
symbol* lookup(const string* name);
symbol* lookup(astree* node);
astree* call_lookup(astree* node);
symbol_table* create_field_table(astree* node);
bool is_both_ints(bitset<ATTR_bitset_size> left, bitset<ATTR_bitset_size> right);
bool math_expr_check(astree* node);
bool compatible(bitset<ATTR_bitset_size> left, bitset<ATTR_bitset_size> right);
void semantic_analysis(astree* node);
void destroy (astree* tree1, astree* tree2 = nullptr);
void errllocprintf (const location&, const char* format, const char*);
void make_oil_file();
string make_oil_field(bitset<ATTR_bitset_size> bits,const string* type);
string vreg(astree* node);
string func_codegen(astree* node);
#endif

