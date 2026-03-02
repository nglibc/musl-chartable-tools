#define _XOPEN_SOURCE
#include <wchar.h>
#include <wctype.h>
#include <endian.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PAGE_SH   8
#define PAGE_MAX  (1u << PAGE_SH)
#define PAGES     (0x20000 / PAGE_MAX)
#define PAGEH     (PAGES + 0x1000/8)
#define DICTM     235   /* store Ns-W full combos @ [DICTM+1,255) */

/* Header max PAGEH bytes and dictionary max 256, but both may be smaller */
static unsigned char  head[PAGEH];
static unsigned short dict[256];
static unsigned words [PAGES][PAGE_MAX/16 + 1];
static unsigned wordnr[PAGES];
static int dictnr = 0, dictmk = DICTM, directwc = 0;
static int vflag, tflag, Tflag;

static void read_data(unsigned wanted, char set[0x110000])
{
	char buf[128], dummy;
	unsigned a, b;
	FILE *f0, *f1, *f2;

	f0 = fopen("data/UnicodeData.txt", "rb");
	f1 = fopen("data/DerivedCoreProperties.txt", "rb");
	f2 = fopen("data/EastAsianWidth.txt", "rb");
	if (!set || !f0 || !f1 || !f2 || wanted<1) {
		printf("error: no Unicode data files or invalid set\n");
		goto exit;
	}

	/* Number (decimal) & punctuation: */
	if (wanted < 3) while (fgets(buf, sizeof buf, f0)) {
		if (sscanf(buf, "%x;%*[^;];Nd%c", &a, &dummy)==2)
			set[a] = 1;
		else if (sscanf(buf, "%x;%*[^;];%c", &a, &dummy)==2)
			set[a] = 2;
	}

	/* Alphabetic: */
	if (wanted < 3) while (fgets(buf, sizeof buf, f1)) {
		if (sscanf(buf, "%x..%x ; Alphabetic%c", &a, &b, &dummy)==3)
			for (; a<=b; a++) set[a]=1;
		else if (sscanf(buf, "%x ; Alphabetic%c", &a, &dummy)==2)
			set[a] = 1;
	}

	/* Wide: flag = 1 (maps to type = ~ns*2|wide^ns = 3) */
	if (wanted >= 3) while (fgets(buf, sizeof buf, f2)) {
		if (sscanf(buf, "%x..%x;%*[WF]%c", &a, &b, &dummy)==3)
			for (; a<=b; a++) set[a] = 1;
		else if (sscanf(buf, "%x;%*[WF]%c", &a, &dummy)==2)
			set[a] = 1;
	}

	/* Nonspacing: flag = 2 (maps to type = ~ns*2|wide^ns = 1) */
	if (wanted >= 3) while (fgets(buf, sizeof buf, f0)) {
		if (sscanf(buf, "%x;%*[^;];M%*[en]%c", &a, &dummy)==2)
			set[a] = 2;
		else if (sscanf(buf, "%x;%*[^;];Cf%c", &a, &dummy)==2)
			set[a] = 2;
	}

	/* Control chars: flag = 3 (maps to type = ~ns*2|wide^ns = 0) */
	for (a=0x00; a<=0x1f; a++) set[a]=3*(wanted==4);
	for (a=0x7f; a<=0x9f; a++) set[a]=3*(wanted==4);
	for (a=0xfffe; a<=0xffff; a++) set[a]=3, set[a|0x10000]=3;

	/* Misc fixes */
	if (wanted < 3) {
		/* Fix misclassified Thai characters */
		set[0xe2f] = set[0xe46] = 2;

		/* Clear digits */
		for (a=0x30; a<=0x39; a++) set[a]=0;

		/* Clear spaces */
		set[0x0020] = 0;
		for (a=0x2000; a<=0x2006; a++) set[a]=0;
		for (a=0x2008; a<=0x200a; a++) set[a]=0;
		set[0x205f] = 0;
		set[0x3000] = 0;

		/* Clear additional controls */
		for (a=0x2028; a<=0x2029; a++) set[a]=0;
		for (a=0xfff9; a<=0xfffb; a++) set[a]=0;
		for (a=0xd800; a<=0xdfff; a++) set[a]=0;

		/* Fill in elided CJK ranges */
		for (a =0x3400; a<=0x4db5; a++) set[a]=1;
		for (a =0x4e00; a<=0x9fcc; a++) set[a]=1;
		for (a =0xac00; a<=0xd7a3; a++) set[a]=1;
		for (a =0x20000; a<=0x2a6d6; a++) set[a]=1;
		for (a =0x2a700; a<=0x2b734; a++) set[a]=1;
		for (a =0x2b740; a<=0x2b81d; a++) set[a]=1;

		for (a=0; a<=0x10ffff; a++)
			set[a] = (set[a] == wanted);
	} else {
		/* Nonspacing: Hangul vowel/trailing, NULL, 0xad */
		for (a=0x1160; a<=0x11ff; a++) set[a] = 2;
		for (a=0xd7b0; a<=0xd7ff; a++) set[a] = 2;
		set[0]=2*(wanted==4); set[0xad]=0;
	}

	exit:
	fclose(f0);
	fclose(f1);
	fclose(f2);
	return;
}

