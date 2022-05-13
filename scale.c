#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

/*
########################
File Variables & Scalars
########################
*/
FILE *billboards;
FILE *statues;
FILE *decals;
FILE *terrain;
FILE *tg;
FILE *flaggers;
FILE *f_temp;
double scalar_input;
double scalar;

/*
#################
Terrain Variables
#################
*/
double terrain_scale;
double min_height;
double max_height;
// Will hold the coords of each line
double coords[3];
/*
#########
Functions
#########
*/
void do_terrain(char *);
void do_billboard(char *);
void do_statue(char *);
void do_decal(char *);
void do_tg_pt_1(char *);
void do_tg_pt_2(char *);
void do_flagger(char *);

void close_temp(char *, FILE *, char *);
void reset(char *, size_t, FILE *);
int get_coords(char *, int);

// this will define which part we will do work on
int timing_gate_num = 0;

int main(int argc, char **argv) {

	//  1. Open the files
	if (argc < 2) {
		printf("\x1b[33mUsage: ./scale <new terrain.hf scale>\n\x1b[0m");
		return -1;
	}
	
	scalar_input = strtod(argv[1], NULL);
	if (!scalar_input || scalar_input <= 0 || scalar_input > 4) {
		printf("\x1b[31mError: Enter a scalar between 0 and 4\n\x1b[0m");
		return -1;
	}
	if (access("terrain.hf", F_OK) == 0) terrain = fopen("terrain.hf", "r");

	if (!terrain) {
		printf("\x1b[31mError: Need terrain.hf file for original track scale info\n\x1b[0m");
		return -1;
	}

	if (access("billboards", F_OK) == 0) billboards = fopen("billboards", "r");
	if (access("statues", F_OK) == 0) statues = fopen("statues", "r");
	if (access("decals", F_OK) == 0) decals = fopen("decals", "r");
	if (access("timing_gates", F_OK) == 0) tg = fopen("timing_gates", "r");
	if (access("flaggers", F_OK) == 0) flaggers = fopen("flaggers", "r");

	if (!billboards && !statues && !decals && !tg && !flaggers) {
		printf("\x1b[31mError: No files to scale\n\x1b[0m");
		return -1;
	}

	
	// First read info from original terrain.hf to get the track info
	char *lineptr = NULL;
	size_t s = 0;

	// get the line, pass it into the do_terrain function, reset line pointer and s
	getline(&lineptr, &s, terrain);
	do_terrain(lineptr);
	reset(lineptr, s, terrain);
	printf("\x1b[32mSucessfully resized terrain.hf!\n");
	
	// ######### BILLBOARDS ##########
	if (billboards) {
		// open temp file
		f_temp = fopen("replace.tmp", "w");

		// go through original file, rewrite to temp file, if we hit a new line just go to the next line
		while (getline(&lineptr, &s, billboards) != -1) {
			if (lineptr[0] != '[') {
				fprintf(billboards, "%s", lineptr);
				continue;
			}
			do_billboard(lineptr);
		}

		// close the temp file, renaming it to the original file name
		close_temp("billboards", f_temp, "replace.tmp");
		// reset line pointer, s, and close billboards file stream
		reset(lineptr, s, billboards);

		// print to user success
		printf("\x1b[32mSucessfully resized billboards!\n\x1b[0m");
	}

	// ######### STATUES ##########
	if (statues) {
		f_temp = fopen("replace.tmp", "w");
		while (getline(&lineptr, &s, statues) != -1) {
			if (lineptr[0] != '[') {
				fprintf(statues, "%s", lineptr);
				continue;
			}
			do_statue(lineptr);
		}
		close_temp("statues", f_temp, "replace.tmp");	
		reset(lineptr, s, statues);
		printf("\x1b[32mSucessfully resized statues!\n\x1b[0m");
	}

	// ######### DECALS ##########
	if (decals) {
		f_temp = fopen("replace.tmp", "w");
		while (getline(&lineptr, &s, decals) != -1) {
			if (lineptr[0] != '[') {
				fprintf(decals, "%s", lineptr);
				continue;
			}
			do_decal(lineptr);
		}
		close_temp("decals", f_temp, "replace.tmp");
		reset(lineptr, s, decals);
		printf("\x1b[32mSucessfully resized decals!\n\x1b[0m");
	}

	// ######### TIMING GATES ##########
	if (tg) {
		f_temp = fopen("replace.tmp", "w");
		while (getline(&lineptr, &s, tg) != -1) {

			// print the headers
			if (strcmp("startinggate:\n", lineptr) == 0) {
				fprintf(f_temp, "startinggate:\n");
				continue;
			}
			if (strcmp("checkpoints:\n", lineptr) == 0) {
				timing_gate_num = 1;
				fprintf(f_temp, "checkpoints:\n");
				continue;
			}

			// Final steps if we reached the lap order
			if (strcmp("firstlap:\n", lineptr) == 0) timing_gate_num = 2;

			if (timing_gate_num == 0) {
				do_tg_pt_1(lineptr);
			} else if (timing_gate_num == 1) {
				do_tg_pt_2(lineptr);
			} else {
				fprintf(f_temp, "%s", lineptr);
			}
		}

		close_temp("timing_gates", f_temp, "replace.tmp");
		reset(lineptr, s, tg);
		printf("\x1b[32mSucessfully resized timing gates!\n\x1b[0m");
	}

	// ######### FLAGGERS ##########
	if (flaggers) {
		f_temp = fopen("replace.tmp", "w");

		while (getline(&lineptr, &s, flaggers) != -1) {
			if (lineptr[0] != '[') {
				fprintf(f_temp, "%s", lineptr);
				continue;
			}
			do_flagger(lineptr);
		}

		close_temp("flaggers", f_temp, "replace.tmp");
		reset(lineptr, s, flaggers);
		printf("\x1b[32mSucessfully resized flaggers!\n\x1b[0m");
	}

	// Free the line pointer
	free(lineptr);

	return 0;
}

