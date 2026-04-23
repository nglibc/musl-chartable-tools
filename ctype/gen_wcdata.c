#define _XOPEN_SOURCE
#include <wchar.h>
#include <wctype.h>
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DICTM     235   /* store Ns-W full combos @ [DICTM+1,255) */
#define PAGE_MAX  256
#define PAGEH     (0x20000 / 64)
#define HASH_SH   (7+6)

/* Header max PAGEH bytes and dictionary max 256, but both may be smaller */
static unsigned short dict[256];
static unsigned char  table[PAGEH];
static unsigned char  head[PAGEH];
static unsigned words [PAGEH][PAGE_MAX + 1];
static unsigned wordnr[PAGEH];
static int dictnr = 0, dictmk = DICTM, directwc = 0;
static int vflag, tflag, Tflag;

static void read_data(unsigned wanted, int set[0x110000])
{
	FILE *f0, *f1, *f2;
	unsigned a, b, u, l;
	int *ltab = calloc(0x110000, sizeof *ltab), c;
	int *utab = calloc(0x110000, sizeof *utab);
	char buf[256], dummy, *fmt, *p;

	f0 = fopen("data/UnicodeData.txt", "rb");
	f1 = fopen("data/DerivedCoreProperties.txt", "rb");
	f2 = fopen("data/EastAsianWidth.txt", "rb");
	if (!set || !f0 || !f1 || !f2 || !ltab || !utab) {
		printf("error: no Unicode data files or invalid buffers\n");
		goto exit;
	}

	/* Nonspacing: flag = 2 (maps to type = 1), Number: 1, Punctuation: 2 */
	for (c = 0, p = buf; c != EOF; p = buf) {
		char us[16] = "", ls[16] = "", cat[16] = " ";

		while ((c = fgetc(f0)) != '\n' && c > 0 && p-buf < sizeof buf-3)
			p += sprintf(p, c == ';' ? " %c":"%c", c);

		fmt = "%x ;%*[^;];%15[^ ;] ;%*[^;];%*[^;];%*[^;];%*[^;];"
		      "%*[^;];%*[^;];%*[^;];%*[^;];%*[^;];%15[^;];%15[^;]";
		sscanf(buf, fmt, &a, cat, us, ls);
		if (*cat > ' ' && strstr("Me Mn Cf ", cat))
			set[a] = (wanted <= 2) * 2;
		else
			set[a] = (wanted <= 1) * (strcmp(cat, "Nd") ? 2:1);

		u = strtoul(us, 0, 16);
		l = strtoul(ls, 0, 16);
		if ((a | l | u) < 0x110000) {
			ltab[a] = l ? l-a : 0;
			utab[a] = u ? u-a : 0;
		}
	}

	/* Alphabetic: */
	if (wanted <= 1) while (fgets(buf, sizeof buf, f1)) {
		if (sscanf(buf, "%x..%x ; Alphabetic%c", &a, &b, &dummy)==3)
			for (; a<=b; a++) set[a]=1;
		else if (sscanf(buf, "%x ; Alphabetic%c", &a, &dummy)==2)
			set[a] = 1;
	}

	/* Wide: flag = 1 (maps to type = ~ns*2|wide^ns = 3) */
	if (wanted == 2) while (fgets(buf, sizeof buf, f2)) {
		if (sscanf(buf, "%x..%x;%*[WF]%c", &a, &b, &dummy)==3)
			for (; a<=b; a++) set[a] |= !set[a];
		else if (sscanf(buf, "%x;%*[WF]%c", &a, &dummy)==2)
			set[a] |= !set[a];
	}

	/* Misc fixes */
	if (wanted <= 2) {
		#include "init_ctype.h"
	} else {
		#include "init_casemap.h"
	}

	exit:
	fclose(f0);
	fclose(f1);
	fclose(f2);
	free(ltab);
	free(utab);
	return;
}

