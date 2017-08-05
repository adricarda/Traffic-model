/****************************************************************************
 *
 * traffic.c - Cellular Automata traffic simulator
 *
 * Written in 2017 by Moreno Marzolla <moreno.marzolla(at)unibo.it>
 *
 * To the extent possible under law, the author(s) have dedicated all 
 * copyright and related and neighboring rights to this software to the 
 * public domain worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication
 * along with this software. If not, see 
 * <http://creativecommons.org/publicdomain/zero/1.0/>. 
 *
 * ---------------------------------------------------------------------------
 *
 * This program implements the Biham-Middleton-Levine traffic model
 * (see
 * https://en.wikipedia.org/wiki/Biham%E2%80%93Middleton%E2%80%93Levine_traffic_model
 * for details). THe BML traffic model is a simple three-state
 * two-dimensional cellular automata over a toroidal square space.
 * Initially, each cell is either empty, or occupied by a
 * left-to-right or top-to-bottom moving vehicle. The model evolves at
 * discrete time steps. At each step, vehicles move one position left
 * or down (depending on the type of vehicle) provided that the
 * destination cell is empty. Each step is logically divided into two
 * phases: in the first phase only left-to-right vehicles move;
 * in the second phase, only top-to-bottom vehicles move. If the
 * inital density of vehicles is large enough, a giant traffic jam may
 * appear, preventing any further movement. For low initial density,
 * the system self-organize to allow a constant flow of vehicles.
 *
 * Compile with:
 *
 * gcc -fopenmp -O2 -std=c99 -Wall -Wpedantic traffic.c -o traffic
 *
 * Run with:
 *
 * ./traffic [nsteps [prob]]
 *
 * Example:
 *
 * ./traffic 1024 0.3
 * 
 * where nsteps is the number of simulation steps to execute, and prob
 * is the initial probability that a cell is occupied by a vehicle
 * (occupied cells are initialized with equal probability with a
 * top-down or left-right vehicle). Default: nsteps=256 prob=0.2
 *
 * For each step, a file called outNNNNN.ppm is generated, where NNNNN
 * is the step number (starts from 0). At the end of the simulation,
 * it is possible to display a movie with:
 *
 * animate -resize 512x512 *.ppm
 *
 * To make a movie with avconv (ex ffmpeg):
 *
 * avconv -i "out%05d.ppm" -vcodec mpeg4 -s 512x512 traffic.avi
 *
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <sys/time.h>


#define SIZE 128
struct timeval stop, start;

/* Possible values stored in a grid cell */

char  TB = 2;      /* top-to-bottom moving vehicle */
char  LR = 1;     /* left-to-right moving vehicle */
char  EMPTY = 0;  /* empty cell                   */

int cur = 0;

unsigned char grid[2][SIZE][SIZE];

float timedifference_msec(struct timeval t0, struct timeval t1)
{
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

/* Move all left-to-right vehicles that are not blocked */

void horizontal_step( ){
	int i, j;
	const int next = 1 - cur;
	for (i=0; i<SIZE; i++) {
		for (j=0; j<SIZE; j++) {
			const int left = (j > 0 ? j-1 : SIZE-1);
			const int right = (j < SIZE-1 ? j+1 : 0);
			if ( grid[cur][i][j] == EMPTY && grid[cur][i][left] == LR ) {
				grid[next][i][j] = LR;
            } else {
				if ( grid[cur][i][j] == LR && grid[cur][i][right] == EMPTY ) {
					grid[next][i][j] = EMPTY;
				} else {
					grid[next][i][j] = grid[cur][i][j];
				}
			}
		}
    }
}

/* Move all top-to-bottom vehicles that are not blocked */
void vertical_step( void ){
	int i, j;
	const int next = 1 - cur;
	for (i=0; i<SIZE; i++) {
		const int top = (i > 0 ? i-1 : SIZE-1);
		const int bottom = (i < SIZE-1 ? i+1 : 0);
		for (j=0; j<SIZE; j++) {
			if ( grid[cur][i][j] == EMPTY && grid[cur][top][j] == TB ) {
				grid[next][i][j] = TB;
			} else {
				if ( grid[cur][i][j] == TB && grid[cur][bottom][j] == EMPTY) {
					grid[next][i][j] = EMPTY;
				} else {
					grid[next][i][j] = grid[cur][i][j];
				}
			}
		}
	}
}

/* Returns 1 with probability p */
int rand01( float p ){
	return ((rand() / (float)RAND_MAX) < p);
}

void setup( float p ){
	int i, j;
	cur = 0;
	for ( i=0; i<SIZE; i++ ) {
		for ( j=0; j<SIZE; j++) {
			if ( rand01( p ) ) {
				grid[cur][i][j] = ( rand01(0.5) ? LR : TB );
			} else {
				grid[cur][i][j] = EMPTY;
			}
		}
	}
}

/* Dump grid[cur][][] as a PPM (Portable PixMap) image written to file
   |filename|. Left-to-right vehicles are shown as blue pixels, while
   top-to-bottom vehicles are shown in red. Empty cells are white. */

void dump_cur( const char* filename )
{
	int i, j;
	FILE *out = fopen( filename, "w" );
	if ( NULL == out ) {
		printf("Cannot create file %s\n", filename );
		abort();
	}
	fprintf(out, "P6\n");
	fprintf(out, "%d %d\n", SIZE, SIZE);
	fprintf(out, "255\n");

	for (i=0; i<SIZE; i++) {
 		for (j=0; j<SIZE; j++) {
 			switch( grid[cur][i][j] ) {
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
				printf("Error: unknown cell state %u\n", grid[cur][i][j]);
				abort();
			}
		}
	}
	fclose(out);
}

int main( int argc, char* argv[] ){
	float elapsed;
	const int buflen = 255;
	int s;
	char buf[buflen];
 	float prob = 0.3;
	int nsteps = 1024;
	if ( argc > 1 ) {
		nsteps = atoi(argv[1]);
	}
	if ( argc > 2 ) {
		prob = atof(argv[2]);
	}
	setup(prob);
	snprintf(buf, buflen, "out00000.ppm");
	dump_cur(buf);
	gettimeofday(&start, NULL);
	for (s=1; s<nsteps; s++) {
		horizontal_step();
		cur = 1 - cur;
		vertical_step();
		cur = 1 - cur;
		snprintf(buf, buflen, "out%05d.ppm",s);
		//dump_cur(buf);
	}
	gettimeofday(&stop, NULL);
	//snprintf(buf, buflen, "out%05d.ppm",nsteps);
	dump_cur(buf);
	elapsed = timedifference_msec(start, stop);	
	fprintf(stderr, "it took %f\n",elapsed);
	return 0;
}
