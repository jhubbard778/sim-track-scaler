#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>

#define NUM_FILES 7

/*
########################
	FILE Variables
########################
*/

char* filenames[NUM_FILES] = {"terrain.hf", "billboards", "statues", "decals", "timing_gates", "flaggers", "edinfo"};
FILE *trackfiles[NUM_FILES];
FILE *f_temp;

/*
#################
	Scalars
#################
*/
double new_terrain_scale;
double multiplier;

/*
#################
Terrain Variables
#################
*/
double terrain_scale;
double min_height;
double max_height;

// Will hold the [x,y,z] coordinates of each decal/statue/billboard/timing gate
double coords[3];

/*
#########################
	SubFolder Variables
#########################
*/

char *prev_folder_directory;
char *original_folder_directory;

/*
################################
		   Functions
################################
*/
// Scale Functions
int do_terrain_work(char *);
void scale_terrain(char *, char *);
void do_billboard(char *);
void do_statue(char *);
void scale_statues_and_billboards(char *, char *);
void scale_decal(char *);
void scale_tg_pt_1(char *);
void scale_tg_pt_2(char *);
void scale_flaggers(char *);
int get_coords(char *, int);

// Folder Functions
char* validate_folder_path(char *);
char* subfolder_exists(char *, char *);

// File Functions
void move_files(char *, FILE *, char *);
void reset(char *, size_t, FILE *);

int main(int argc, char **argv) {

	//  1. Validate User Input
	if (argc < 3) {
		printf("\x1b[33mUsage: ./scale <folder path> <new terrain.hf scale>\n\x1b[0m");
		return -1;
	}
 	
	new_terrain_scale = strtod(argv[2], NULL);
	if (!new_terrain_scale || new_terrain_scale <= 0) {
		printf("\x1b[31mError: Enter a terrain.hf scale greater than 0\x1b[0m\n");
		return -1;
	}

	char* folder_path = validate_folder_path(argv[1]);
	if (folder_path == NULL) {
		printf("\x1b[31mError: Folder allocation failed or path does not exist!\x1b[0m\n");
		return -1;
	}

	// 2. Check if the original and previous files subfolder exists
	original_folder_directory = subfolder_exists(folder_path, "original files");
	prev_folder_directory = subfolder_exists(folder_path, "previous files");

	// 3. Read in terrain and scale
	int ret = do_terrain_work(folder_path);
	if (ret == -1) {
		if (original_folder_directory != NULL) free(original_folder_directory);
		if (prev_folder_directory != NULL) free(prev_folder_directory);
		return -1;
	}

	char *lineptr = NULL;
	char *current_filepath = NULL;
	size_t s = 0;

	// Go through all the files
	for (int i = 1; i < NUM_FILES; i++) {

		// get the current filepath to the file
		current_filepath = realloc(current_filepath, strlen(folder_path) + strlen(filenames[i]) + 2);
		strcpy(current_filepath, folder_path);
		strcat(current_filepath, "/");
		strcat(current_filepath, filenames[i]);

		// if the file doesn't exist go to next file
		if (access(current_filepath, F_OK) != 0) {
			continue;
		}

		trackfiles[i] = fopen(current_filepath, "r");

		// open the temporary file
		f_temp = fopen("replace.tmp", "w");
		
		if (strcmp(filenames[i], "billboards") == 0 || strcmp(filenames[i], "statues") == 0) {
			printf("Doing %s work\n", filenames[i]);
			while (getline(&lineptr, &s, trackfiles[i]) != -1) {
				if (lineptr[0] != '[') {
					continue;
				}
				scale_statues_and_billboards(lineptr, filenames[i]);
			}
		}

		move_files(current_filepath, f_temp, "replace.tmp");
		reset(lineptr, s, trackfiles[i]);
		printf("\x1b[32mSucessfully resized %s!\n\x1b[0m", filenames[i]);
	}

	free(current_filepath);
	free(folder_path);

	/*
	
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
		move_files(folder_path + "/billboards", f_temp, "replace.tmp");
		// reset line pointer, s, and close billboards file stream
		reset(lineptr, s, billboards);

		// print to user success
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
		move_files(folder_path + "/statues", f_temp, "replace.tmp");	
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
			scale_decal(lineptr);
		}
		move_files("decals", f_temp, "replace.tmp");
		reset(lineptr, s, decals);
		printf("\x1b[32mSucessfully resized decals!\n\x1b[0m");
	}

	// this defines which timing gate part we will do work on
	int current_timing_gate_part = 1;

	// ######### TIMING GATES ##########
	if (tg) {
		f_temp = fopen("replace.tmp", "w");
		while (getline(&lineptr, &s, tg) != -1) {

			// print the starting gate and checkpoint headers
			if (strcmp("startinggate:\n", lineptr) == 0) {
				fprintf(f_temp, "startinggate:\n");
				continue;
			}

			if (strcmp("checkpoints:\n", lineptr) == 0) {
				current_timing_gate_part++;
				fprintf(f_temp, "checkpoints:\n");
				continue;
			}

			// If we reach the timing gate order we're done with scaling so just print the rest of the file
			if (strcmp("firstlap:\n", lineptr) == 0) {
				current_timing_gate_part++;
			}

			switch (current_timing_gate_part) {
				case 1:
					scale_tg_pt_1(lineptr);
					break;
				case 2:
					scale_tg_pt_2(lineptr);
					break;
				default:
					fprintf(f_temp, "%s", lineptr);
					break;
			}
		}

		move_files("timing_gates", f_temp, "replace.tmp");
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
			scale_flaggers(lineptr);
		}

		move_files("flaggers", f_temp, "replace.tmp");
		reset(lineptr, s, flaggers);
		printf("\x1b[32mSucessfully resized flaggers!\n\x1b[0m");
	}
	*/

	// Free the line pointer and subfolder directories
	free(lineptr);
	if (original_folder_directory != NULL) free(original_folder_directory);
	if (prev_folder_directory != NULL) free(prev_folder_directory);
	return 0;
}

