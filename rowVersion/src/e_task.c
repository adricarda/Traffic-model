#include <stdio.h>
#include <stdlib.h>
#include "e-lib.h"
#include "struct.h"
#include <string.h>
#include <stddef.h>

char EMPTY = 0;  /* empty cell                   */
char LR = 1;     /* left-to-right moving vehicle */
char TB = 2 ;     /* top-to-bottom moving vehicle */

core_t me;
int cur = 0;
int nrows= _Nelem / _Ncores;

void horizontal_step(core_t *me)
{
	int i, j;
    const int next = 1 - cur;

	for (i=0; i<nrows; i++) {
		for (j=0; j<_Nelem; j++) {
            const int left = (j > 0 ? j-1 : _Nelem-1);
            const int right = (j < _Nelem-1 ? j+1 : 0);
			if ( me->_grid[cur][i*_Nelem+j] == EMPTY && me->_grid[cur][i*_Nelem+left] == LR ) {
                me->_grid[next][i*_Nelem+j] = LR;
            }
            else
			{
           		if (me->_grid[cur][i*_Nelem+j] == LR && me->_grid[cur][i*_Nelem+right] == EMPTY) {
					me->_grid[next][i*_Nelem+j] = EMPTY;
                } else
 					me->_grid[next][i*_Nelem+j] = me->_grid[cur][i*_Nelem+j];
            }
        }
    }

}

void vertical_step( char *firstRow, char *lastRow, core_t *me)
{
    int i, j;
    const int next = 1 - cur;
	//update first row
	for(j=0; j<_Nelem; j++)
	{
		if (me->_grid[cur][j] == EMPTY && firstRow[j] == TB )
		{
			me->_grid[next][j] = TB;
		}
		else {
			if ( me->_grid[cur][j] == TB && me->_grid[cur][1*_Nelem+j] == EMPTY) {
				me->_grid[next][j] = EMPTY;
           	} else
 				me->_grid[next][j] = me->_grid[cur][j];
        }
	}
    
	for (i=1; i<nrows-1; i++) {
        const int top = i-1;
        const int bottom = i+1;
        for (j=0; j<_Nelem; j++)
		{
			if (me->_grid[cur][i*_Nelem+j] == EMPTY && me->_grid[cur][top*_Nelem+j] == TB ) {
                    me->_grid[next][i*_Nelem+j] = TB;
            }
            else {
                  if (me->_grid[cur][i*_Nelem+j] == TB && me->_grid[cur][bottom*_Nelem+j] == EMPTY) {
                    me->_grid[next][i*_Nelem+j] = EMPTY;
                  } else {
                        me->_grid[next][i*_Nelem+j] = me->_grid[cur][i*_Nelem+j];
					}
            }
        }
    }
	//update last row
	i=nrows-1;
	for(j=0; j<_Nelem; j++)
	{
		if (me->_grid[cur][i*_Nelem+j] == EMPTY && me->_grid[cur][(i-1)*_Nelem+j] == TB )
		{
			me->_grid[next][i*_Nelem+j] = TB;
		}
		else {
			if ( me->_grid[cur][i*_Nelem+j] == TB && lastRow[j] == EMPTY) {
				me->_grid[next][i*_Nelem+j] = EMPTY;
           	} else
 				me->_grid[next][i*_Nelem+j] = me->_grid[cur][i*_Nelem+j];
        }
	}

}

void data_copy(e_dma_desc_t *dma_desc, void *dst, void *src)
{
	// Make sure DMA is inactive before modifying the descriptor
	e_dma_wait(E_DMA_0);
	dma_desc->src_addr = src;
	dma_desc->dst_addr = dst;
	e_dma_start(dma_desc, E_DMA_0);

	// All DMA transfers are blocking, so wait for process to finish
	e_dma_wait(E_DMA_0);

	return;
}

