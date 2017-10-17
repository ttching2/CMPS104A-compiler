GCC	  = g++ -g -O0 -Wall -Wextra -std=gnu++14
MKDEP	  = g++ -MM -std=gnu++14
FLEX      = flex --outfile=${CLGEN}
BISON     = bison --defines=${HYGEN} --output=${CYGEN} --xml

MKFILE	  = Makefile
DEPFILE	  = Makefile.dep
SOURCES	  = oc.cpp auxlib.cpp string_set.cpp astree.cpp lyutils.cpp yylex.cpp yyparse.cpp
EXEC	  = oc
SMALLFILES= ${DEPFILE} auxlib.h string_set.h astree.h lyutls.h
CHECKINS  = ${SOURCES} ${MKFILE} ${SMALLFILES} scanner.l
LSOURCES  = scanner.l
YSOURCES  = parser.y
CLGEN     = yylex.cpp
HYGEN     = yyparse.h
CYGEN     = yyparse.cpp
LREPORT   = yylex.output
YREPORT   = yyparse.output
TRASH     = *.oc *.oc.out *.oc.err *.str *.tok *.ast *.lexyacctrace *.sym *.oil
OBJECTS   = ${SOURCES:.cpp=.o}

all : ${SOURCES} ${CLGEN} ${CYGEN} ${DEPFILE}
	${GCC} -o${EXEC} ${SOURCES}

${CLGEN} : ${LSOURCES}
	flex --outfile=${CLGEN} ${LSOURCES}

${CYGEN} ${HYGEN} : ${YSOURCES}
	bison --defines=${HYGEN} --output=${CYGEN} ${YSOURCES}

clean:
	- rm ${TRASH} ${CLGEN} ${CYGEN} ${YREPORT} ${HYGEN} ${OBJECTS}

spotless: clean
	- rm ${EXEC} ${DEPFILE}

ci:
	ci -l ${CHECKINS}
	checksource ${CHECKINS} 

${DEPFILE} : ${CLGEN} ${CYGEN}
	${MKDEP} ${SOURCES} > ${DEPFILE}

dep :
	@ touch ${DEPFILE}
	- rm ${DEPFILE}
	${MAKE} --no-print-directory ${DEPFILE}

include Makefile.dep
