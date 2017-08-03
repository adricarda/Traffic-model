#ifndef __STRUCT__
#define __STRUCT__
//#define PRINT

#define _Ncores 16
#define _Nelem 128
#define _Nside 32

typedef struct {
    unsigned coreID;
    unsigned corenum;
    unsigned row;
    unsigned col;
	unsigned left_core_row;
	unsigned left_core_column;
	unsigned right_core_row;
	unsigned right_core_column;
	unsigned upper_core_row;
	unsigned upper_core_column;
	unsigned lower_core_row;
	unsigned lower_core_column;
	char _grid[2][_Nside*_Nside];
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