int main(void)
{
	volatile shared_buf_ptr_t Mailbox;
    int nsteps;
	float elapsed;
    e_dma_desc_t smem2local ;
    e_dma_desc_t vneigh2local ;
    e_dma_desc_t local2smem ;
    volatile e_barrier_t  barriers[_Ncores];
    volatile e_barrier_t *tgt_bars[_Ncores];
    int i,j,k;
	volatile unsigned *go, *ready; 
    char upper_row[_Nelem];
    char lower_row[_Nelem];
	e_mutex_t mutex = MUTEX_NULL; 
	char *upper_core_grid;
	char *lower_core_grid;
		
	//initialization
	go = (unsigned *) 0x7000;//Done flag
	ready = (unsigned *) 0x7010;//ready flag to signal end of computation

    me.coreID = e_get_coreid();
    me.row = e_group_config.core_row;
    me.col = e_group_config.core_col;
    me.corenum = me.row * e_group_config.group_cols + me.col;
   
	e_neighbor_id(E_PREV_CORE, E_GROUP_WRAP, &me.row_previous, &me.col_previous);
    e_neighbor_id(E_NEXT_CORE, E_GROUP_WRAP, &me.row_next, &me.col_next);
 	upper_core_grid =e_get_global_address(me.row_previous, me.col_previous, &me._grid[1][0]);
	lower_core_grid =e_get_global_address(me.row_next, me.col_next, &me._grid[1][0]);
	   
	Mailbox.pBase = (void *) e_emem_config.base;
    Mailbox.pGrid = Mailbox.pBase + offsetof(shared_buf_t, grid);
    Mailbox.pNsteps = Mailbox.pBase + offsetof(shared_buf_t, nsteps);

	nsteps = *Mailbox.pNsteps;
	

	e_barrier_init(barriers, tgt_bars);	

	// Make sure DMA is inactive while setting descriptors
	e_dma_wait(E_DMA_0);

	//smem2local.config = E_DMA_MSGMODE | E_DMA_WORD | E_DMA_MASTER | E_DMA_ENABLE ;
	smem2local.config = E_DMA_WORD | E_DMA_MASTER | E_DMA_ENABLE ;
    smem2local.inner_stride = (0x04 << 16) | 0x04;
    smem2local.count = (0x01  << 16) | _Nelem*nrows >> 2 ;
    smem2local.outer_stride = (0x04 << 16) | 0x04; 

    //vneigh2local.config = E_DMA_MSGMODE | E_DMA_WORD | E_DMA_MASTER | E_DMA_ENABLE;
    vneigh2local.config =  E_DMA_WORD | E_DMA_MASTER | E_DMA_ENABLE;
    vneigh2local.inner_stride = (0x04 << 16) | 0x04;
    vneigh2local.count = (0x01 << 16) | _Nelem >> 2;
    vneigh2local.outer_stride = (0x04 << 16) | 0x04;

	local2smem.config = E_DMA_WORD | E_DMA_MASTER | E_DMA_ENABLE;
	local2smem.inner_stride = (0x04 << 16) | 0x04;
	local2smem.count = (0x01 << 16) | _Nelem*nrows >> 2;
	local2smem.outer_stride = 0x04 | 0x04;	

//	e_mutex_lock(0, 0, &mutex);	
	data_copy(&smem2local, &me._grid, (Mailbox.pGrid + _Nelem*nrows*me.corenum ));	
//	e_mutex_unlock(0, 0, &mutex);	

for(k=1; k<nsteps; k++){

	e_barrier(barriers, tgt_bars);
	
	horizontal_step(&me);

	cur = 1 - cur;
	e_barrier(barriers, tgt_bars);
	
	data_copy(&vneigh2local, upper_row, upper_core_grid +(nrows-1)*_Nelem);
	data_copy(&vneigh2local, lower_row, lower_core_grid);
   	
	vertical_step(upper_row, lower_row, &me);
	cur = 1 - cur;
   
#ifdef PRINT
//	e_mutex_lock(0, 0, &mutex);	
	data_copy(&local2smem, (Mailbox.pGrid + _Nelem*nrows*me.corenum), me._grid); 
//	e_mutex_unlock(0, 0, &mutex);	
#endif

	if (me.corenum == 0){
		*ready = 0x00000001;		
 		while( *go ==0) {};
			*go = 0;
	}

}   
#ifndef PRINT
	//copy last frame
	data_copy(&local2smem, (Mailbox.pGrid + _Nelem*nrows*me.corenum), me._grid); 
#endif
	return 0;
}