static int encode_data(unsigned wanted, const char set[0x110000])
{
	unsigned a = sizeof(unsigned);
	int      p, i, j, d, e, err;
	memset(head,   0, sizeof head);
	memset(dict,   0, sizeof dict);
	memset(words,  0, sizeof words);
	memset(wordnr, 0, sizeof wordnr);

	for (p=0, err=0; p < PAGES; p++) {
		unsigned wc = p * PAGE_MAX;
		char  *exts = (char *)words[p] + a;

		/* Some code pages where direct bitmap encoding is better */
		if (wc < directwc) {
			for (i = 0; i < PAGE_MAX; i++) {
				unsigned val = set[wc + i] == 1;
				head[PAGES + (wc+i)/8] |= val << ((wc+i) & 7);
			}
			continue;
		}

		/* Skip all-one or all-zero pages */
		head[p] = 255;
		for (i = 0; i < PAGE_MAX && set[wc+i] == set[wc]; i++);
		if (i == PAGE_MAX && set[wc] <= 1) {
			head[p] = set[wc];
			continue;
		}

		/* Mixed block: split page into 16 cp sets, create dict & exts entries */
		if (vflag)
			printf("\npage U+%05x: ", wc);
		for (i = 0, e = 0; i<PAGE_MAX/16; i++) {
			unsigned word = 0, wide = 0, ns = 0;
			unsigned quad = 8, ext = 0, shfl = i*2 % (a*8);

			for (j = 0; j < 16; j++) {
				ns   |= set[wc + i*16 + j]/2 << j; /* Ns 0=>type 2, 2=>type0/1 */
				wide |= set[wc + i*16 + j]%2 << j; /* wctype attr/Asian width  */
			}

			word = ns << 16 | wide;  /* if all set[wc] <= 1, then word=wide */
			if (wanted < 3) {
				unsigned pat_lo = wanted == 1 ? 255:0, pat_hi = word & 0xff80;

				if (word == 0x0000 || word == 0xffff) {
					words[p][i/(4*a)] |= (word&1) << shfl;
					continue;
				}

				if (word <= 0x3fff || word >= 0xc000) {
					if (pat_hi == 0 || pat_hi == 0xff80)  /* Store lower 8b */
						quad = 2, ext = word & 0xff;
					else if ((word & 0xff) == pat_lo)     /* Store upper 7b */
						quad = pat_lo & 1, ext = word>>7 & 0xfe;
				}

				if (!err && quad == 8) {
					for (j = 0; j < dictnr && dict[j] != word; j++);
					if (j == dictnr)
						j < 128 ? (dict[dictnr++] = word) : (err = 2);
					quad = ~pat_lo & 1, ext = j << 1, dictmk = dictnr;
				}
			} else {
				unsigned type = (~ns & 1)*2 | (wide^ns) & 1;

				if (word == 0 || word == 0xffff || word == 0xffff0000) {
					words[p][i/(4*a)] |= (type^3) << shfl;  /* Store type xor 3 */
					continue;
				}

				for (j = wide ? DICTM:-1; !err; ) {
					if (ns && ++j>=dictmk && (!wide || ++j>254))  /* Ns_W: j+=2 */
						err |= 2;
					if (!dict[j] || dict[j]==(wide ? wide:~ns & 0xffff))
						break;
					if (!ns && --j<dictmk && dict[dictmk=j])
						err |= 2;
				}

				if (!err && !dict[ext = j]) {
					err |= (~ns & 0xffff) == 0;  /* zero vals are illegal in dict */
					if (ns)   dict[j--] = ~ns;   /* idx Ns>=0 (stored as inverse) */
					if (wide) dict[j--] = wide;  /* idx Wide<=DICTM, Ns_W>DICTM   */
				}
			}
			words[p][i/(4*a)] |= (quad >= 2 ? 3:2) << shfl;
			exts[e++] = ext | quad & 1;

			if (vflag && (!ns || !wide))
				printf("0%c%04x [%02x] ", 'x' - !!ns*32, ns | wide, ext);
			else if (vflag)
				printf("0x%08x [%02x] ", ns<<16 | wide, ext);
		}
		wordnr[p] = e;
	}

	if (!err && wanted >= 3) {
		/* Compact dict: close gap btw lo & hi vals, set dictnr=num lo+hi+combos */
		for (e = dictmk, d = e-1; d >= 0 && !dict[d]; d--);
		memmove(dict+(++d), dict+e, sizeof dict - e*sizeof dict[0]);
		dictnr = d;
	}
	printf("\n%.*s"+!vflag, err*20, err<2 ? "error: ~ns=0\n":"error: dict full\n");
	return err;
}

