
// The OpenGL libraries, make sure to include the GLUT and OpenGL frameworks
#ifdef WINDOWS
#include <gl/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#else
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#endif

#include <math.h>
#include "glm/glm.hpp"
#include "glm/gtx/random.hpp"
#include "glm/gtc/type_precision.hpp"
#include "glm/gtc/type_ptr.hpp"

using glm::i32vec2;
using glm::i32;

using glm::vec2;
using glm::vec3;

vec3 woodcolor;
static void setwood(vec3 col)
{
	woodcolor = col;
	glClearColor(woodcolor.x, woodcolor.y, woodcolor.z, 0.0);
}

i32vec2 woodsize = i32vec2(1000, 500);
//i32vec2 woodsize = i32vec2(800, 600);

i32 millrad = 6;

int block_count = 0;

i32vec2 *blocksize;
i32vec2 *blockpos;
int *blockdone;

i32vec2 *drilling_path;
int drillpoint_count = 0;
int slowness = 3;

static void output_gcode()
{
	printf("outputting gcode..\n");

	FILE *f = fopen("drillpath.gcode", "wt");
	if (!f)
	{
		printf("can't write!\n");
		return;
	}


	fprintf(f, "H DX=%i DY=%i DZ=16 -A C=0 T=0 R=1 *MM /\"def\" BX=0  BY=0  BZ=6\n",woodsize.x, woodsize.y);
	int i;

	if (drillpoint_count % 5 != 0)
	{
		printf("PROBLEM! drillpoint_count is not divisable by 5!\n");
		fclose(f);
		return;
	}

	for (i = 0; i < drillpoint_count; i++)
	{
		if (i % 5 == 0)
			fprintf(f, "XG0 X=%i Y=%i Z=16.2 E=2# T=107 C=0\n", drilling_path[i].x, drilling_path[i].y);
		else // this is never executed if i == 0
		{
			bool xdiffer = drilling_path[i].x != drilling_path[i - 1].x;
			bool ydiffer = drilling_path[i].y != drilling_path[i - 1].y;
			if (xdiffer && ydiffer)
				fprintf(f, "G1 X=%i Y=%i\n", drilling_path[i].x, drilling_path[i].y);
			else if (ydiffer)
				fprintf(f, "G1 Y=%i\n", drilling_path[i].y);
			else if (xdiffer)
				fprintf(f, "G1 X=%i\n", drilling_path[i].x);
		}
	}
	fclose(f);
//	printf("XG0 X=0 Y=-6 Z=16.2 E=2# T=107 C=0
}

static void output_remaining()
{
	int i;
	int donestat = 0;

	FILE *f = fopen("remaining.set", "wt");
	if (!f)
		return;

	fprintf(f, "WOOD_SIZE=%d,%d\n", woodsize.x, woodsize.y);
	fprintf(f, "SLOWNESS=%d\n", slowness);

	for (i = 0; i < block_count; i++)
	{
		if (blockdone[i])
			donestat++;
		else
			fprintf(f, "BLOCK=%d,%d\n", blocksize[i].x, blocksize[i].y);
	}
	fclose(f);
	printf("%i/%i done.\n", donestat, block_count);

}

static void init_blocks(int bc)
{
	printf("initting %i blocks.\n", bc);

	blocksize = new i32vec2[bc];
	blockpos = new i32vec2[bc];
	blockdone = new int[bc];

	drilling_path = new i32vec2[bc * 5];

	int i;
	block_count = bc;

	for (i = 0; i < block_count; i++)
	{
		blockpos[i].x = woodsize.x;
		blockpos[i].y = 0;
		blockdone[i] = 0;
	}
}

static void randomize_blocks()
{
	int i;
	for (i = 0; i < block_count; i++)
	{
		i32vec2 basesize = i32vec2(rand() & 255, rand() & 255);

		blocksize[i].x = (basesize.x / 16) * 9 + 10;
		blocksize[i].y = (basesize.y / 16) * 9 + 10;
	}
}

static inline i32 area_func(i32vec2 dimensions)
{
	int area = dimensions.x * dimensions.y;
	/*int biggest_axel = dimensions.x;
	if (dimensions.y > biggest_axel)
		biggest_axel = dimensions.y;*/

	//return dimensions.x;

	//return biggest_axel;
	return area;
}

void sort_blocks()
{
	int i;

	for (i = 0; i < block_count; i++)
	{
		if (blocksize[i].y > blocksize[i].x)
		{
			i32 tmp_dim = blocksize[i].x;
			blocksize[i].x = blocksize[i].y;
			blocksize[i].y = tmp_dim;
		}
	}
}

