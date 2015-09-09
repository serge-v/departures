/**
* Serge Voilokov, 2015.
* Get departures for NJ transit trains.
*/
#include <ctype.h>
#include <curl/curl.h>
#include <err.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>

#include "../common/net.h"
#include "stations.h"
#include "parser.h"
#include "util.h"
#include "color.h"
#include "version.h"

static int debug = 0;                 /* debug parameter */
static int debug_server = 0;          /* make requests to debug local server */
static char *station_from = NULL;     /* departure station */
static char *station_to = NULL;       /* destination station */
static FILE *log = NULL;              /* verbose debug log */
static int email = 0;                 /* send email */
static int all = 0;                   /* show all trains for station */
static char *train = NULL;            /* train code */

static struct option longopts[] = {
	{ "list",         no_argument,       NULL, 'l' },
	{ "from",         required_argument, NULL, 'f' },
	{ "to",           required_argument, NULL, 't' },
	{ "mail",         no_argument,       NULL, 'm' },
	{ "all",          no_argument,       NULL, 'a' },
	{ "stops",        no_argument,       NULL, 'p' },
	{ "debug",        no_argument,       NULL, 'd' },
	{ "help",         no_argument,       NULL, 'h' },
	{ "debug-server", no_argument,       NULL, 's' },
	{ "version",      no_argument,       NULL, 'v' },
	{ NULL,           0,                 NULL,  0  }
};

static void
synopsis()
{
	printf("usage: departures [-ldmhvap] [-f station] [-t station] [-p train]\n");
}

static void
usage()
{
	synopsis();

	printf(
		"options:\n"
		"    -l, --list            list stations\n"
		"    -f, --from=station    get next departure and train status\n"
		"    -t, --to=station      set destination station\n"
		"    -a, --all             get all departures for station\n"
		"    -p, --stops=train     get stops for train\n"
		"    -m, --mail            send email with nearest departure\n"
		"    -d, --debug           output debug information\n"
		"    -s, --debug-server    use debug server\n"
		"    -v, --version         print version\n"
		);
}

/* ===== data structures ===================== */

struct departure
{
	char            *time;          /* departure time */
	char            *destination;   /* destination station name */
	char            *line;          /* rail route name */
	char            *train;         /* train label or number */
	char            *track;         /* departure track label or number */
	char            *status;        /* train status */
	const char      *code;          /* station code */
	size_t          next;           /* next train order starting from 1 */
	SLIST_ENTRY(departure) entries; /* handler for slist */
};

SLIST_HEAD(departure_list, departure);

struct departures
{
	struct departure_list *list;   /* departures list */
	size_t size;                   /* departures list size */
};

struct station
{
	char *code;                     /* station code */
	char *name;                     /* station name */
	struct departures* deps;        /* list of departures for this station */
	SLIST_ENTRY(station) entries;   /* handler for slist */
};

SLIST_HEAD(station_list, station);

struct route
{
	const char *name;                  /* route name */
	struct station_list *stations;     /* station list for this route */
};

struct stop
{
	char *name;
	const char *code;
	char *status;
	SLIST_ENTRY(stop) entries;
};

SLIST_HEAD(stop_list, stop);

/* =========================================== */

static void
departure_dump(struct departure *d)
{
	printf(COL_TIME COL_TRAIN "%s " COL_DEST COL_TRACK "%s\n",
		d->time, d->train, d->code, d->destination,
		d->track, d->status);
}

static void
parse_tr(char *text, int len, struct departures *deps, struct departure **last_added)
{
	if (strstr(text, "<td colspan=") != NULL)
		return;

	struct trscanner scan;
	struct departure *dep;

	trscanner_create(&scan, text, len);
	dep = calloc(1, sizeof(struct departure));

	if (!trscanner_next(&scan))
		err(1, "first table cell doesn't contain a time");

	dep->time = scan.sbeg; // 1

	if (strncmp("DEP", dep->time, 3) == 0)
		return;

	if (!trscanner_next(&scan))
		errx(1, "second table cell doesn't contain a destination station");

	dep->destination = scan.sbeg; // 2
	if (strstr(dep->destination, "&nbsp;-") != NULL) {
		strcpy(&dep->destination[strlen(dep->destination)-7], " (SEC)");
	}

	if (!trscanner_next(&scan))
		errx(1, "Cannot parse track");

	dep->track = scan.sbeg; // 3
	if (strcmp("Single", dep->track) == 0)
		strcpy(dep->track, "1");

	if (!trscanner_next(&scan))
		errx(1, "Cannot parse line");

	dep->line = scan.sbeg; // 4

	if (!trscanner_next(&scan))
		errx(1, "Cannot parse train");

	dep->train = scan.sbeg; // 5

	if (trscanner_next(&scan))
		dep->status = scan.sbeg; // 6

	trscanner_destroy(&scan);

	dep->code = station_code(dep->destination);

	if (*last_added == NULL)
		SLIST_INSERT_HEAD(deps->list, dep, entries);
	else
		SLIST_INSERT_AFTER(*last_added, dep, entries);

	deps->size++;
	*last_added = dep;
}

