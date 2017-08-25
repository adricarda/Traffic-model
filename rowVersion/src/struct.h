#ifndef __STRUCT__
#define __STRUCT__
//#define PRINT

#define _Ncores 16 
#define _Nelem 128

typedef struct {
	unsigned coreID;
	unsigned corenum;
	unsigned row;
	unsigned col;
	unsigned row_next;
	unsigned col_next;
	unsigned row_previous;
	unsigned col_previous;
	char _grid[2][_Nelem* (_Nelem/_Ncores)];
} core_t;

typedef struct {
	char grid[_Nelem*_Nelem];
	int nsteps;
} shared_buf_t;

typedef struct {
	void  *pBase;   
	char  *pGrid;   
	int *pNsteps;
} shared_buf_ptr_t;

#endif 
