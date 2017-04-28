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

void horizontal_step(char *firstColumn,char *lastColumn, core_t *me)
{
	int i, j;
    const int next = 1 - cur;

	//update first column
	for (i=0; i<_Nside; i++)
	{
		if (me->_grid[cur][i*_Nside] == EMPTY && firstColumn[i] == LR ) {
			me->_grid[next][i*_Nside] = LR;
        }
        else {
			if ( me->_grid[cur][i*_Nside] == LR && me->_grid[cur][i*_Nside+1] == EMPTY ) {
				me->_grid[next][i*_Nside] = EMPTY;
			} else
				me->_grid[next][i*_Nside] = me->_grid[cur][i*_Nside];
		}
	}
	
	
	for (i=0; i<_Nside; i++) {
		for (j=1; j<_Nside-1; j++) {
			const int left = j-1;
			const int right = j+1;
            if ( me->_grid[cur][i*_Nside+j] == EMPTY && me->_grid[cur][i*_Nside+left] == LR ) {
                me->_grid[next][i*_Nside+j] = LR;
            }
            else
			{
           		if (me->_grid[cur][i*_Nside+j] == LR && me->_grid[cur][i*_Nside+right] == EMPTY) {
					me->_grid[next][i*_Nside+j] = EMPTY;
                } else
 					me->_grid[next][i*_Nside+j] = me->_grid[cur][i*_Nside+j];
            }
        }
    }

	//update last column
	j=_Nside-1;	
	for (i=0; i<_Nside; i++)
    {
        if (me->_grid[cur][i*_Nside+j ] == EMPTY && me->_grid[cur][i*_Nside+j-1] == LR ) {
            me->_grid[next][i*_Nside+j] = LR;
        }
        else {
            if ( me->_grid[cur][i*_Nside+j] == LR && lastColumn[i] == EMPTY ) {
                me->_grid[next][i*_Nside+j] = EMPTY;
            } else
                me->_grid[next][i*_Nside+j] = me->_grid[cur][i*_Nside+j];
        }
    }


}