// Reset takes in a pointer, size, and file stream.
// It points the pointer to null, resets the size, and closes the file stream.

void reset(char *ptr, size_t size, FILE *file_stream) {
	ptr = NULL;
	size = 0;
	fclose(file_stream);
}

int do_terrain_work(char* folder_path) {
	char* lineptr = NULL;
	size_t s = 0;

	char* terrain_filepath = malloc(strlen(folder_path) + strlen(filenames[0]) + 2);
	strcpy(terrain_filepath, folder_path);
	strcat(terrain_filepath, "/");
	strcat(terrain_filepath, filenames[0]);

	if (access(terrain_filepath, F_OK) == 0) {
		trackfiles[0] = fopen(terrain_filepath, "r");
	}

	if (trackfiles[0] == NULL) {
		printf("\x1b[31mError: No terrain.hf file for original track scale info\x1b[0m\n");
		return -1;
	}

	// get the line, pass it into the scale_terrain function
	getline(&lineptr, &s, trackfiles[0]);
	scale_terrain(lineptr, terrain_filepath);
	free(terrain_filepath);

	// reset the line pointer, size, and close the file
	reset(lineptr, s, trackfiles[0]);

	// free the line pointer
	free(lineptr);
	printf("\x1b[32mSucessfully resized terrain.hf!\x1b[0m\n");
	return 0;
}

void scale_terrain(char *line, char *terrain_filepath) {

	char *stringp = line;

	char *token = strsep(&stringp, " ");
	int terrain_resolution = atoi(token);


	token = strsep(&stringp, " ");
	terrain_scale = strtod(token, NULL);

	token = strsep(&stringp, " ");
	min_height = strtod(token, NULL);

	token = strsep(&stringp, "\0");
	max_height = strtod(token, NULL);

	// Reset the line ptr to the beginning and rewrite the information out
	stringp = line;
	multiplier = new_terrain_scale / terrain_scale;
	double new_min_height = min_height * multiplier;
	double new_max_height = max_height * multiplier;

	// open temp file, write to it, and close
	f_temp = fopen("replace.tmp", "w");
	fprintf(f_temp, "%d %f %f %f\n", terrain_resolution, new_terrain_scale, new_min_height, new_max_height);
	move_files(terrain_filepath, f_temp, "replace.tmp");

}

void scale_statues_and_billboards(char *line, char *type) {
	char *stringp = line;
	int offset = get_coords(stringp, 3);
	stringp += offset;

	double new_x = coords[0] * multiplier;
	double new_y = coords[1] * multiplier;
	double new_z = coords[2] * multiplier;

	if (strcmp(type, "billboards") == 0) {
		char *sizetoken = strsep(&stringp, " ");
		double size = strtod(sizetoken, NULL);
		char *aspect_and_png = strsep(&stringp, "\n");
		double new_size = size * multiplier;

		fprintf(f_temp, "[%f %f %f] %f %s\n", new_x, new_y, new_z, new_size, aspect_and_png);
		return;
	}

	char *path = strsep(&stringp, "\n");
	fprintf(f_temp, "[%f %f %f] %s\n", new_x, new_y, new_z, path);
}

