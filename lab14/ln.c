#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int is_valid = 1;
	int fd_src;
	char *f_src = NULL;
	char *f_dst = NULL;

	if (argc != 3) {
		is_valid = 0;
	}

	if (!is_valid) {
		printf("Usage: ln <src> <dst>\n");
		exit(1);
	}

	f_src = argv[1];
	f_dst = argv[2];

	if (*f_src == 0 || *f_dst == 0) {
        printf("Invalid file name\n");
        exit(1);
    }

    fd_src = open(f_src, O_RDONLY);
    if (fd_src < 0) {
        printf("Error opening the file %s\n", f_src);
        exit(1);
    }

    if (sys_symlink(fd_src, f_dst) < 0) {
        printf("Error: cannot create symlink %s\n", f_dst);
        exit(1);
    }
    
    if (close(fd_src) < 0) {
        printf("Error: cannot close file %s\n", f_src);
        exit(1);
    }

	return 0;
}
