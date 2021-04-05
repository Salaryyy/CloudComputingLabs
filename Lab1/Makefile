Sudoku: sudoku_dancing_links.o main.o
	g++ $^ -lpthread -o sudoku
sudoku_dancing_links.o:sudoku_dancing_links.cc
	g++ -c sudoku_dancing_links.cc -o sudoku_dancing_links.o
main.o:main.c
	g++ -c main.c -lpthread -o main.o
.PHONY:
clean:
	rm *.o sudoku
