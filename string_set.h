// $Id: string_set.h,v 1.1 2017/04/15 03:38:25 ttching Exp $

#ifndef __STRING_SET__
#define __STRING_SET__

#include <string>
#include <unordered_set>
using namespace std;

#include <stdio.h>

struct string_set {
   string_set();
   static unordered_set<string> set;
   static const string* intern (const char*);
   static void dump (FILE*);
};

#endif

