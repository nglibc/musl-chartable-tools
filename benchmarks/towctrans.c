#include <wctype.h>
#include <endian.h>

#define ENDIAN_SH   (__BYTE_ORDER == __LITTLE_ENDIAN ? 0:24)
#define BYTE(x,y)   ((x) >> (y^ENDIAN_SH) & 255)
#define BYTES(x)    BYTE(x,0),BYTE(x,8),BYTE(x,16),BYTE(x,24)
#define ALT(r, wc)  ((r) & 1 ^ ((wc) & ((r) >> 1 == 1)))   /* Rule 2~3 = Alt +1,-1 */
#define HASH_SH     (7+PAGE_SH)
#define PAGE_SH     6
#define PAGE_MAX    (1U << PAGE_SH)
#define PAGEH       ((1U << HASH_SH-PAGE_SH) + 128)

const static union {
	unsigned char b[];
	unsigned int  w[];
} table = {{
#include "towctrans_table.h"
}};

const static short dict[] = {
#include "towctrans_dict.h"
};

static int casemap(unsigned wc, int dir)
{
	const unsigned h1 = PAGEH/4, h2 = PAGEH+4;
	unsigned page, high, base, lane, rev, target, splat;
	unsigned huff, runs, type, popc, tag, cmp, hit, rd, rt;
	unsigned char rule;

	/* Early loading of base index */
	page = (wc >> PAGE_SH);
	high = (page & ~3) == 132;
    if (page >= 1020)
        if (page < 2048) high = 1; else return wc;
	base = table.b[page & 127 | high * 128] * 2;
	rev  =-(page & 1);

	/* Sum the run length prediction + residual in exceptions array
	   Runs are in 4 lane bi-endian SWAR fmt: stop|start|start|stop */
	target = wc & (PAGE_MAX-1);
	splat = target * 0x01010101 | 0x80808080;
	lane = (target >> 2);
	runs = (table.w + h1)[base + rev + 1];
	huff = (table.w + h1)[base - rev];    /* Odd pages end at 64b align */
	cmp  = (splat - runs);
	cmp  = (cmp >> 8) ^ cmp;
	hit  = (cmp >> 4 & 8) | (cmp >> 19 & 16);    /* Hit=8|16 */
	rule = (huff >> 16 >> 16 - (hit^ENDIAN_SH)) & 63;
	tag  = (huff >> 30 | huff >> 20 & 12) == (wc >> HASH_SH);
	type = (huff >> lane & 1);     /* Huff prefix: 1=32b ext */

	/* Rules 0-15 have rd = power of 2. Rules 2-3 have alternating logic */
	if (rule >> 4 < !type | !tag) {
		rd = tag << (rule>>1) >> 1;
		rt = ALT(rule, wc);
		goto done;
	} else if (type) {
		popc = (huff << (31 - lane));
		popc = (popc << 1) - (popc & 0x55555555);    /* Max 15b set */
		popc = (popc & 0x33333333) + (popc >> 2 & 0x33333333);
		popc = (popc * 0x11111111 >> 28)*4 + (wc & 3) + 4;
		rule += (table.b + h2 + base*4)[(int)(popc^rev)];
	}

	/* Rules >= 16 need dict lookup, may have oversize or titlecase logic */
	rd = dict[rule >> 1];
	rt = ALT(rule, wc);
	if ((rd & 0xf000) == 0x8000) {
		if ((rd ^= 0xffff8000) > 9)
			rd = (rd & 0xfff) | dict[(rule >> 1) + 1] << 12;
		else
			rd -= 8, rt = rd == 1 ? !dir : (wc >> 3 & 1);    /* Greek +-8 Title +-1 */
	}
	done: return wc + (int)((rt ? rd:-rd) & -(rt^dir));
}

wint_t towlower(wint_t wc)
{
	return casemap(wc, 0);
}

wint_t towupper(wint_t wc)
{
	return casemap(wc, 1);
}

wint_t __towupper_l(wint_t c, locale_t l)
{
	return towupper(c);
}

wint_t __towlower_l(wint_t c, locale_t l)
{
	return towlower(c);
}

#if 0
weak_alias(__towupper_l, towupper_l);
weak_alias(__towlower_l, towlower_l);
#endif

