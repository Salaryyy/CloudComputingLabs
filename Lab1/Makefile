Sudoku: sudoku_dancing_links.o main.o tools.o
	g++ $^ -lpthread -o sudoku
sudoku_dancing_links.o:sudoku_dancing_links.cc
	g++ -c sudoku_dancing_links.cc -o sudoku_dancing_links.o
main.o:main.cc
	g++ -c main.cc -lpthread -o main.o
tools.o:tools.c
	g++ -c tools.c -o tools.o
clean:
	rm *.o sudoku