static int export_table(int quiet)
{
	unsigned a = sizeof(unsigned);
	unsigned start = directwc/PAGE_MAX;
	unsigned tabnr = 2*a;    /* Base 0-1 reserved */
	int      p, q, i, j;
	char     buf[PAGES * (40 + 20 + 4*PAGE_MAX/16)], *bp = buf;

	for (p = start, q = 0; p < PAGES || q >= 0; p += 2) {
		int threshold = 0, pad = 0, dictgap = dictmk - dictnr;
		int l = p < PAGES ? wordnr[p]:0, m = 0;
		if (p < PAGES && head[p] != 255)
			continue;

		/* Match even page p with odd page q so that padding is minimised */
		for (q = start+1; q < PAGES; q += 2) {
			pad = (a-1) - (l + (m = wordnr[q]) + a-1) % a;
			if (head[q] == 255 && pad <= threshold)
				break;
			if (q+2 > PAGES)
				threshold++, q = start-1;
			if (threshold > a) {
				pad = (a-1) - (l + (m = 0) + a-1) % a;
				q = -1;
				break;
			}
		}
		if (tabnr+3*a+l+m >= 256*a || bp+(l+m)*4+120-buf > sizeof buf) {
			printf("error: data too big or resize buffer to fit data\n");
			return 1;
		}

		/* Write table data for even page, then alignment padding */
		if (p < PAGES || q >= 0)
			bp += sprintf(bp, "\n\n/* wctype: U+%05x,%05x idx [%d-%d) */\n",
			              p*PAGE_MAX*(p<PAGES), q*PAGE_MAX*(q>=0),
			              tabnr/a-2, tabnr/a-2+(l+m+pad)/a+(q>=0)+(p<PAGES));
		if (p < PAGES) {
			bp += sprintf(bp, "BYTES(0x%08x),", words[p][0]);
			head[p] = tabnr/a;
			tabnr += a;
			for (j = 0; j < l; j++, tabnr++) {
				unsigned char *v = (unsigned char *)words[p] + a + j;
				bp += sprintf(bp, "%d,", *v>=dictnr ? *v-dictgap:*v);
			}
		}
		for (i = 0; i < pad; i++, tabnr++)
			bp += sprintf(bp, "0,");

		/* Write table data for odd page */
		if (q >= 0) {
			for (j = m-1; j >= 0; j--, tabnr++) {
				unsigned char *v = (unsigned char *)words[q] + a + j;
				bp += sprintf(bp, "%d,", *v>=dictnr ? *v-dictgap:*v);
			}
			bp += sprintf(bp, "BYTES(0x%08x),", words[q][0]);
			head[q] = tabnr/a;
			tabnr += a;
		}
	}

	printf("/* wctype: pages %d x %d, bmap 0x%x */\n", PAGES, PAGE_MAX, directwc);
	for (p = 0; p < PAGES + directwc/8; p++)
		printf("%3d%s", head[p], p%16 == 15 ? ",\n":",");
	if (!quiet)
		printf("%s\n", buf);
	return 0;
}

