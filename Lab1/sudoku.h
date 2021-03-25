#ifndef SUDOKU_H
#define SUDOKU_H

enum { ROW=9, COL=9, N = 81, NEIGHBOR = 20 };
bool solve_sudoku_dancing_links(int *board);
#endif
