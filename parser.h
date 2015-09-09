#include <regex.h>

struct trscanner
{
	char            *text;          // text to scan
	int             len;	        // maximum length
	regex_t         p1;             // <td>
	regex_t         p2;             // </td>
	regmatch_t      m1;             // td start match
	regmatch_t      m2;             // td end matches
	char            *sbeg;          // td inner text start
	char            *send;          // td inner text end
	size_t          mlen;           // td inner text length
};

void trscanner_create(struct trscanner *s, char *text, int len);
void trscanner_destroy(struct trscanner *s);
int trscanner_next(struct trscanner *s);

void print_rex_error(int errcode, const regex_t *preg);

