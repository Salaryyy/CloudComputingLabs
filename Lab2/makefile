httpserver: httpserver.o threadpool.o tools.o
	gcc $^ -lpthread -o httpserver
httpserver.o: httpserver.c
	gcc -c httpserver.c -o httpserver.o
threadpool.o: threadpool.c
	gcc -c threadpool.c -o threadpool.o
tools.o: tools.c
	gcc -c tools.c -o tools.o
clean: 
	rm *.o httpserver