void vertical_step( char *firstRow, char *lastRow, core_t *me)
{
    int i, j;
    const int next = 1 - cur;
	//update first row
	for(j=0; j<_Nside; j++)
	{
		if (me->_grid[cur][j] == EMPTY && firstRow[j] == TB )
		{
			me->_grid[next][j] = TB;
		}
		else {
			if ( me->_grid[cur][j] == TB && me->_grid[cur][1*_Nside+j] == EMPTY) {
				me->_grid[next][j] = EMPTY;
           	} else
 				me->_grid[next][j] = me->_grid[cur][j];
        }
	}
    
	for (i=1; i<_Nside-1; i++) {
        const int top = i-1;
        const int bottom = i+1;
        for (j=0; j<_Nside; j++)
		{
			if (me->_grid[cur][i*_Nside+j] == EMPTY && me->_grid[cur][top*_Nside+j] == TB ) {
                    me->_grid[next][i*_Nside+j] = TB;
            }
            else {
                  if (me->_grid[cur][i*_Nside+j] == TB && me->_grid[cur][bottom*_Nside+j] == EMPTY) {
                    me->_grid[next][i*_Nside+j] = EMPTY;
                  } else {
                        me->_grid[next][i*_Nside+j] = me->_grid[cur][i*_Nside+j];
					}
            }
        }
    }
	//update last row
	i=_Nside-1;
	for(j=0; j<_Nside; j++)
	{
		if (me->_grid[cur][i*_Nside+j] == EMPTY && me->_grid[cur][(i-1)*_Nside+j] == TB )
		{
			me->_grid[next][i*_Nside+j] = TB;
		}
		else {
			if ( me->_grid[cur][i*_Nside+j] == TB && lastRow[j] == EMPTY) {
				me->_grid[next][i*_Nside+j] = EMPTY;
           	} else
 				me->_grid[next][i*_Nside+j] = me->_grid[cur][i*_Nside+j];
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
    e_dma_desc_t hneigh2local ;
    e_dma_desc_t vneigh2local ;
    e_dma_desc_t local2smem ;
    volatile e_barrier_t  barriers[_Ncores];
    volatile e_barrier_t *tgt_bars[_Ncores];
    int i,j,k;
	volatile unsigned *go, *ready; 
    char upper_row[_Nside];
    char lower_row[_Nside];
    char left_column[_Nside];
    char right_column[_Nside];
	e_mutex_t mutex = MUTEX_NULL; 
	char *left_core_grid;
	char *right_core_grid;
	char *upper_core_grid;
	char *lower_core_grid;
	
	//initialization
	go = (unsigned *) 0x7000;//Done flag
	ready = (unsigned *) 0x7010;//ready flag to signal end of computation

    me.coreID = e_get_coreid();
    me.row = e_group_config.core_row;
    me.col = e_group_config.core_col;
    me.corenum = me.row * e_group_config.group_cols + me.col;
    e_neighbor_id(E_PREV_CORE, E_ROW_WRAP, &me.rowh, &me.colh);
    e_neighbor_id(E_NEXT_CORE, E_ROW_WRAP, &me.rowhnext, &me.colhnext);
    e_neighbor_id(E_PREV_CORE, E_COL_WRAP, &me.rowv, &me.colv);
	e_neighbor_id(E_NEXT_CORE, E_COL_WRAP, &me.rowvnext, &me.colvnext);
    
	Mailbox.pBase = (void *) e_emem_config.base;
    Mailbox.pGrid = Mailbox.pBase + offsetof(shared_buf_t, grid);
    Mailbox.pNsteps = Mailbox.pBase + offsetof(shared_buf_t, nsteps);

	nsteps = *Mailbox.pNsteps;
	
	left_core_grid = e_get_global_address(me.rowh, me.colh, &me._grid[0][0]);
	right_core_grid = e_get_global_address(me.rowhnext, me.colhnext, &me._grid[0][0]);
	upper_core_grid =e_get_global_address(me.rowv, me.colv, &me._grid[1][0]);
	lower_core_grid =e_get_global_address(me.rowvnext, me.colvnext, &me._grid[1][0]);
	
	e_barrier_init(barriers, tgt_bars);	

	// Make sure DMA is inactive while setting descriptors
	e_dma_wait(E_DMA_0);

	smem2local.config = E_DMA_MSGMODE | E_DMA_WORD | E_DMA_MASTER | E_DMA_ENABLE;
    smem2local.inner_stride = (0x04 << 16) | 0x04;
    smem2local.count = (_Nside << 16) | _Nside >> 2 ;
    smem2local.outer_stride = (0x04 << 16) | (((_Nelem - _Nside) * sizeof(char)) + 0x04); 

	hneigh2local.config = E_DMA_MSGMODE | E_DMA_BYTE | E_DMA_MASTER | E_DMA_ENABLE;
    hneigh2local.inner_stride = (0x01 << 16) | 0x01;
    hneigh2local.count = (_Nside << 16) | 0x01;
    hneigh2local.outer_stride = (0x01 << 16) | _Nside;

    vneigh2local.config = E_DMA_MSGMODE | E_DMA_WORD | E_DMA_MASTER | E_DMA_ENABLE;
    vneigh2local.inner_stride = (0x04 << 16) | 0x04;
    vneigh2local.count = (0x01 << 16) | 0x0010;
    vneigh2local.outer_stride = (0x04 << 16) | 0x04;

	local2smem.config = E_DMA_MSGMODE | E_DMA_WORD | E_DMA_MASTER | E_DMA_ENABLE;
	local2smem.inner_stride = (0x04 << 16) | 0x04;
	local2smem.count = (_Nside << 16) | _Nside >> 2;
	local2smem.outer_stride = ( (((_Nelem - _Nside) * sizeof(char)) + 0x04) << 16) | 0x04;	

//	e_mutex_lock(0, 0, &mutex);	
	data_copy(&smem2local, &me._grid, (Mailbox.pGrid + _Nelem*_Nside*me.row  + _Nside*me.col));	
//	e_mutex_unlock(0, 0, &mutex);	
	e_barrier(barriers, tgt_bars);


for(k=1; k<nsteps; k++){

	e_barrier(barriers, tgt_bars);
	
	data_copy(&hneigh2local, left_column, left_core_grid+_Nside-1);
	data_copy(&hneigh2local, right_column, right_core_grid);
    	
	horizontal_step(left_column, right_column, &me);

	cur = 1 - cur;
	e_barrier(barriers, tgt_bars);
	
	data_copy(&vneigh2local, upper_row, upper_core_grid +(_Nside-1)*_Nside);
	data_copy(&vneigh2local, lower_row, lower_core_grid);
   	
	vertical_step(upper_row, lower_row, &me);
	cur = 1 - cur;
   
#ifdef PRINT
//	e_mutex_lock(0, 0, &mutex);	
	data_copy(&local2smem, (Mailbox.pGrid + _Nelem*_Nside*me.row  + _Nside*me.col), me._grid); 
//	e_mutex_unlock(0, 0, &mutex);	
#endif

	e_barrier(barriers, tgt_bars);

	if (me.corenum == 0){
		*ready = 0x00000001;		
 		while( *go ==0) {};
			*go = 0;
	}
}   
#ifndef PRINT
	//copy last frame
	data_copy(&local2smem, (Mailbox.pGrid + _Nelem*_Nside*me.row  + _Nside*me.col), me._grid); 
#endif
	return 0;
}


