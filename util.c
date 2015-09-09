#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>

int
read_text(const char *fname, char **text, size_t *len)
{
	struct stat st;
	FILE *f = NULL;
	char *buff = NULL;
	int ret = -1;

	if (stat(fname, &st) != 0) {
		printf("Cannot stat file %s. Error: %d\n", fname, errno);
		goto out;
	}

	f = fopen(fname, "rt");
	if (f == NULL) {
		printf("Cannot open file %s. Error: %d\n", fname, errno);
		goto out;
	}

	buff = malloc(st.st_size + 1);
	if (buff == NULL) {
		printf("Cannot allocate %" PRId64 " bytes. Error: %d\n", st.st_size + 1, errno);
		goto out;
	}

	fread(buff, 1, st.st_size, f);
	fclose(f);
	f = NULL;
	buff[st.st_size] = 0;
	*text = buff;
	*len = st.st_size;
	ret = 0;

out:
	if (f != NULL)
		fclose(f);

	return ret;
}

int
expired(const char *fname)
{
	struct stat st;

	int rc = stat(fname, &st);
	if (rc != 0) {
		return 1;
	}

	if (st.st_mtime + 60 < time(NULL)) {
		return 2;
	}

	return 0;
}

