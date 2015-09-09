#include <stdio.h>
#include <string.h>

#include "stations_defs.c"

const char credits[] =
	"\n"
	"**********************************\n"
	"Data provided by NJ TRANSIT, which\n"
	"is the sole owner of the Data.\n"
	"**********************************\n";

const size_t sz_credits = sizeof(credits);

void
stations_list()
{
	int i;
	for (i = 0; i < n_stations; i++) {
		printf("%-40s    %2s\n", stations[i], codes[i]);
	}

}

size_t
station_index(const char *code)
{
	int i;
	for (i = 0; i < n_stations; i++) {
		if (strcmp(code, codes[i]) == 0)
			return i;
	}
	return -1;
}

const char *
station_name(const char *code)
{
	int i;
	for (i = 0; i < n_stations; i++) {
		if (strcmp(code, codes[i]) == 0)
			return stations[i];
	}
	return NULL;
}

const char *
station_code(const char *name)
{
	int i;
	for (i = 0; i < n_stations; i++) {
		if (strcmp(name, stations[i]) == 0)
			return codes[i];
	}
	return NULL;
}

const char *
station_verify_code(const char *code)
{
	if (code == NULL)
		return NULL;

	int i;
	for (i = 0; i < n_stations; i++) {
		if (strcmp(code, codes[i]) == 0)
			return codes[i];
	}
	return NULL;
}


