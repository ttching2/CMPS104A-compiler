/* Trevor Ching
 * oc.cpp
 * */
#include <string>
#include <unistd.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include "auxlib.h"
#include "string_set.h"
#include "astree.h"
#include "lyutils.h"

using namespace std;
FILE* sym_file;
FILE* oil_file;
const string CPP = "cpp -nostdinc";
constexpr size_t LINESIZE = 1024;

//Chomps off the end of a string once a 
//certain delim is found and replaces
//it with a null terminating character
void chomp(char* word, char delim){
	size_t len = strlen(word);
	if(len == 0) return;
	char* nlpos = word + len - 1;
	if(*nlpos == delim) *nlpos = '\0';
}

//Runs cpp against a file, example from class website used as reference
void cpplines(FILE* pipe, const char* filename){
	char inputname[LINESIZE];
	strcpy(inputname, filename);
	for(;;){
		char buffer[LINESIZE];
		char* fgets_rc = fgets(buffer, LINESIZE, pipe);
		//Break at the end of the buffer
		if (fgets_rc == NULL) break;
		//If a new line is in the buffer then chomp off the newline and
		//place in a terminating character
		chomp(buffer, '\n');
		char* savepos = NULL;
		char* bufptr = buffer;
		//Tokenize the line of input and add it into the string set
		for(;;){
			char* token = strtok_r(bufptr," \t\n", &savepos);
			bufptr = NULL;
			if(token == NULL) break;
			//Adds the token to the string_set
			string_set::intern(token);
		}
	}
}

int main(int argc, char** argv){
	exec::execname = basename(argv[0]);
	//Check for arguments, prints usage
	if(argc == 1){
		fprintf(stderr,"Usage: oc [-ly] [-@ flag...] [-D string]"
			" program.oc\n");
		return 1;
	}
	//Gets the file name check file extension and what not
	string filename = basename(argv[argc-1]);
	if(filename.substr(filename.find_last_of(".") + 1).compare("oc") != 0){
		fprintf(stderr,"File extension does not match\n");
		return 1;
	}
	int opt;
	string command = CPP+ " ";
	//Commented out both ints so that the compileri doesn't complain
	yy_flex_debug = 0;
	yydebug	      = 0;
	//Gets the command line argments
	while((opt = getopt(argc, argv, "ly@:D:")) != -1){
		if(opt == 'l'){
			yy_flex_debug = 1;
		}else if(opt == 'y'){
			yydebug = 1;
		}else if(opt == '@'){
			set_debugflags(optarg);
		}else if(opt == 'D'){
			command +="-D"+string(optarg)+" ";
		}else{
			fprintf(stderr,"Invalid argument used. Avaliable args:"
				" [-ly] [-@] [-D]\n");
			return 1;
		}
	}
	//Piece together the cpp command
	command += string(filename);
	DEBUGF('s',"%s\n",command);
	//Create tok file, the tok file is created as yyparse runs
	string tok_file_name = filename.substr(0,filename.find("."))+".tok";
	astree::setFile(tok_file_name);
	string sym_file_name = filename.substr(0,filename.find("."))+".sym";
	sym_file = fopen(sym_file_name.c_str(), "w");
	string ast_file_name = filename.substr(0,filename.find("."))+".ast";
	string oil_file_name = filename.substr(0,filename.find("."))+".oil";
	oil_file = fopen(oil_file_name.c_str(), "w");

	FILE* ast_file;
	ast_file = fopen(ast_file_name.c_str(), "w");
	yyin = popen(command.c_str(), "r");
	int parse_rc = yyparse();
	astree::closeFile();
	if(parse_rc){
		errprintf("parse failed (%d)\n", parse_rc);
	}
	else{

		semantic_analysis(parser::root);
		if(pclose(sym_file) != 0) return 1;
		astree::print(ast_file,parser::root);
		if(pclose(ast_file) != 0) return 1;

		//Do the oil file thingy
		fprintf(oil_file,"#define __OCLIB_C__\n");
		fprintf(oil_file,"#include \"oclib.oh\"\n\n");
		make_oil_file();
		if(pclose(oil_file) != 0) return 1;
		delete parser::root;
	}
	//Dump the string_set into a file

	string project_stringADT_file = filename.substr(0,
			filename.find("."))+".str";

	FILE* str_file;
	str_file = fopen(project_stringADT_file.c_str(), "w");
	if(str_file == NULL){ 
		fprintf(stderr, "Error opening file");
		return 1;
	}
	string_set::dump(str_file);
	if(pclose(str_file) != 0) return 1;
	
	return 0;
}
