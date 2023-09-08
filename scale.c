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

// This array is the supported filetypes to scale
char* filenames[NUM_FILES] = {"terrain.hf", "billboards", "statues", "decals", "timing_gates", "flaggers", "edinfo"};
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
unsigned short original_folder_directory_created = 0;

/*
################################
		   Functions
################################
*/
// Scale Functions
void scale_terrain(char *, char *, char *);
void scale_statues_and_billboards(char *, char *);
void scale_decal(char *);
void scale_tg_pt_1(char *);
void scale_tg_pt_2(char *);
void scale_flaggers(char *);
void scale_edinfo(char *);
int get_coords(char *, int);
int do_terrain_work(char *);

// Folder Functions
char* validate_folder_path(char *);
char* subfolder_exists(char *, char *);

// File Functions
void move_files(char *, char *, FILE *, char *);
void reset(char *, size_t, FILE *);
char* basename(char* path);

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
		free(folder_path);
		if (original_folder_directory != NULL) free(original_folder_directory);
		if (prev_folder_directory != NULL) free(prev_folder_directory);
		return -1;
	}

	char *lineptr = NULL;
	char *current_filepath = NULL;
	size_t s = 0;

	// 4. Scale the rest of the files
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

		FILE *current_file = fopen(current_filepath, "r");

		// open the temporary file
		f_temp = fopen("replace.tmp", "w");
		
		if (strcmp(filenames[i], "timing_gates") != 0) {
			while (getline(&lineptr, &s, current_file) != -1) {
				if (strcmp(filenames[i], "edinfo") == 0) {
					scale_edinfo(lineptr);
					continue;
				}

				if (lineptr[0] != '[') {
					fprintf(f_temp, "%s", lineptr);
					continue;
				}
				if (strcmp(filenames[i], "billboards") == 0 || strcmp(filenames[i], "statues") == 0) {
					scale_statues_and_billboards(lineptr, filenames[i]);
					continue;
				}

				if (strcmp(filenames[i], "decals") == 0) {
					scale_decal(lineptr);
					continue;
				}

				if (strcmp(filenames[i], "flaggers") == 0) {
					scale_flaggers(lineptr);
				}				
			}
		} else {
			// do timing gates work
			unsigned short current_timing_gate_part = 1;
			while (getline(&lineptr, &s, current_file) != -1) {
				// print the starting gate and checkpoint headers
				if (strcmp("startinggate:\n", lineptr) == 0) {
					fprintf(f_temp, "startinggate:\n");
					continue;
				}

				if (strcmp("checkpoints:\n", lineptr) == 0) {
					current_timing_gate_part++; // current_timing_gate_part = 2
					fprintf(f_temp, "checkpoints:\n");
					continue;
				}

				// If we reach the timing gate order we're done with scaling so just print the rest of the file
				if (strcmp("firstlap:\n", lineptr) == 0) {
					current_timing_gate_part++; // current_timing_gate_part = 3
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
		}

		move_files(folder_path, current_filepath, f_temp, "replace.tmp");
		reset(lineptr, s, current_file);
		printf("\x1b[32mSucessfully resized %s!\x1b[0m\n", filenames[i]);
	}

	// Free memory in use
	free(lineptr);
	free(current_filepath);
	free(folder_path);
	free(original_folder_directory);
	free(prev_folder_directory);

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

	FILE *terrain;
	if (access(terrain_filepath, F_OK) != 0) {
		printf("\x1b[31mError: No terrain.hf file for original track scale info\x1b[0m\n");
		free(terrain_filepath);
		return -1;
	}

	terrain = fopen(terrain_filepath, "r");

	// get the line, pass it into the scale_terrain function
	getline(&lineptr, &s, terrain);
	scale_terrain(lineptr, terrain_filepath, folder_path);
	free(terrain_filepath);

	// reset the line pointer, size, and close the file
	reset(lineptr, s, terrain);

	// free the line pointer
	free(lineptr);
	printf("\x1b[32mSucessfully resized terrain.hf!\x1b[0m\n");
	return 0;
}

void scale_terrain(char *line, char *terrain_filepath, char *folder_path) {

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
	min_height *= multiplier;
	max_height *= multiplier;

	// open temp file, write to it, and close
	f_temp = fopen("replace.tmp", "w");
	fprintf(f_temp, "%d %f %f %f\n", terrain_resolution, new_terrain_scale, min_height, max_height);
	move_files(folder_path, terrain_filepath, f_temp, "replace.tmp");
}