void reset(char *ptr, size_t size, FILE *file_stream) {
	ptr = NULL;
	size = 0;
	fclose(file_stream);
}

void do_terrain(char *line) {

	// First parse information into global variables
	char *stringp = line;

	char *token = strsep(&stringp, " ");
	int terrain_scale_num = atoi(token);

	token = strsep(&stringp, " ");
	terrain_scale = strtod(token, NULL);

	token = strsep(&stringp, " ");
	min_height = strtod(token, NULL);

	token = strsep(&stringp, "\0");
	max_height = strtod(token, NULL);

	// Reset the line ptr to the beginning and rewrite the information out
	stringp = line;
	scalar = scalar_input / terrain_scale;
	double new_min_height = min_height * scalar;
	double new_max_height = max_height * scalar;

	fseeko(terrain, 0, SEEK_SET);
	fprintf(terrain, "%d %f %f %f\n", terrain_scale_num, scalar_input, new_min_height, new_max_height);

}

void do_billboard(char *line) {

	char *stringp = line;

	// get coords returns the new offset in the line ptr
	int offset = get_coords(stringp, 3);

	stringp += offset;

	char *token = strsep(&stringp, " ");
	double size = strtod(token, NULL);

	// get the aspect and png, which are unchanging so we can store as a string
	char *aspect_and_png = strsep(&stringp, "\n");

	double new_x = coords[0] * scalar;
	double new_y = coords[1] * scalar;
	double new_z = coords[2] * scalar;
	double new_size = size * scalar;

	// Write to temporary file
	fprintf(f_temp, "[%f %f %f] %f %s\n", new_x, new_y, new_z, new_size, aspect_and_png);

}

void do_statue(char *line) {
	char *stringp = line;
	int offset = get_coords(stringp, 3);
	stringp += offset;

	// the only thing we need to change in the statues is the coords, so we can put the rest of the info in one string path
	char *path = strsep(&stringp, "\n");

	double new_x = coords[0] * scalar;
	double new_y = coords[1] * scalar;
	double new_z = coords[2] * scalar;

	// Write to temporary file
	fprintf(f_temp, "[%f %f %f] %s\n", new_x, new_y, new_z, path);
}

void do_decal(char *line) {
	char *stringp = line;
	int offset = get_coords(stringp, 2);
	stringp += offset;

	// unchanging angle
	char *angle = strsep(&stringp, " ");

	// parse size to double
	char *token = strsep(&stringp, " ");
	double size = strtod(token, NULL);

	// unchanging aspect and png
	char *aspect_and_png = strsep(&stringp, "\n");

	double new_x = coords[0] * scalar;
	double new_z = coords[1] * scalar;
	double new_size = size * scalar;

	// Write to temp file
	fprintf(f_temp, "[%f %f] %s %f %s\n", new_x, new_z, angle, new_size, aspect_and_png);
}

void do_tg_pt_1(char *line) {

	char *stringp = line;
	int offset = get_coords(stringp, 3);

	double new_x = coords[0] * scalar;
	double new_y = coords[1] * scalar;
	double new_z = coords[2] * scalar;

	stringp += offset;

	// set the rest of the path equal to the angle and directory stuff
	char *path = strsep(&stringp, "\n");

	// write to temp file
	fprintf(f_temp, "[%f %f %f] %s\n", new_x, new_y, new_z, path);

}

void do_tg_pt_2(char *line) {
	char *stringp = line;

	// Get size of gate
	char *token = strsep(&stringp, " ");
	double size = strtod(token, NULL);
	double new_size = size * scalar;

	// Get first set of coords
	int offset = get_coords(stringp, 3);
	double new_x1 = coords[0] * scalar;
	double new_y1 = coords[1] * scalar;
	double new_z1 = coords[2] * scalar;
	stringp += offset;

	// Get second set of coords
	offset = get_coords(stringp, 3);
	double new_x2 = coords[0] * scalar;
	double new_y2 = coords[1] * scalar;
	double new_z2 = coords[2] * scalar;

	// write to temp file
	fprintf(f_temp, "%f [%f %f %f] [%f %f %f]\n", new_size, new_x1, new_y1, new_z1, new_x2, new_y2, new_z2);
}

void do_flagger(char *line) {
	char *stringp = line;
	get_coords(stringp, 3);
	double new_x = coords[0] * scalar;
	double new_y = coords[1] * scalar;
	double new_z = coords[2] * scalar;

	// write to temp file
	fprintf(f_temp, "[%f %f %f]\n", new_x, new_y, new_z);
}

int get_coords(char *ptr, int num_of_coords) {
	char *stringp = ptr;

	// skip first bracket
	stringp++;

	// get x coord
	char *token = strsep(&stringp, " ");
	double x = strtod(token, NULL);
	coords[0] = x;
	
	// get y coord if available
	if (num_of_coords > 2) {
		token = strsep(&stringp, " ");
		double y = strtod(token, NULL);
		coords[1] = y;
	}

	// get z coord
	token = strsep(&stringp, "]");
	double z = strtod(token, NULL);

	if (num_of_coords < 3) coords[1] = z;
	else coords[2] = z;

	// skip next space
	stringp++;

	// get and return the index we finished at
	return stringp - ptr;
}

void close_temp(char *file, FILE *temp, char *temp_file_name) {
	remove(file);
	fclose(temp);
	rename(temp_file_name, file);
}