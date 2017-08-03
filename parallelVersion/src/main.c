#include <stdlib.h>
#include <stdio.h>
#include <e-hal.h>
#include "struct.h"
#include <stddef.h>
#include <sys/time.h>

e_platform_t platform;
shared_buf_t Mailbox;
e_epiphany_t dev;

char EMPTY = 0;  /* empty cell                   */
char LR = 1;   /* left-to-right moving vehicle */
char TB = 2;    /* top-to-bottom moving vehicle */
unsigned clr = (unsigned)0x00000000; 
unsigned flag = (unsigned)0x00000001; 
 
/* Returns 1 with probability p */
int rand01( float p )
{
    return ((rand() / (float)RAND_MAX) < p);
}

float timedifference_msec(struct timeval t0, struct timeval t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

void dump_cur( const char* filename, char *grid)
{
	int i, j;
    FILE *out = fopen( filename, "w" );
    if ( NULL == out ) {
        printf("Cannot create file %s\n", filename );
        abort();
    }
    fprintf(out, "P6\n");
    fprintf(out, "%d %d\n", _Nelem, _Nelem);
    fprintf(out, "255\n");
    for (i=0; i<_Nelem; i++) {
        for (j=0; j<_Nelem; j++) {
            switch( grid[i*_Nelem +j] ) {
            case 0:
             fprintf(out, "%c%c%c", 255, 255, 255);
                break;
            case 1:
               fprintf(out, "%c%c%c", 0, 0, 255);
                break;
            case 2:
               fprintf(out, "%c%c%c", 255, 0, 0);
                break;
            default:
                     printf("Error: unknown cell state %u\n", grid[i*_Nelem +j]);
                abort();
            }
        
		}
    }
    fclose(out);
}

void setup( float prob ) 
{ 
    int i, j; 
    for ( i=0; i<_Nelem; i++ ) { 
        for ( j=0; j<_Nelem; j++) { 
            if ( rand01( prob) ) { 
                Mailbox.grid[i*_Nelem+j] = ( rand01(0.5) ? LR : TB ); 
            } else { 
                Mailbox.grid[i*_Nelem+j] = EMPTY;
            }
        }
    }
}


void bml_go(e_mem_t *pDRAM, int nsteps)
{
	float elapsed;
	struct timeval stop, start;	
    unsigned int addr;
    size_t sz;
	int i,j,k;
	char buf[255];
	unsigned done = 0;
	unsigned timer = 0;

	gettimeofday(&start, NULL);      

#ifdef PRINT
	for(k=1; k<nsteps; k++){
		
		while(1){
			e_read(&dev, 0, 0, 0x7010, &done, sizeof(unsigned));
			if (done == 1){
				done =0;
				e_write(&dev, 0, 0, 0x7010, &clr, sizeof(clr));
				break;
			}
		}	
		//read data

		addr = 0;
		sz = sizeof(Mailbox);
		e_read(pDRAM, 0, 0, addr, &Mailbox, sz);
		printf("iteration %d\n",k);
		snprintf(buf, 255, "out%05d.ppm",k);
		dump_cur(buf, Mailbox.grid);
		e_write(&dev, 0, 0, 0x7000, &flag, sizeof(flag));

	}	
	gettimeofday(&stop, NULL);      
#endif

#ifndef PRINT

		while(1){
			e_read(&dev, 0, 0, 0x7010, &done, sizeof(unsigned));
			if (done == 1){
				break;
			}
		}	
		
		gettimeofday(&stop, NULL);      
		addr = 0;
		sz = sizeof(Mailbox);
		e_read(pDRAM, 0, 0, addr, &Mailbox, sz);

		snprintf(buf, 255, "out%05d.ppm",nsteps);
		dump_cur(buf, Mailbox.grid);
		e_write(&dev, 0, 0, 0x7000, &flag, sizeof(flag));
#endif

	elapsed = timedifference_msec(start, stop);
	fprintf(stderr, "it took %f with %d steps\n",elapsed, nsteps);

	e_read(&dev, 0, 0, 0x7020, &timer, sizeof(unsigned));
	fprintf(stderr, "%u clock tick\n",timer);


}

int main(int argc, char *argv[])
{
	int i,j;
	char buf[255];
    e_mem_t DRAM;
    unsigned int msize;
    unsigned int addr;
	int result;
    size_t sz;
	float prob = 0.3;
    int nsteps = 1024;
    msize = 0x00400000;
    if ( argc > 1 ) {
        nsteps = atoi(argv[1]);
    }
    if ( argc > 2 ) {
        prob = atof(argv[2]);
    }
   
	//Initalize Epiphany device
	e_init(NULL);                      
	e_reset_system();                                      //reset Epiphany
	e_get_platform_info(&platform);                          

    if ( e_alloc(&DRAM, 0x00000000, msize) )
    {
        fprintf(stderr, "\nERROR: Can't allocate Epiphany DRAM!\n");
        exit(1);
    }

	if ( e_open(&dev, 0, 0, platform.rows, platform.cols))
//	if ( e_open(&dev, 0, 0, 2,2))
    {
        fprintf(stderr, "\nERROR: Can't establish connection to Epiphany device!\n");
        exit(1);
    }
	//clear data and set parameters
	e_write(&dev, 0, 0, 0x7000, &clr, sizeof(clr));
	e_write(&dev, 0, 0, 0x7010, &clr, sizeof(clr));
	
	Mailbox.nsteps = nsteps;
	addr = offsetof(shared_buf_t, nsteps);
	sz = sizeof(Mailbox.nsteps);
	e_write(&DRAM, 0, 0, addr, (void *) &Mailbox.nsteps, sz);
	
	setup(prob);

	//write grid in shared memory	
    addr = offsetof(shared_buf_t, grid);
    sz = sizeof(Mailbox.grid);
    fprintf(stderr, "Writing Mailbox.grid[%uB] to address %08x...\n", sz, addr);
    e_write(&DRAM, 0, 0, addr, (void *) Mailbox.grid, sz);
	
	//print out first frame
	snprintf(buf, 255, "out00000.ppm");
    dump_cur(buf, Mailbox.grid);

  //Load program to cores
    fprintf(stderr, "Loading program on Epiphany chip...\n");
	result = e_load_group("e_traffic.elf", &dev, 0, 0, platform.rows, platform.cols, E_FALSE);
    //result = e_load_group("e_traffic.elf", &dev, 0, 0, 2, 2, E_FALSE);
    if (result == E_ERR) {
        fprintf(stderr, "Error loading Epiphany program.\n");
        exit(1);
    }
	// start cores
	e_start_group(&dev);

	bml_go(&DRAM, nsteps);
	
	//Close down Epiphany device
	if (e_close(&dev))
    {
        fprintf(stderr, "\nERROR: Can't close connection to Epiphany device!\n\n");
        exit(1);
    }
    if (e_free(&DRAM))
    {
        fprintf(stderr, "\nERROR: Can't release Epiphany DRAM!\n\n");
        exit(1);
    }
	e_finalize();
	return EXIT_SUCCESS;
}