static void
station_load(struct station* st, const char *fname)
{
	int rc;
	regex_t p1, p2;
	regmatch_t m1, m2;
	char *text;
	size_t len;
	struct departure *last_added = NULL;

	st->deps = calloc(1, sizeof(struct departures));
	if (st->deps == NULL)
		err(1, "Cannot allocate deps");

	st->deps->list = calloc(1, sizeof(struct departure_list));
	SLIST_INIT(st->deps->list);

	rc = read_text(fname, &text, &len);
	if (rc != 0)
		err(rc, "cannot read file");

	if (debug)
		fprintf(log, "read %zu bytes from %s\n", len, fname);

	rc = regcomp(&p1, "<tr[^>]*>", REG_EXTENDED);
	if (rc != 0)
		print_rex_error(rc, &p1);

	rc = regcomp(&p2, "</tr>", REG_EXTENDED);
	if (rc != 0)
		print_rex_error(rc, &p2);

	m1.rm_so = 0;
	m1.rm_eo = len;

	for (;;)
	{
		rc = regexec(&p1, text, 1, &m1, REG_STARTEND);

		if (rc == REG_NOMATCH)
			break;

		if (rc != 0)
			print_rex_error(rc, &p1);

		m2.rm_so = m1.rm_eo;
		m2.rm_eo = len;

		rc = regexec(&p2, text, 1, &m2, REG_STARTEND);

		if (rc == REG_NOMATCH)
			break;

		if (rc != 0)
			print_rex_error(rc, &p2);

		if (debug)
			fprintf(log, "tr: %.*s\n", (int)(m2.rm_so - m1.rm_so), &text[m1.rm_eo]);

		parse_tr(&text[m1.rm_eo], m2.rm_so - m1.rm_eo, st->deps, &last_added);

		m1.rm_so = m2.rm_eo;
		m1.rm_eo = len;
	}

	regfree(&p1);
	regfree(&p2);
}

static struct station*
station_create(const char *station_code)
{
	const char *api_url = "http://dv.njtransit.com/mobile/tid-mobile.aspx?SID=%s&SORT=A";
	if (debug_server)
		api_url = "http://127.0.0.1:8000/njtransit-%s.html";

	char fname[PATH_MAX];
	char url[100];

	struct station* st = calloc(1, sizeof(struct station));
	if (st == NULL)
		err(1, "Cannot allocate station");

	st->code = strdup(station_code);
	st->name = strdup(station_name(station_code));
	snprintf(fname, PATH_MAX, "/tmp/njtransit-%s.html", st->code);
	snprintf(url, 100, api_url, st->code);

	if (expired(fname)) {
		if (fetch_url(url, fname) != 0)
			err(1, "Cannot fetch departures for station");

		if (debug)
			fprintf(log, "%s fetched\n", fname);
	}

	station_load(st, fname);

	return st;
}

static void
station_dump(struct station *s)
{
	printf("=== %s(%s) === [%zu] =====================\n",
		s->name, s->code, s->deps->size);

	struct departure header = {
		.time = "DEP",
		.train = "TRAIN",
		.code = "SC",
		.destination = "TO",
		.track = "TRK",
		.status = "STATUS",
	};

	departure_dump(&header);

	struct departure* dep;

	SLIST_FOREACH(dep, s->deps->list, entries) {
		departure_dump(dep);
	}

	printf("--\n");
}

