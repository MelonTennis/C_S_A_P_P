/*
 *
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp);
 * A is the source matrix, B is the destination
 * tmp points to a region of memory able to hold TMPCOUNT (set to 256) doubles as temporaries
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 2KB direct mapped cache with a block size of 64 bytes.
 *
 * Programming restrictions:
 *   No out-of-bounds references are allowed
 *   No alterations may be made to the source array A
 *   Data in tmp can be read or written
 *   This file cannot contain any local or global doubles or arrays of doubles
 *   You may not use unions, casting, global variables, or 
 *     other tricks to hide array data in other forms of local or global memory.
 */ 
#include <stdio.h>
#include <stdbool.h>
#include "cachelab.h"
#include "contracts.h"

/* Forward declarations */
bool is_transpose(size_t M, size_t N, double A[N][M], double B[M][N]);
void trans(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp);
void trans_tmp(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";


void transpose_submit(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp)
{
    /*
     * This is a good place to call your favorite transposition functions
     * It's OK to choose different functions based on array size, but
     * your code must be correct for all values of M and N
     */
	int s = 5, b = 6;  // cache property
	int B_cache = 1 << b;  // block size
	int S = 1 << s;  // set number
	int cache_size = B_cache * S;
	int type_size = 8;  // double, 8 byte
	int i, j, i1, j1;  // loop variables
	int diag;  // record of diagonal index	
	int set_num;  // current set number in cache
	int idx;  // idx in tmp where to store diagonal data

	if(M == 32 && N ==32) {
		int block_size = 8;
		for(i = 0; i < N; i += block_size) {
			for(j = 0; j < M; j += block_size) {
				// block_size * block_size mini matrix
				for(i1 = i; i1 < i + block_size; i1++) {
					for(j1 = j; j1 < j + block_size; j1++) {
						if(i1 != j1 || i != j) {
							B[j1][i1] = A[i1][j1];
						} else {
							// reduce miss in diagonal, save data in A in tmp
							int matri_pos;  // position in matrix
							diag = i1;
							matri_pos = i * M * type_size + j * type_size;
							set_num = (matri_pos % cache_size) / B_cache;
							// save data in an unused set
							idx = (set_num + 1) * type_size + j1 - j; 
							tmp[idx] = A[i1][j1];	
						}
					}
					if(i == j) {
						B[diag][diag] = tmp[idx];
					}
				}
			}
		}
	return;
	} else if(M == 64 && N == 64) {
		int block_size = 8;
		diag = -1;
		for(i = 0; i < N; i += block_size) {
			for(j = 0; j < M; j += block_size) {
				// block_size * block_size mini matrix
				for(i1 = i; i1 < i + block_size / 2; i1++) {
					for(j1 = j; j1 < j + block_size; j1++) {
						if(i1 != j1 && j1 < j + block_size / 2) {
							B[j1][i1] = A[i1][j1];
						} else {
								int matri_pos;  // position in matrix
								matri_pos = i * M * type_size + j * type_size;
								set_num = (matri_pos % cache_size) / B_cache;
							if(i1 == j1) {
								// save data in an unused set
								idx = (set_num + 1) * type_size + j1 - j; 
								diag = i1;
								tmp[idx] = A[i1][j1];	
							} else {
								int idx2 = (set_num + 2 + i1 -i) * type_size;
								tmp[idx2 + j1 - (j + block_size/2)] = A[i1][j1];
							}
						}
					}
					if(i == j && diag >= 0) {
						B[diag][diag] = tmp[idx];
					}
				} 
				for(int k = 0; k < block_size / 2; k++) {
					for(int l = 0; l < block_size / 2; l++) {
						B[j + k + block_size / 2][i + l] = tmp[(set_num + 2 + l) * type_size + k];
					}
				}
				for(i1 = i + block_size / 2; i1 < i + block_size; i1++) {
					for(j1 = j; j1 < j + block_size; j1++) {
						if(j1 < j + block_size / 2) {
							int matri_pos;  // position in matrix
							matri_pos = i * M * type_size + j * type_size;
							set_num = (matri_pos % cache_size) / B_cache;
							int idx2 = (set_num + 2 + i1 - i - block_size/2) * type_size; 
							tmp[j1 - j + idx2] = A[i1][j1];
						} else {
							if(j1 != i1) B[j1][i1] = A[i1][j1];
							else {
								int matri_pos;
								matri_pos = i * M * type_size + j * type_size;
								set_num = (matri_pos % cache_size) / B_cache;
								diag = i1;
								idx = (set_num + 1) * type_size + j1 - j;
								tmp[idx] = A[i1][j1];
							}
						}
					}
					if(i == j && diag >= 0) {
						B[diag][diag] = tmp[idx];
					}	
				}
				for(int k = 0; k < block_size / 2; k++) {
					for(int l = 0; l < block_size / 2; l++) {
						B[k + j][i + l + block_size / 2] = tmp[(set_num + 2 + l) * type_size + k];
					}
				}
			}
		}
	return;
	} else if(M == 63 && N ==65) {
		int block_size = 4;
		for(i = 0; i < N; i += block_size) {
			for(j = 0; j < M; j += block_size) {
				// block_size * block_size mini matrix
				for(i1 = i; i1 < i + block_size && i1 < N; i1++) {
					for(j1 = j; j1 < j + block_size && j1 < M; j1++) {
						if(i1 != j1 || i != j) {
							B[j1][i1] = A[i1][j1];
						}else {
							// reduce miss in diagonal, save data in A in tmp
							int matri_pos;  // position in matrix
							diag = i1;
							matri_pos = i * M * type_size + j * type_size;
							set_num = (matri_pos % cache_size) / B_cache;
							// save data in an unused set
							idx = (set_num + 1) * type_size + j1 - j; 
							tmp[idx] = A[i1][j1];	
						}
					}
					if(i == j) {
						B[diag][diag] = tmp[idx];
					}
				}
			}
		}
	return;
	} else {
        trans_tmp(M, N, A, B, tmp);
	}
}
/* 
 * You can define additional transpose functions below. We've defined
 * some simple ones below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";

/*
 * The following shows an example of a correct, but cache-inefficient
 *   transpose function.  Note the use of macros (defined in
 *   contracts.h) that add checking code when the file is compiled in
 *   debugging mode.  See the Makefile for instructions on how to do
 *   this.
 *
 *   IMPORTANT: Enabling debugging will significantly reduce your
 *   cache performance.  Be sure to disable this checking when you
 *   want to measure performance.
 */
void trans(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp)
{
    size_t i, j;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            B[j][i] = A[i][j];
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * This is a contrived example to illustrate the use of the temporary array
 */

char trans_tmp_desc[] =
    "Simple row-wise scan transpose, using a 2X2 temporary array";

void trans_tmp(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp)
{
    size_t i, j;
    /* Use the first four elements of tmp as a 2x2 array with row-major ordering. */
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            int di = i%2;
            int dj = j%2;
            tmp[2*di+dj] = A[i][j];
            B[j][i] = tmp[2*di+dj];
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 
    registerTransFunction(trans_tmp, trans_tmp_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
bool is_transpose(size_t M, size_t N, double A[N][M], double B[M][N])
{
    size_t i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return false;
            }
        }
    }
    return true;
}

