##
## @file simple_message_server.c
## @file simple_message_client.c
## Verteilte Systeme TCP File
## 
## @author Thomas Schmid     1410258013 <thomas.schmid@technikum-wien.at>
## @author Andrea Maierhofer 1410258024 <andrea.maierhofer@technikum-wien.at>
## @date 2015/11/24
## 
## @version $Revision: 10 $
## 
## @todo
## 
## Last Modified: $Author: Thomas Schmid $
##

##
## ------------------------------------------------------------- variables --
##


CC=gcc
CFLAGS=-Wall -Werror -Wextra -Wstrict-prototypes -pedantic -fno-common -g -O3 -std=gnu11
CFLGS2=-Wall -Werror -Wextra -Wstrict-prototypes -pedantic -fno-common -g -O3 -o simple_message_client simple_message_client.o -lsimple_message_client_commandline_handling
CFLGS3=-Wall -Werror -Wextra -Wstrict-prototypes -pedantic -fno-common -g -O3 -o simple_message_server simple_message_server.o
GREP=grep
DOXYGEN=doxygen


OBJECTS= simple_message_server.o

EXCLUDE_PATTERN=footrulewidth

##
## ----------------------------------------------------------------- rules --
##

## jedes Object-File haengt vom gleichnamigen C-File ab
%.o : %.c
	## gcc kompiliert .c zu .o
	$(CC) $(CFLAGS) -c $<

##
## --------------------------------------------------------------- targets --
##

## "make all"
all: client_server


## client_server haengt von allen Eintraegen in der Liste OBJECTS ab
client_server: $(OBJECTS)
	$(CC) $(CFLGS3)

clean:
	rm -f *.o simple_message_client simple_message_server ok.png vcs_tcpip_bulletin_board_response.html
  

distclean: clean
	rm -f -r doc

## doc haengt von pdf ab, (welches von html abhängt)
doc: pdf

## html ruft doxygen auf
html:
	$(DOXYGEN) doxygen.dcf

## Testet den Client
testclient:
	export SMSL_TESTCASE=1 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test1 && \
	export SMSL_TESTCASE=2 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test2 && \
	export SMSL_TESTCASE=3 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test3 && \
	export SMSL_TESTCASE=4 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test4 && \
	export SMSL_TESTCASE=5 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test5 && \
	export SMSL_TESTCASE=6 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test6 && \
	export SMSL_TESTCASE=7 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test7

testserver:
	export SMSL_TESTCASE=1 && \
	simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test1 && \
	export SMSL_TESTCASE=2 && \
	simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test2 && \
	export SMSL_TESTCASE=3 && \
	simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test3 && \
	export SMSL_TESTCASE=4 && \
	simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test4 && \
	export SMSL_TESTCASE=5 && \
	simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test5 && \
	export SMSL_TESTCASE=6 && \
	simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test6 && \
	export SMSL_TESTCASE=7 && \
	simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test7
	
testserver_local:
	export SMSL_TESTCASE=1 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test1 && \
	export SMSL_TESTCASE=2 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test2 && \
	export SMSL_TESTCASE=3 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test3 && \
	export SMSL_TESTCASE=4 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test4 && \
	export SMSL_TESTCASE=5 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test5 && \
	export SMSL_TESTCASE=6 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test6 && \
	export SMSL_TESTCASE=7 && \
	./simple_message_client -s localhost -p 6823 -u AUTOTESTER -m Test7

## pdf haengt von html ab, (welches doxygen aufruft)
pdf: html
	cd doc/pdf && \
	mv refman.tex refman_save.tex && \
	$(GREP) -v $(EXCLUDE_PATTERN) refman_save.tex > refman.tex && \
	rm -f refman_save.tex && \
	make && \
	mv refman.pdf refman.save && \
	rm -f *.pdf *.html *.tex *.aux *.sty *.log *.eps *.out *.ind *.idx \
	      *.ilg *.toc *.tps *.md5 *.ttf Makefile && \
	mv refman.save refman.pdf

##
## ---------------------------------------------------------- dependencies --
##

##
## =================================================================== eof ==
##