void scale_statues_and_billboards(char *line, char *type) {
	char *stringp = line;
	int offset = get_coords(stringp, 3);
	stringp += offset;

	double new_x = coords[0] * multiplier;
	double new_y = coords[1] * multiplier;
	double new_z = coords[2] * multiplier;

	// get extra variables to scale if we're scaling billboards
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
	size *= multiplier;

	// Write to temp file
	fprintf(f_temp, "[%f %f] %s %f %s\n", new_x, new_z, angle, size, aspect_and_png);
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
	size *= multiplier;

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
	fprintf(f_temp, "%f [%f %f %f] [%f %f %f]\n", size, new_x1, new_y1, new_z1, new_x2, new_y2, new_z2);
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

void scale_edinfo(char *line) {
	char *stringp = line;
	char *token = strsep(&stringp, " ");

	if (strcmp(token, "add_gradient") == 0) {
		// add_gradient <start_x> <start_z> <end_x> <end_z>
		token = strsep(&stringp, " ");
		double start_x = strtod(token, NULL);

		token = strsep(&stringp, " ");
		double start_z = strtod(token, NULL);

		token = strsep(&stringp, " ");
		double end_x = strtod(token, NULL);

		token = strsep(&stringp, " ");
		double end_z = strtod(token, NULL);

		start_x *= multiplier;
		start_z *= multiplier;
		end_x *= multiplier;
		end_z *= multiplier;

		fprintf(f_temp, "add_gradient %f %f %f %f\n", start_x, start_z, end_x, end_z);
		return;
	}

	if (strcmp(token, "add_point") == 0) {
		// add_point <0 linear, 1 curved> <distance from origin> <height>
		token = strsep(&stringp, " ");
		int point_type = atoi(token);

		token = strsep(&stringp, " ");
		double distance = strtod(token, NULL);

		token = strsep(&stringp, " ");
		double height = strtod(token, NULL);

		distance *= multiplier;
		height *= multiplier;

		fprintf(f_temp, "add_point %d %f %f\n", point_type, distance, height);
	}
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
void move_files(char *folder_path, char *filename, FILE *temp, char *temp_file_name) {
	
	// close the temp file
	fclose(temp);

	char *base_filename = basename(filename);
	char *file_location = NULL;

	if (original_folder_directory == NULL) {
		// allocate memory for the original files directory
		original_folder_directory = malloc(strlen(folder_path) + strlen("/original files/") + 1);
		strcpy(original_folder_directory, folder_path);
		strcat(original_folder_directory, "/original files/");

		// create the directory, and let
		mkdir(original_folder_directory, 0700);
		original_folder_directory_created = 1;
	}

	// if the previous folder directory is empty and we never generated a original folder directory it means that the folder already exists
	if (prev_folder_directory == NULL && original_folder_directory_created == 0) {
		// allocate memory for the previous files directory
		prev_folder_directory = malloc(strlen(folder_path) + strlen("/previous files/") + 1);
		strcpy(prev_folder_directory, folder_path);
		strcat(prev_folder_directory, "/previous files/");

		// create the directory
		mkdir(prev_folder_directory, 0700);
	}

	// if we didn't create an original folder, go to previous files location
	if (original_folder_directory_created == 0) {
		file_location = malloc(strlen(prev_folder_directory) + strlen(base_filename) + 1);
		strcpy(file_location, prev_folder_directory);
		strcat(file_location, base_filename);
	} else { // otherwise go to original files location
		file_location = malloc(strlen(original_folder_directory) + strlen(base_filename) + 1);
		strcpy(file_location, original_folder_directory);
		strcat(file_location, base_filename);
	}

	rename(filename, file_location);
	rename(temp_file_name, filename);

	free(file_location);
	free(base_filename);
}

char* subfolder_exists(char *folder_path, char *subfolder) {
	char *subfolder_directory = malloc(strlen(folder_path) + strlen(subfolder) + 3);
	
	strcpy(subfolder_directory, folder_path);
	strcat(subfolder_directory, "/");
	strcat(subfolder_directory, subfolder);
	strcat(subfolder_directory, "/");

	struct stat sb;
	if (!(stat(subfolder_directory, &sb) == 0 && S_ISDIR(sb.st_mode))) {
		free(subfolder_directory);
        return NULL;
    }

	return subfolder_directory;
}

char* basename(char *path) {
	char *s = strrchr(path, '/');
	if (s == NULL) {
		return strdup(path);
	}
	return strdup(s + 1);
}