static void validate_blocks()
{
	int i;

	i32 total_area = 0;
	i32 total_area_mill = 0;

	i32 wood_area = area_func(woodsize);

	printf("wood is %i,%i. area %i\n", woodsize.x, woodsize.y, wood_area);

	for (i = 0; i < block_count; i++)
	{
		total_area += area_func(blocksize[i]);
		total_area_mill += area_func(blocksize[i] + i32vec2(millrad, millrad) * 2);
		if (blocksize[i].x > woodsize.x)
			printf("block %i is too wide. (%i)\n", i, blocksize[i].x);

		if (blocksize[i].y > woodsize.y)
			printf("block %i is too tall. (%i)\n", i, blocksize[i].y);
	}

	printf("total area of blocks is %i. about %i with tolerances.\n", total_area, total_area_mill);
}

static inline bool test_blocks(int b1, i32vec2 b2_tl, i32vec2 b2_br)
{
	i32vec2 b1_tl = blockpos[b1];
	i32vec2 b1_br = blockpos[b1] + blocksize[b1] - i32vec2(1, 1);

	// 111       111   11111    111
	//   222   222      222    22222

	b1_tl -= i32vec2(millrad);
	b1_br += i32vec2(millrad);
	b2_tl -= i32vec2(millrad);
	b2_br += i32vec2(millrad);

	return !(b2_tl.x > b1_br.x || b2_br.x < b1_tl.x || b2_tl.y > b1_br.y || b2_br.y < b1_tl.y);
}

static inline int find_lowest_x(int bn, int y)
{
	i32vec2 thissize = blocksize[bn];
	i32 x;
	int i;
	for (x = woodsize.x - blocksize[bn].x - 1; x >= 0; x--)
	{
		i32vec2 b2_tl = i32vec2(x, y);
		i32vec2 b2_br = b2_tl + blocksize[bn] - i32vec2(1, 1);
		for (i = 0; i < block_count; i++)
		{
			if (blockdone[i] == 2 && test_blocks(i, b2_tl, b2_br))
			{
				return x + 1;
			}
		}
	}

	// at this point, we always return 0.
	return x + 1;
}

static bool finished_flag;

static inline void search_iterate()
{
	static int state = 0;

	static int block_i_meta;

	static int block_i;
	static i32 block_y;
	static i32 block_area;

	static i32 lowest_block_x;
	static i32 lowest_block_xr;
	static i32 lowest_block_y;
	static int lowest_block_i;
	static i32 lowest_block_area;
	static bool iterdir = true;
	int i;

	if (state == 0)
	{
		lowest_block_x = woodsize.x;
		lowest_block_xr = woodsize.x;
		lowest_block_area = 0;
		block_i = -1;
		block_i_meta = -1;
		iterdir = true;
		state = 2;
		return;
	}
	else if (state == 1)
	{
		i32 block_x = find_lowest_x(block_i, block_y);

#ifdef CAREFUL_PLACING
		if (block_x == (woodsize.x - blocksize[block_i].x))
		{
			blockdone[block_i] = 0;
			state = 5;
			return;
		}
#endif

		i32 block_xr = block_x + blocksize[block_i].x;

/*		if (	(lowest_block_xr > block_xr)
				//|| ((lowest_block_x == block_x)
				//&& (lowest_block_area < block_area))
		)*/
		if (	(lowest_block_x > block_x)
				|| ((lowest_block_x == block_x)
				&& (lowest_block_area < block_area))
		)
		{
			lowest_block_x = block_x;
			lowest_block_xr = block_xr;
			lowest_block_y = block_y;
			lowest_block_i = block_i;
			lowest_block_area = block_area;
		}

		blockpos[block_i].x = block_x;
		blockpos[block_i].y = block_y;

		if (iterdir)
			block_y++;
		else
			block_y--;

		if (block_y > (woodsize.y - blocksize[block_i].y - 1) || block_y < 0)
		{
			blockdone[block_i] = 0;
			state = 2;
			return;
		}
	}
	else if (state == 2)
	{
		bool all_done = true;
		for (i = 0; i < block_count; i++)
		{
			if (blockdone[i] != 2)
				all_done = false;
		}

		if (all_done)
		{
			state = 6;
			return;
		}

		while(1)
		{
			//block_i++;

			//if (block_i >= block_count)
			if (block_i_meta >= slowness)
			{
				// if even the lowest xr is over, no blocks can fit
				if (lowest_block_xr >= woodsize.x)
				{
					state = 5;
					return;
				}

				blockdone[lowest_block_i] = 2;
				blockpos[lowest_block_i].x = lowest_block_x;
				blockpos[lowest_block_i].y = lowest_block_y;

				i32vec2 dp_lu = blockpos[lowest_block_i] - millrad;
				i32vec2 dp_rd = blockpos[lowest_block_i] + blocksize[lowest_block_i] + millrad;

				drilling_path[drillpoint_count++] = i32vec2(dp_lu.x, dp_lu.y);
				drilling_path[drillpoint_count++] = i32vec2(dp_rd.x, dp_lu.y);
				drilling_path[drillpoint_count++] = i32vec2(dp_rd.x, dp_rd.y);
				drilling_path[drillpoint_count++] = i32vec2(dp_lu.x, dp_rd.y);
				drilling_path[drillpoint_count++] = i32vec2(dp_lu.x, dp_lu.y);

				state = 0;
				return;
			}

			block_i = rand() % block_count;

			if (blockdone[block_i] == 0)
			{
				if (iterdir)
					block_y = 0;
				else
					block_y = woodsize.y - blocksize[block_i].y - 1;
				blockdone[block_i] = 1;
				block_area = area_func(blocksize[block_i]);

				//printf("this block is %i. area %i\n", block_i, block_area);
				block_i_meta++;

				state = 1;
				return;
			}

		}
	}
	else if (state == 3)
	{
		printf("finished.\n");
		output_gcode();
		output_remaining();
		state = 4;
		finished_flag = true;
	}
	else if (state == 4)
	{
	}
	else if (state == 5)
	{
		setwood(vec3(1.0f, 0.f, 0.f));

		state = 3;
		return;
	}
	else if (state == 6)
	{
		setwood(vec3(0.f, 1.f, 0.f));

		state = 3;
		return;
	}

}


