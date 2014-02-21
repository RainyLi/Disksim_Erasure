all:gen_parv trace

gen_parv:gen_parv.cpp scanner.o common.o raid5.o rdp.o
	g++ -g -o gen_parv scanner.o common.o raid5.o gen_parv.cpp -lgflags
scanner.o:lexer.l
	flex lexer.l
	g++ -c scanner.cpp
common.o:common.cpp common.h
	g++ -c common.cpp
raid5.o:raid5.cpp raid5.h
	g++ -c raid5.cpp
rdp.o:rdp.cpp rdp.h
	g++ -c rdp.cpp
trace:trace.cpp
	g++ -o trace trace.cpp -lgflags
clean:
	rm -rf *.o gen_parv trace

