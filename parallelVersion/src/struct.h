#ifndef __STRUCT__
#define __STRUCT__
#define PRINT

#define _Ncores 16
#define _Nelem 256
#define _Nside 64

typedef struct {
    unsigned coreID;
    unsigned corenum;
    unsigned row;
    unsigned col;
    unsigned rowh;
    unsigned rowhnext;
    unsigned colh;
    unsigned colhnext;
    unsigned rowv;
    unsigned rowvnext;
    unsigned colv;
    unsigned colvnext;

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
