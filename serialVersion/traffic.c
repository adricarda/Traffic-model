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

void horizontal_step( )
{
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
void vertical_step( void )
{
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
int rand01( float p )
{
    return ((rand() / (float)RAND_MAX) < p);
}

/* Randomly initialize grid[cur][][] with vehicles with density p.
   For each vehicle, its direction (left-to-right or top-to-bottom) is
   chosen with equal probability. */
void setup( float p )
{
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