static int encode_data(unsigned wanted, const int set[0x110000])
{
	unsigned a = 4;  /* word alignment */
	unsigned n = wanted <= 2 ? 256:64, last = 0x20000 / n;
	unsigned p, i, j, r, d, e, err, base;

	for (p = 0, err = 0, base = 0; p < last; p++) {
		unsigned char buf[PAGE_MAX] = {0}, rules[4] = {0};
		unsigned char *exts  = (unsigned char *)words[p] + a;
		unsigned odd = p & 1, wc = p * n;
		unsigned left = 0, right = 1;
		head[p] = 255;

		/* Some code pages where direct bitmap encoding is better */
		if (wc < directwc) {
			for (i = 0; i < n; i++) {
				unsigned val = (set[wc + i] & 0xff) == 1;
				head[last + (wc+i)/8] |= val << ((wc+i) & 7);
			}
			continue;
		}

		/* Skip all-zero/all-one pages, except any NULL first page */
		for (i = 0, r = 0; i < n; r += buf[i++] == buf[0])
			buf[i] = set[wc + i] & 0xff;
		if (p >= (wanted >= 3) && r == n && buf[0] <= 1) {
			head[p] = buf[0] & 1;
			continue;
		}

		/* Sub-block runs: left (len>=4 & merged) overwrite right (len<4) */
		if (wanted >= 3) for (i = 0, r = 1; i < n; i = r) {
			unsigned rule = buf[i], z, brk;
			for (r = i+1; r < n && buf[r] == rule; r++);
			for (j = r+0; j < n && buf[j] == 0; j++);

			if (rule-1 < 55 && (r-i >= 4 || j == n)) {
				if (rule == 8*2+1)    /* Alternating +8/-8 runs */
					for (z = i; z < r && i%16 == 8; rule = --buf[z++]);
				brk = rules[left > 1] != rule || j == n;

				if (brk && left <= 1 || !left) {
					exts[left*2 + !left] = i;
					exts[left*2 +  left] = r;
					rules[left++] = rule;
				} else if (rules[left-1] == rule) {
					i = exts[left*3-3], exts[left*3-3] = r;
				}
			} else if (rule-1 < 55 && right >= left) {
				exts[right*2 + !right] = i;
				exts[right*2 +  right] = r;
				rules[right] = rule, right -= !!right;
			}
		}

		/* Mixed block: encoding is specific to wanted set */
		if (vflag)
			printf("page U+%05x: [0x%02x-%02x=%2x,%02x-%02x=%2x]",
			       wc, exts[1], exts[0], rules[0], exts[2], exts[3], rules[1]);
		if (wanted <= 2) {
			#include "enc_ctype.h"
		} else {
			#include "enc_casemap.h"
		}
		wordnr[p] = e;
		if (vflag)
			printf(e > 0 ? "\n\0  <%d>%s\n":"%.*s\n", base += e+4, "");
	}

	if (!err && wanted == 2) {
		/* Compact dict: close gap btw lo & hi vals, set dictnr=num lo+hi+combos */
		for (e = dictmk, d = e-1; d >= 0 && !dict[d]; d--);
		memmove(dict+(++d), dict+e, sizeof dict - e*sizeof dict[0]);
		dictnr = d;
	}
	printf("\n%.*s"+!vflag, err*20, err<2 ? "error: ~ns=0\n":"error: dict full\n");
	return err;
}

static int export_table(int wanted, int header_only)
{
	unsigned a = wanted <= 2 ? 4:8;
	unsigned n = wanted <= 2 ? 256:64;
	unsigned start = directwc/n, last = 0x20000/n;
	unsigned tabnr = wanted <= 2 ? 2*a:0;    /* Reserve base 0-1 */
	char     buf[PAGEH * (40 + 20 + 4*n/16)], *bp = buf;
	int      p, q, i, j, *map;

	for (p = start, q = 0; p < last || q > 0; p += 2) {
		int threshold = 0, pad = 0, dictgap = dictmk - dictnr;
		int l = p < last ? wordnr[p]:0, m = 0, r = 0;
		if (p < last && head[p] != 255)
			continue;

		/* Match even page p with odd page q so that padding is minimised */
		for (q = start+1; q < last; q += 2) {
			pad = (a-1) - (l + (m = wordnr[q]) + (a-4)*(p>=last) + a-1) % a;
			if (head[q] == 255 && pad <= threshold)
				break;
			if (q+2 > last)
				threshold++, q = start-1;
			if (threshold > a) {
				pad = (a-1) - (l + (m = 0) + (a-4) + a-1) % a;
				q = -1;
				break;
			}
		}
		if (tabnr+8+a+l+m >= 256*a || bp+(l+m)*4+120-buf > sizeof buf) {
			printf("error: data (%d,%d) too big\n", tabnr+l+m, (int)(bp-buf)+(l+m)*4);
			return 1;
		}

		/* Write table data for even page, then alignment padding */
		if (p < last || q > 0)
			bp += sprintf(bp, "\n\n/* wcdata: U+%05x,%05x idx [%d-%d) */\n",
			              p * n * (p<last), q * n * (q>0),
			              tabnr/a, tabnr/a+(l+m+pad+4*(q>0)+4*(p<last))/a);
		if (p < last) {
			bp += sprintf(bp, "BYTES(0x%08x),", words[p][0]);
			head[p] = tabnr/a;
			tabnr += 4;
			for (j = 0; j < l; j++, tabnr++) {
				unsigned char val = ((unsigned char *)words[p])[4 + j];
				if (val >= dictnr) val -= dictgap;
				bp += sprintf(bp, "%d%s", val, r++%16==15 ? ",\n":",");
			}
		}
		for (i = 0; i < pad; i++, tabnr++)
			bp += sprintf(bp, "0,");

		/* Write table data for odd page */
		if (q > 0) {
			for (j = m-1; j >= 0; j--, tabnr++) {
				unsigned char val = ((unsigned char *)words[q])[4 + j];
				if (val >= dictnr) val -= dictgap;
				bp += sprintf(bp, "%d%s", val, r++%16==15 ? ",\n":",");
			}
			bp += sprintf(bp, "BYTES(0x%08x),", words[q][0]);
			head[q] = tabnr/a;
			tabnr += 4;
		}
	}

	map = wanted<=2 ? (int[]) {0, 512, 0, 512}
	                : (int[]) {0, 136, 0, 136, 136, 884, 0, 128, 1020, 1028, 128, 128};
	for (i = 0; i < last + directwc/8; map += 4) {
		for (i = map[0]; i < (map[1] + directwc/8) + map[0]; i++) {
			j = map[2] + i % (map[3] + directwc/8);
			if (table[j] && head[i])
				return printf("error: hash collision at %d+%d\n", map[0], map[1]), 1;
			table[j] += head[i];
		}
	}

	printf("/* wcdata: pages %d x %d, bmap 0x%x */\n", last, n, directwc);
	for (p = 0; p <= j; p++) {
		printf("%3d%s", table[p], p % 16 == 15 ? ",\n":",");
	}
	printf("%s\n", header_only ? "":buf);
	return 0;
}