static void
station_destroy(struct station *s)
{
	if (s == NULL)
		return;

	struct departure *dep;

	while (!SLIST_EMPTY(s->deps->list)) {
		dep = SLIST_FIRST(s->deps->list);
		SLIST_REMOVE_HEAD(s->deps->list, entries);
		free(dep);
	}

	free(s->deps);
	free(s);
}

/*
 * Sets "next" member to the "1" for the next train, "2" for the train after the next and so on.
 * Returns the number of next trains to  the destination.
 */
static size_t
departures_calculate_next(struct departures *deps, const char *dest_code)
{
	struct departure *dep;
	size_t num = 0;

	SLIST_FOREACH(dep, deps->list, entries) {
		if (strcmp(dep->code, dest_code) == 0)
			dep->next = ++num;
	}

	return num;
}

static bool
train_append_status(struct buf *b, struct station *st, const char *train, bool appended)
{
	struct departure *dep;
	int rc;
	char s[1024];
	size_t sz = sizeof(s);
	const char *status = NULL;
	const char *positive = " Previous stops status:\n\n";

	SLIST_FOREACH(dep, st->deps->list, entries) {

		if (strcmp(train, dep->train) != 0)
			continue;

		status = dep->status;
		if (status != NULL && *status != 0) {
			rc = snprintf(s, sz, "    %s(%s): %s\n", station_name(st->code), st->code, status);

			if (!appended) {
				buf_append(b, positive, strlen(positive));
				appended = true;
			}

			buf_append(b, s, rc);
		}
	}

	return appended;
}

static int
compare(const void *v1, const void *v2)
{
	const char *s1 = v1;
	const char *s2 = v2;
	return strcmp(s1, s2);
}

static const char *
propose_destinations(struct station *st, const char *to)
{
	to = station_verify_code(to);
	if (to != NULL)
		return to;

	const size_t sz = st->deps->size;
	const char **codes = calloc(sz, sizeof(char*));

	size_t i = 0;
	struct departure* dep;
	SLIST_FOREACH(dep, st->deps->list, entries) {
		codes[i++] = dep->code;
	}

	qsort(codes, sz, sizeof(char*), compare);

	size_t uniq_count = 0;
	const char *prev = NULL;
	for (i = 0; i < sz; i++) {
		if (prev == NULL || strcmp(prev, codes[i]) != 0) {
			uniq_count++;
			prev = codes[i];
		}
	}

	if (uniq_count == 1)
		return codes[0];

	printf("Multiple destinantions found.\nUse -t parameter and station code from the list:\n");
	prev = NULL;
	for (i = 0; i < sz; i++) {
		if (prev == NULL || strcmp(prev, codes[i]) != 0) {
			printf("%-20s %s\n", station_name(codes[i]), codes[i]);
			prev = codes[i];
		}
	}
	return NULL;
}

static void
parse_par(char *text, size_t len, char **stop_name, char **stop_status)
{
	regex_t p1, p2;
	regmatch_t m1, m2;

	int rc = regcomp(&p1, "<p[^>]*>", REG_EXTENDED);
	if (rc != 0)
		print_rex_error(rc, &p1);

	rc = regcomp(&p2, "</p>", REG_EXTENDED);
	if (rc != 0)
		print_rex_error(rc, &p2);

	m1.rm_so = 0;
	m1.rm_eo = len;

	rc = regexec(&p1, text, 1, &m1, REG_STARTEND);

	if (rc == REG_NOMATCH)
		return;

	if (rc != 0)
		print_rex_error(rc, &p1);

	m2.rm_so = m1.rm_eo;
	m2.rm_eo = len;

	rc = regexec(&p2, text, 1, &m2, REG_STARTEND);

	if (rc == REG_NOMATCH)
		return;

	if (rc != 0)
		print_rex_error(rc, &p2);


	size_t plen = m2.rm_so - m1.rm_eo;
	char* ptext = &text[m1.rm_eo];
	if (debug)
		fprintf(log, "  p raw: %.*s\n", (int)plen, ptext);
	char *p = strstr(ptext, "&nbsp;&nbsp;");
	if (p != NULL)
		memset(p, 0, 12);

	*stop_name = ptext;
	*stop_status = p + 12;
	text[m2.rm_so] = 0;

	if (debug)
		fprintf(log, "stop_name: %s, stop_status: %s\n", *stop_name, *stop_status);
}

