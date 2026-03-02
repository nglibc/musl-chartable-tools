#include <wctype.h>
#include <endian.h>

#define BYTE(x,y) ((x) >> (y^(__BYTE_ORDER == __BIG_ENDIAN ? 0:24)) & 255)
#define BYTES(x)  BYTE(x,24),BYTE(x,16),BYTE(x,8),BYTE(x,0)
#define PAGE_SH   8
#define PAGE_MAX  (1u << PAGE_SH)
#define PAGES     (0x20000 / PAGE_MAX)
#define PAGEH     (0x1000/8 + PAGES)   /* Direct map 0x1000 codepts */

const static union {
	unsigned char b[PAGEH + 184*4];
	unsigned int  w[PAGEH/4 + 184];
} table = {{
#include "iswalpha_table.h"
}};

const static unsigned short dict[87] = {
#include "iswalpha_dict.h"
};

int iswalpha(wint_t wc)
{
	unsigned direct, page, bmap, shfr, lane, base, rev;
	unsigned target, huff, type, popc, fast;
	signed char ext;

	/* Uncommon codepoints, skipped by branch predictor */
	if ((unsigned)wc >= 0x20000)
		return (unsigned)wc < 0x2fffe;

	/* Direct path used in 220 of the 256 BMP code pages */
	direct = wc < (PAGEH-PAGES)*8;
	page = (wc >> PAGE_SH);
	bmap = (wc >> 3) + PAGES;
	base = table.b[direct ? bmap:page];
	if (base <= direct*256 + 1)
		return base >> (wc & -direct & 7) & 1;

	/* 2nd & 3rd level arrays: final level idx=popc^rev_direction */
	target = wc & (PAGE_MAX-1);
	shfr = (target & 15);
	lane = (target >> 4);
	huff = table.w[base += PAGEH/4 - 2];     /* base 0/1 reserved */
	type = (huff >> (2 * lane)) & 3;
	popc = (huff << (31 - 2 * lane));
	popc = (popc & 0x11111111) + ((popc & 0x44444444) >> 2);
	popc = (popc * 0x11111111) >> 28;
	base+= (rev = -(page & 1)) + 1;
	ext  = (table.b + base*4)[(int)(popc^rev)];

	/* Dictionary lookup is only 1% of codepoints */
	fast = (type != 2 | ext & 1);
	if (fast) {
		shfr = (shfr + 5) & -(type >= 2);
		shfr = (shfr - (6 & -(type == 2)) + type);
		return (ext << 8 | 0xfe) >> shfr & 1;
	} else {
		return dict[ext >> 1 & 0x7f] >> shfr & 1;
	}
}

