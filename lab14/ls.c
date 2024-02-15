/*
 * Copyright (C) 2014-2017 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2019-2020 Julián Burella Pérez <julian.burellaperez@heig-vd.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <syscall.h>

#define KB 1024

/* Convert a size in bytes to a human readable format
 * Only one number after the comma will be displayed
 */
char *bytes_to_human_readable(const unsigned long int bytes, char *buf) {
	char *prefix[] = {"", "K", "M", "G", "T"};
	size_t i = 0;
	double size = (double) bytes;

	while (size >= KB) {
		size /= KB;
		i++;
	}

	// Note: Seems like printf is bugged with %g and %f
	sprintf(buf, "%g%s", size, prefix[i]);
	return buf;
}

char *attr_to_str(mode_t mod, char *buf) {

	buf[0] = mod & AM_DIR ? mod & AM_SYM ? 'l' : 'd' : '-';
	buf[1] = mod & AM_RD ? 'r' : '-';
	buf[2] = mod & AM_WR ? 'w' : '-';
	buf[3] = mod & AM_EX ? 'x' : '-';
	buf[4] = 0;

	return buf;
}

void close_fd(int fd, const char* file){
	if (close(fd) < 0) {
        printf("Error closing fd %s\n", file);
    }
}

/**
 * Convert a symbolic link to a string
 * The string must be freed by the caller
 * @param file the path to the symbolic link
 * @return the string representation of the symbolic link
*/
char *link_to_str(const char *file) {

    int fd = open(file, O_NOFOLLOW | O_PATH);
    if (fd < 0) {
        printf("Error opening the file %s\n", file);
        close_fd(fd, file);
        return NULL;
    }

	char* buf = calloc(FILENAME_SIZE, sizeof(char));
    if (buf == NULL) {
        printf("Error during memory allocation\n");
        close_fd(fd, file);
        return NULL;
    }

    int err = read(fd, buf, FILENAME_SIZE - 1);
    if (err < 0) {
        printf("Error reading the file %s\n", file);
        close_fd(fd, file);
        return NULL;
    }

    return buf;
}

/**
 * Print the information of a file
 * @param file the path to the file
 * @param is_dir 1 if the file is a directory, 0 otherwise
 * @param extended 1 if the extended information must be printed, 0 otherwise
*/
void print_info_file(const char *file, int is_dir, int extended) {

	char date[80];
	char size[80];
	char modes[5];
	struct stat f_stat;
	time_t time;
	struct tm tm;

	if (!extended) {
		printf("%s%s\n", file, is_dir ? "/" : "");

	} else {
		if (stat(file, &f_stat) < 0) {
			printf("Error getting metadata\n");
			return;
		}

		 // Convert modes
        attr_to_str(f_stat.st_mode, modes);

        // get last modif. time struct
        time = f_stat.st_mtim;

		// Get local time and convert it
        tm = *localtime(&time);

		// Format date (Unix time require to add 1900 to year and 1 to month)
        sprintf(date, "%d-%d-%d %d:%d", 1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);

        // Convert size to human readable format
        bytes_to_human_readable(f_stat.st_size, size);

		// Print the file information
        printf("%s | %s | %s | %s%s", modes, date, size, file, is_dir ? "/" : "");

        // Manage symbolic links
        if (f_stat.st_mode & AM_SYM) {
            char* link = link_to_str(file);

			if (link == NULL) {
				printf("\n");
				return;
			}

            printf(" -> %s\n", link);
            free(link);
        } else {
            printf("\n");
        }
	}
}

/*
 * Main function of ls application.
 * The ls application is very very short and does not support any options like
 * -l -a etc. It is only possible to give a subdir name to list the directory
 * entries of this subdir.
 */

int main(int argc, char **argv) {
	DIR *stream;
	struct dirent *p_entry;
	char *dir;
	int extended = 0;
	const char *usage = "Usage: ls [-l] [DIR]\n";

	if (argc == 1) {
		dir = ".";
	} else if ((argc > 1) && (argc < 4)) {

		if (!strcmp(argv[1], "-l")) {
			extended = 1;
			if (argc != 2) {
				dir = argv[2];
			} else {
				dir = ".";
			}

		} else if ((argc == 3) && !strcmp(argv[2], "-l")) {
			extended = 1;
			dir = argv[1];
		} else if (argc == 2) {
			dir = argv[1];
		} else {
			printf("%s", usage);
			exit(1);
		}
	} else {
		printf("%s", usage);
		exit(1);
	}

	stream = opendir(dir);

	if (stream == NULL)
		exit(1);

	while ((p_entry = readdir(stream)) != NULL) {
		print_info_file(p_entry->d_name, p_entry->d_type == DT_DIR,
				extended);
	}

	exit(0);
}