static int export_dict()
{
	int i, max = dictnr;
	while (dict[max] && ++max < sizeof dict/sizeof dict[0]);

	/* Print lo vals [0-LO_END), hi vals [LO_END-HI_END) & combo vals beyond */
	printf("/* wcdata: dictionary %d entries*/\n", max);
	printf("#define LO_END %d\n", dictnr);
	printf("#define HI_END %d\n", DICTM + 1 - (dictmk - dictnr));

	for (i = 0; i < max; i++)
		printf("0x%x%s", dict[i], i%8 == 7 ? ",\n":",");
	return printf("\n"), 0;
}

static void verify_data(int wanted, const int set[0x110000], unsigned wc)
{
	unsigned log, fails, tests;
	char wcmap[] = {3, 2, 0, 1};

	for (log = fails = tests = 0; wc < 0x20000; wc++, tests++) {
		int exp = set[wc] >> 12;
		int got = wanted >= 4 ? towupper(wc)
				: wanted == 3 ? towlower(wc)
				: wanted == 2 ? wcmap[wcwidth(wc)+1]
				: wanted == 1 ? !!iswpunct(wc) : !!iswalpha(wc);

		if (exp != got)
			log += !log, fails++;

		if (log && log++ <= 20) {
			char *msg = (exp == got) ? "PASS":"FAIL", direct = wc < directwc;

			printf("/* %s U+%05X: exp %d got %d index %c */\n", 
			       msg, wc, exp, got, 'b'+!direct);
		}
	}
	printf("/* wcdata: %u codepoints verified, %u errors */\n", tests, fails);
}

int main(int argc, char *argv[])
{
	const char *help = "usage: %s [-v][-t][-T] a|A|p|P|w|W|l|L|u|U|c|C\n"; 
	const char *cmds = "apwluc";
	char *cmd, *arg0;
	int  *set = calloc(0x110000, sizeof *set), wanted;

	setlocale(LC_CTYPE, "");
	for (arg0 = *argv++; *argv && **argv == '-' && (*argv)++; argv++) {
		if ((**argv | 32) == 'v')  /* Verbose output */
			vflag = **argv;
		else if (**argv == 't')  /* Test output */
			tflag = 1;
		else if (**argv == 'T')  /* Test with header output */
			Tflag = 1;
		else break;
	}

	cmd = *argv ? strchr(cmds, **argv | 32) : NULL;
	if (cmd && set) {
		read_data(wanted = cmd - cmds, set);

		/* Encode data for export or test header log output */
		if (wanted <= 1)
			directwc = wanted ? 0x800:0x1000;
		if (!tflag && !encode_data(wanted, set))
			**argv >= 'a' ? export_table(wanted, Tflag):export_dict();

		/* Verify starting at codepoint given by arg or 0x0 */
		if (tflag || Tflag)
			verify_data(wanted, set, *++argv ? strtoul(*argv, NULL, 0):0);
	} else {
		fprintf(stderr, set ? help:"error:%s:mem alloc failed", arg0);
	}
	free(set);
	return 0;
}