static void
parse_train_stops(const char *fname, struct stop_list* list)
{
	int rc;
	regex_t p1, p2;
	regmatch_t m1, m2;
	char *text;
	size_t len;

	rc = read_text(fname, &text, &len);
	if (rc != 0)
		err(rc, "cannot read file");

	rc = regcomp(&p1, "<tr[^>]*>", REG_EXTENDED);
	if (rc != 0)
		print_rex_error(rc, &p1);

	rc = regcomp(&p2, "</tr>", REG_EXTENDED);
	if (rc != 0)
		print_rex_error(rc, &p2);

	m1.rm_so = 0;
	m1.rm_eo = len;

	struct stop *last = NULL;

	for (;;)
	{
		rc = regexec(&p1, text, 1, &m1, REG_STARTEND);

		if (rc == REG_NOMATCH)
			break;

		if (rc != 0)
			print_rex_error(rc, &p1);

		m2.rm_so = m1.rm_eo;
		m2.rm_eo = len;

		rc = regexec(&p2, text, 1, &m2, REG_STARTEND);

		if (rc == REG_NOMATCH)
			break;

		if (rc != 0)
			print_rex_error(rc, &p2);


		size_t tdlen = m2.rm_so - m1.rm_eo;
		if (debug)
			fprintf(log, "tr: %.*s\n", (int)tdlen, &text[m1.rm_eo]);

		char *name = NULL;
		char *status = NULL;

		parse_par(&text[m1.rm_eo], tdlen, &name, &status);

		if (name != NULL) {
			struct stop *stop = calloc(1, sizeof(struct stop));

			stop->name = name;
			stop->code = station_code(stop->name);
			stop->status = status;

			if (SLIST_EMPTY(list))
				SLIST_INSERT_HEAD(list, stop, entries);
			else
				SLIST_INSERT_AFTER(last, stop, entries);

			last = stop;
		}

		m1.rm_so = m2.rm_eo;
		m1.rm_eo = len;
	}

	regfree(&p1);
	regfree(&p2);
}

static size_t
get_prev_stations(const char *from_code, const char *train, struct stop_list *list)
{
	const char *prefix = "";
	const char *api_url = "http://dv.njtransit.com/mobile/train_stops.aspx?sid=%s&train=%s%s";
	if (debug_server)
		api_url = "http://127.0.0.1:8000/njtransit-train-%s-%s%s.html";

	char fname[PATH_MAX];
	char url[100];

	snprintf(fname, PATH_MAX, "/tmp/njtransit-train-%s-%s.html", from_code, train);

	if (!debug_server && strlen(train) == 2)
		prefix = "00";

	snprintf(url, 100, api_url, from_code, prefix, train);

	if (expired(fname)) {
		if (fetch_url(url, fname) != 0)
			return -1;

		if (debug)
			printf("%s fetched\n", fname);
	}

	parse_train_stops(fname, list);

	if (debug) {
		struct stop *stop;
		SLIST_FOREACH(stop, list, entries) {
			printf("stop: %s(%s), %s\n", stop->name, stop->code, stop->status);
		}
	}

	return 0;
}

static struct stop *
stop_find(struct stop_list *list, const char *station_code)
{
	struct stop *stop;

	SLIST_FOREACH(stop, list, entries) {
		if (strcmp(stop->code, station_code) == 0)
			return stop;
	}

	return NULL;
}

static struct stop_list *
reversed(struct stop_list *list)
{
	struct stop *stop, *newstop;
	struct stop_list *nl = calloc(1, sizeof(struct stop_list));

	SLIST_FOREACH(stop, list, entries) {
		newstop = calloc(1, sizeof(struct stop));
		memcpy(newstop, stop, sizeof(struct stop));
		SLIST_INSERT_HEAD(nl, newstop, entries);
	}

	return nl;
}