void scale_decal(char *line) {
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

	double new_x = coords[0] * multiplier;
	double new_z = coords[1] * multiplier;
	double new_size = size * multiplier;

	// Write to temp file
	fprintf(f_temp, "[%f %f] %s %f %s\n", new_x, new_z, angle, new_size, aspect_and_png);
}

void scale_tg_pt_1(char *line) {

	char *stringp = line;
	int offset = get_coords(stringp, 3);

	double new_x = coords[0] * multiplier;
	double new_y = coords[1] * multiplier;
	double new_z = coords[2] * multiplier;

	stringp += offset;

	// set the rest of the path equal to the angle and directory stuff
	char *path = strsep(&stringp, "\n");

	// write to temp file
	fprintf(f_temp, "[%f %f %f] %s\n", new_x, new_y, new_z, path);

}

void scale_tg_pt_2(char *line) {
	char *stringp = line;

	// Get size of gate
	char *token = strsep(&stringp, " ");
	double size = strtod(token, NULL);
	double new_size = size * multiplier;

	// Get first set of coords
	int offset = get_coords(stringp, 3);
	double new_x1 = coords[0] * multiplier;
	double new_y1 = coords[1] * multiplier;
	double new_z1 = coords[2] * multiplier;
	stringp += offset;

	// Get second set of coords
	offset = get_coords(stringp, 3);
	double new_x2 = coords[0] * multiplier;
	double new_y2 = coords[1] * multiplier;
	double new_z2 = coords[2] * multiplier;

	// write to temp file
	fprintf(f_temp, "%f [%f %f %f] [%f %f %f]\n", new_size, new_x1, new_y1, new_z1, new_x2, new_y2, new_z2);
}

void scale_flaggers(char *line) {
	char *stringp = line;
	get_coords(stringp, 3);
	double new_x = coords[0] * multiplier;
	double new_y = coords[1] * multiplier;
	double new_z = coords[2] * multiplier;

	// write to temp file
	fprintf(f_temp, "[%f %f %f]\n", new_x, new_y, new_z);
}

int get_coords(char *ptr, int num_of_coords) {
	char *stringp = ptr;

	// Skip the first bracket character
	stringp++;

	// get x coord
	char *coordtoken = strsep(&stringp, " ");
	double x = strtod(coordtoken, NULL);
	coords[0] = x;
	
	// get y coord if available
	if (num_of_coords > 2) {
		coordtoken = strsep(&stringp, " ");
		double y = strtod(coordtoken, NULL);
		coords[1] = y;
	}

	// get z coord
	coordtoken = strsep(&stringp, "]");
	double z = strtod(coordtoken, NULL);

	// Assign to proper indices in array
	if (num_of_coords < 3) coords[1] = z;
	else coords[2] = z;

	// skip next space
	stringp++;

	// get and return the index we finished at
	return stringp - ptr;
}

char* validate_folder_path(char* folder_path_input) {
	// Create a folder path string variable to be stored on the heap
	size_t folder_path_length = strlen(folder_path_input);
	char* folder_path = malloc(folder_path_length + 1);

	// If we couldn't allocate the space necessary return
	if (folder_path == NULL) {
		return NULL;
	}

	// Validate the folder exists, if it doesn't, free the folder path and return
	struct stat sb;
    if (!(stat(folder_path_input, &sb) == 0 && S_ISDIR(sb.st_mode))) {
		free(folder_path);
        return NULL;
    }

	// Copy the user inputted folder path to the folder path variable
	strcpy(folder_path, folder_path_input);
	return folder_path;
}


// Move files will check where to put the original file, move it, and close then rename the temp file to the original filename
void move_files(char *filename, FILE *temp, char *temp_file_name) {
	fclose(temp);
	remove(filename);
	rename(temp_file_name, filename);
}

char* subfolder_exists(char *folder_path, char *subfolder) {
	char *subfolder_directory = malloc(strlen(folder_path) + strlen(subfolder) + 2);
	
	strcpy(subfolder_directory, folder_path);
	strcat(subfolder_directory, "/");
	strcat(subfolder_directory, subfolder);

	struct stat sb;
	if (!(stat(subfolder_directory, &sb) == 0 && S_ISDIR(sb.st_mode))) {
		free(subfolder_directory);
        return NULL;
    }

	return subfolder_directory;
}