static void init() // Called before main loop to set up the program
{
	setwood(vec3(0.6f, 0.3f, 0.0f));
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	//glShadeModel(GL_SMOOTH);
}

static inline float fracf(float luku)
{
	return luku - floorf(luku);
}

int drillpoint_draw_count = 0;
static void draw_blocks()
{
	int i;

	glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

	glBegin(GL_QUADS);

	for (i = 1; i < drillpoint_draw_count; i++)
	{
		if (i % 5 == 0)
			i++;
	/*
	1     1
	 2   2
	*/
	 	vec3 tmpcol = woodcolor - (float)(drillpoint_draw_count - i) * vec3(0.1f);
	 	if (drillpoint_draw_count == drillpoint_count)
	 		tmpcol = vec3(0.0f);

		glColor3fv(glm::value_ptr(tmpcol));

		i32vec2 dp_lu = drilling_path[i - 1];
		i32vec2 dp_rd = drilling_path[i];
		i32vec2 tmp;
		if (dp_rd.x < dp_lu.x || dp_rd.y < dp_lu.y)
		{
			tmp = dp_lu;
			dp_lu = dp_rd;
			dp_rd = tmp;
		}
		dp_lu -= i32vec2(millrad);
		dp_rd += i32vec2(millrad);

		glVertex2f(dp_lu.x, dp_lu.y);
		glVertex2f(dp_rd.x, dp_lu.y);
		glVertex2f(dp_rd.x, dp_rd.y);
		glVertex2f(dp_lu.x, dp_rd.y);
	}

	glEnd();

	for (i = 0; i < block_count; i++)
	{
		if (!blockdone[i])
			continue;

		if (blockdone[i] == 1)
			glColor3f(0.6f, 0.6f, 0.6f);
		else
			glColor3f(1.0f, 1.0f, 1.0f);


		i32vec2 thispos = blockpos[i];
		i32vec2 thissize = blocksize[i];

		glLoadIdentity();
		glTranslatef(thispos.x, thispos.y, 0.0);

		glBegin(GL_QUADS);
		glVertex2f(0.0, 0.0);
		glVertex2f(thissize.x, 0.0);
		glVertex2f(thissize.x, thissize.y);
		glVertex2f(0.0, thissize.y);
		glEnd();
	}

/*
	if (drillpoint_draw_count > 1 && drillpoint_draw_count % 5 == 1)
	{
		glLineWidth(5);
		glBegin(GL_LINES);
		glColor3f(1.0f, 0.0f, 0.0f);
		i32vec2 p1 = drilling_path[drillpoint_draw_count - 2];
		i32vec2 p2 = drilling_path[drillpoint_draw_count - 1];
		glVertex2f(p1.x, p1.y);
		glVertex2f(p2.x, p2.y);
		glEnd();
	}
*/
}