static int
departures_get_upcoming(const char* from_code, const char *dest_code, struct buf *b)
{
	struct station *st = station_create(from_code);
	if (debug)
		station_dump(st);

	dest_code = propose_destinations(st, dest_code);

	if (dest_code == NULL)
		return 1;

	size_t n_next_trains = departures_calculate_next(st->deps, dest_code);
	if (n_next_trains == 0)
		errx(1, "No next trains to %s(%s) found", station_name(dest_code), dest_code);

	if (debug)
		printf("number of next trains to %s: %zu\n", dest_code, n_next_trains);

	size_t i = 0, n = 0;
	char s[1024];
	size_t sz = sizeof(s);

	const char *dest_name = station_name(dest_code);
	const char *from_name = station_name(from_code);

	struct departure *dep = SLIST_FIRST(st->deps->list);

	if (debug)
		printf("previous stations list:\n");

	n = snprintf(s, sz, "\nTrains from %s to %s:\n\n", from_name, dest_name);
	buf_append(b, s, n);

	while (i < n_next_trains) {

		if (dep->next == 0) {
			dep = SLIST_NEXT(dep, entries);
			continue;
		}

		if (debug)
			printf("get status for next train %s to %s, idx: %zu\n", dep->train, dest_code, dep->next);

		n = snprintf(s, sz, "%s #%s, Track %s",
			dep->time, dep->train, dep->track);
		buf_append(b, s, n);

		if (dep->status != NULL && strlen(dep->status) > 0) {
			buf_append(b, " ", 1);
			buf_append(b, dep->status, strlen(dep->status));
		}
		buf_append(b, ".", 1);

		struct stop_list *route = calloc(1, sizeof(struct stop_list));

		get_prev_stations(from_code, dep->train, route);

		if (SLIST_EMPTY(route)) {
			n = snprintf(s, sz, "No route found for train %s from %s to %s\n", dep->train, from_code, dest_code);
			buf_append(b, s, n);
			dep = SLIST_NEXT(dep, entries);
			i++;
			continue;
		}

		struct stop_list *rev_route = reversed(route);

		/* extract to separate method */
		struct stop *origin_stop = stop_find(rev_route, from_code);
		if (origin_stop == NULL)
			errx(1, "Strange that cannot get origin stop code from reversed route");

		struct stop *stop = SLIST_NEXT(origin_stop, entries);

		const char *negative = " No previous stops status.\n";
		bool appended = false;

		while (stop != NULL) {

			struct station *st = station_create(stop->code);

			if (st == NULL) {
				fprintf(stderr, "Cannot get departures for station code %s\n", stop->code);
				return 1;
			}

			if (debug)
				station_dump(st);

			appended |= train_append_status(b, st, dep->train, appended);

			stop = SLIST_NEXT(stop, entries);
			station_destroy(st);
		}
		/* == */

		if (!appended)
			buf_append(b, negative, strlen(negative));

		buf_append(b, "\n", 1);

		dep = SLIST_NEXT(dep, entries);
		i++;
		if (i > 2)
			break; // max 2 next trains
	}

	buf_append(b, credits, sz_credits);

	return 0;
}

static void
version()
{
	printf("departures\n");
	printf("version %s\n", app_version);
	printf("date %s\n", app_date);
	if (strlen(app_diff_stat) > 0) {
		printf("uncommited changes:\n%s\n", app_diff_stat);
		if (debug)
			printf("full diff:\n%s\n", app_diff_full);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		synopsis();
		return 1;
	}

	int ch;

	while ((ch = getopt_long(argc, argv, "lhdsmvaf:t:p:", longopts, NULL)) != -1) {
		switch (ch) {
			case 'd':
				debug = 1;
				log = fopen("/tmp/departures-debug.log", "wt");
				fprintf(log, "==================\n\n\n\n\n\n\n");
				break;
			case 'm':
				email = 1;
				break;
			case 'a':
				all = 1;
				break;
			case 'l':
				stations_list();
				return 0;
			case 'f':
				station_from = optarg;
				break;
			case 'p':
				train = optarg;
				break;
			case 't':
				station_to = optarg;
				break;
			case 's':
				debug_server = 1;
				break;
			case 'h':
				usage();
				return 1;
			case 'v':
				version();
				return 1;
			default:
				synopsis();
				return 1;
		}
	}

	if (station_from == NULL)
		errx(1, "Origin station is not specified");

	curl_global_init(CURL_GLOBAL_ALL);

	struct buf b;
	memset(&b, 0, sizeof(struct buf));

	if (departures_get_upcoming(station_from, station_to, &b) == 0) {
		printf("%s", b.s);

		if (email) {
			struct message m = {
				.from = "serge0x76@gmail.com",
				.to = "serge0x76+njt@gmail.com",
				.subject = "train to Hoboken",
				.body = b.s,
			};

			int rc = send_email(&m);

			if (rc != 0)
				return 1;
		}
	}

	curl_global_cleanup();
	return 0;
}

