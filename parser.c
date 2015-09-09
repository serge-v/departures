#include "parser.h"
#include <memory.h>
#include <err.h>
#include <ctype.h>

void
print_rex_error(int errcode, const regex_t *preg)
{
	char buf[1000];
	size_t rc = regerror(errcode, preg, buf, sizeof(buf));
	if (rc <= 0)
		err(1, "cannot get error string for code: %d", errcode);
	err(1, "%s", buf);
}

void
trscanner_create(struct trscanner *s, char *text, int len)
{
	int rc;

	memset(s, 0, sizeof(struct trscanner));

	rc = regcomp(&s->p1, "<td[^>]*>", REG_EXTENDED);
	if (rc != 0)
		print_rex_error(rc, &s->p1);

	rc = regcomp(&s->p2, "</td>", REG_EXTENDED);
	if (rc != 0)
		print_rex_error(rc, &s->p2);

	s->text = text;
	s->len = len;
	s->m1.rm_so = 0;
	s->m1.rm_eo = len;
}

void
trscanner_destroy(struct trscanner *s)
{
	regfree(&s->p1);
	regfree(&s->p2);
	memset(s, 0, sizeof(struct trscanner));
}

int
trscanner_next(struct trscanner *s)
{
	int rc;

	rc = regexec(&s->p1, s->text, 1, &s->m1, REG_STARTEND);

	if (rc == REG_NOMATCH)
		return 0;

	if (rc != 0)
		print_rex_error(rc, &s->p1);

	s->m2.rm_so = s->m1.rm_eo;
	s->m2.rm_eo = s->len;

	rc = regexec(&s->p2, s->text, 1, &s->m2, REG_STARTEND);

	if (rc == REG_NOMATCH)
		return 0;

	if (rc != 0)
		print_rex_error(rc, &s->p2);

	s->sbeg = &s->text[s->m1.rm_eo];
	s->send = &s->text[s->m2.rm_so];
	s->mlen = s->m2.rm_so - s->m1.rm_eo;
	*s->send = 0;

	/* skip space at start */

	while (isspace(*s->sbeg)) {
		s->sbeg++;
		s->mlen--;
	}

	/* trim the end */

	char *p = strchr(s->sbeg, '<');
	if (p == NULL)
		p = s->send;

	while (isspace(*p) || *p == '<') {
		*p = 0;
		s->mlen--;
		p--;
	}

	s->m1.rm_so = s->m2.rm_eo;
	s->m1.rm_eo = s->len;

	return 1;
}

