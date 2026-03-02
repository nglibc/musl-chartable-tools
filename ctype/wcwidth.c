#include <wchar.h>
#include <endian.h>

#define BYTE(x,y) ((x) >> (y^(__BYTE_ORDER == __BIG_ENDIAN ? 0:24)) & 255)
#define BYTES(x)  BYTE(x,24),BYTE(x,16),BYTE(x,8),BYTE(x,0)
#define PAGE_SH   8
#define PAGE_MAX  (1u << PAGE_SH)
#define PAGEH     (0x20000 / PAGE_MAX)

const static union {
	unsigned char b[PAGEH + 185*4];
	unsigned int  w[PAGEH/4 + 185];
} table = {{
#include "wcwidth_table.h"
}};

const static unsigned short dict[216] = {
#include "wcwidth_dict.h"
};

int wcwidth_lookup(wchar_t wc)
{
	unsigned page, shfr, base, lane, rev, target;
	unsigned huff, type, popc, val;
	unsigned char ext;

	/* Direct path used in most of the 256 BMP code pages */
	page = (wc >> PAGE_SH);
	base = table.b[page];
	if (base < 2)
		return base + 1 - (((wc+1 & ~0x80) < 0x21) << !!wc);

	/* 2nd & 3rd level arrays: final level idx=popc^rev_direction */
	target = wc & (PAGE_MAX-1);
	shfr = (target & 15);
	lane = (target >> 4);
	huff = table.w[base += PAGEH/4 - 2];    /* Base 0-1 reserved */
	type = (huff >> (2 * lane)) & 3 ^ 3;
	popc = (huff << (31 - 2 * lane));
	popc = (popc << 1) & popc;    /* Count the 0b11 bit pairs */
	popc = (popc & 0x11111111) + ((popc & 0x44444444) >> 2);
	popc = (popc * 0x11111111) >> 28;
	base+= (rev = -(page & 1)) + 1;
	if (type)    /* Return value: Ns 1, Normal 2, Wide 3 */
		return type - 1;

	/* Dictionary lookup: 208 out of 216 entries are below HI_END */
	ext = (table.b + base*4)[(int)(popc^rev)];
	val = dict[ext]>>shfr & 1;
	if (ext < HI_END)
		return val + !(ext < LO_END);
	else    /* Return value: 0-3, where Control char is 0 */
		return (val << 1 | (dict[ext-1]>>shfr & 1) ^ !val) - 1;
}

int wcwidth(wchar_t wc)
{
	if (wc < 0x20000)
		return wcwidth_lookup(wc);
	if (wc-0x20000U < 0x20000)
		return 2;
	if (wc == 0xe0001 || wc-0xe0020U < 0x5f || wc-0xe0100U < 0xef)
		return 0;
	return 1;
}