static int export_dict()
{
	int i, max = dictnr;
	while (dict[max] && ++max < sizeof dict/sizeof dict[0]);

	/* Print lo vals [0-LO_END), hi vals [LO_END-HI_END) & combo vals beyond */
	printf("/* wctype: dictionary %d entries*/\n", max);
	printf("#define LO_END %d\n", dictnr);
	printf("#define HI_END %d\n", DICTM + 1 - (dictmk - dictnr));

	for (i = 0; i < max; i++)
		printf("0x%x%s", dict[i], i%8 == 7 ? ",\n":",");
	return printf("\n"), 0;
}

static void verify_data(int wanted, const char set[0x110000], unsigned wc)
{
	unsigned log, fails, tests;
	char wcmap[] = {3, 2, 0, 1};

	for (log = fails = tests = 0; wc < 0x20000; wc++, tests++) {
		int exp = set[wc];
		int got = wanted == 1 ? !!iswalpha(wc)
	            : wanted == 2 ? !!iswpunct(wc) : wcmap[wcwidth(wc)+1];
		if (exp != got)
			log += !log, fails++;

		if (log && log++ <= 20) {
			unsigned direct, page, lane;
			unsigned target;
			char *msg = (exp == got) ? "PASS":"FAIL";

			direct = wc < directwc;
			target = wc & (PAGE_MAX-1);
			page = !direct ? (wc >> PAGE_SH) : (wc>>3) + PAGES;
			lane = (target >> 4);
			printf("/* %s U+%05X: exp %d got %d page %03x %c(%02x) "
			       "target 0x%03x lane %d */\n", 
				   msg, wc, exp, got, page, 'b'+!direct, head[page], target, lane);
		}
	}
	printf("/* wctype: %u codepoints verified, %u errors */\n", tests, fails);
}

int main(int argc, char *argv[])
{
	char *cmds = "apwv", *set = calloc(0x110000,1);
	char *cmd, *arg0;

	for (arg0 = *argv++; *argv && **argv == '-'; argv++) {
		if (strcmp(*argv, "-v") == 0)       /* Verbose output */
			vflag = 1;
		else if (strcmp(*argv, "-t") == 0)  /* Test output */
			tflag = 1;
		else if (strcmp(*argv, "-T") == 0)  /* Test with header output */
			Tflag = 1;
		else break;
	}

	cmd = strchr(cmds, **argv | 32);
	if (cmd) {
		int wanted = cmd - cmds + 1;
		read_data(wanted, set);

		/* Encode data for export or test header log output */
		if (wanted < 3)
			directwc = wanted == 1 ? 0x1000:0x800;
		if (!tflag && !encode_data(wanted, set))
			**argv >= 'a' ? export_table(Tflag):export_dict();

		/* Verify starting at codepoint given by arg or 0x0 */
		if (tflag || Tflag)
			verify_data(wanted, set, *++argv ? strtoul(*argv, NULL, 0):0);
	} else {
		fprintf(stderr, "usage: %s [-v][-t][-T] a|A|p|P|w|W|v|W\n", arg0);
	}
	free(set);
	return 0;
}