int lasttime = 0;
int dp_lasttime = 0;

#define ITERATIONS_PER_FRAME (1000)

static void display()
{
	int thistime = glutGet(GLUT_ELAPSED_TIME);
	search_iterate();
	if ((thistime - lasttime) < 100)
		return;

	if (drillpoint_draw_count < drillpoint_count)
	{
		while((thistime - dp_lasttime) > 100)
		{
			drillpoint_draw_count += (drillpoint_count - drillpoint_draw_count) / 20 + 1;
			dp_lasttime += 100;
		}
	}
	else
		dp_lasttime = thistime;

	lasttime = thistime;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//	int i;
//	for (i = 0; i < ITERATIONS_PER_FRAME; i++)
//		search_iterate();

	draw_blocks();

	glutSwapBuffers();
}

// Called every time a window is resized to resize the projection matrix
void reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, woodsize.x, woodsize.y, 0.0, -1.0, 1.0);
//    glFrustum(-0.1, 0.1, -(float)h/(10.0*(float)w), (float)h/(10.0f*(float)w), 0.2f, 9999999.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void keyboard(unsigned char key, int x, int y)
{
	exit(0);
}

static bool read_config(char *fn)
{
	int mode = 0;
	int blockcounter = 0;

	if (!fn)
		return false;

	FILE *f = fopen(fn, "rt");
	if (!f)
		return false;

	for (mode = 0; mode < 2; mode++)
	{
		fseek(f, 0, SEEK_SET);
		blockcounter = 0;

		while(!feof(f))
		{
			char tmp_str[1024];
			char *value;
			char *key;

			fgets(tmp_str, 1024, f);

			char *end = strchr(tmp_str,'\r');
			if (end)
				*end = 0;
			end = strchr(tmp_str,'\n');
			if (end)
				*end = 0;

			char *sep = strchr(tmp_str,'=');
			if (!sep)
				continue;

			*sep = 0;
			key = tmp_str;
			value = sep + 1;

			printf("key '%s' is '%s'\n", key, value);

			if (!strcmp(key, "WOOD_SIZE"))
				sscanf(value, "%d , %d", &(woodsize.x), &(woodsize.y));
			else if (!strcmp(key, "SLOWNESS"))
				sscanf(value, "%d", &slowness);
			else if (!strcmp(key, "BLOCK"))
			{
				if (mode == 1)
					sscanf(value, "%d , %d", &(blocksize[blockcounter].x), &(blocksize[blockcounter].y));

				blockcounter++;
			}
		}

		if (mode == 0)
		{
			if (blockcounter > 0)
				init_blocks(blockcounter);
			else
			{
				fclose(f);
				return false;
			}
		}
	}
	

	fclose(f);

	return true;
}

static void setup(char *fn)
{
	if (fn)
	{
		if (!read_config(fn))
		{
			// show box!
			exit(1);
		}
	}
	else
	{
		init_blocks(40);
		randomize_blocks();
	}

	sort_blocks();
	validate_blocks();

	printf("slowness parameter is %i\n", slowness);
}

int main(int argc, char **argv)
{
	glutInit(&argc, argv); // initializes glut

	srand(time(NULL));

	if (argc > 1)
	{
		setup(argv[1]);
	}
	else
		setup(NULL);

	// Sets up a double buffer with RGBA components and a depth component
	glutInitDisplayMode(GLUT_RGBA);

	int window_w = 1100;
	int window_h = (int)((float)woodsize.y * (float)window_w / (float)woodsize.x);

	if (window_h > 700)
	{
		window_h = 700;
		window_w = (int)((float)woodsize.x * (float)window_h / (float)woodsize.y);
	}

	glutInitWindowSize(window_w, window_h);

	// Sets the window position to the upper left
	glutInitWindowPosition(0, 0);

	// Creates a window using internal glut functionality
	glutCreateWindow("Super Woodslipper 2000 Turbo");

	// passes reshape and display functions to the OpenGL machine for callback
	glutReshapeFunc(reshape);
	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutKeyboardFunc(keyboard);

	init();

	// Starts the program.
	glutMainLoop();
	return 0;
}
