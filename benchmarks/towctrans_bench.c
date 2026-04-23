#include <wctype.h>
#define weak_alias(a, b)

#define casemap casemap_small
#include "towctrans.c"
#undef casemap

int gap[8000];

static const unsigned char tab[];

static const unsigned char rulebases[512];
static const int rules[];

static const unsigned char exceptions[][2];

#include "casemap.h"

static int casemap_base(unsigned c, int dir)
{
	unsigned b, x, y, v, rt, xb, xn;
	int r, rd, c0 = c;

	if (c >= 0x20000) return c;

	b = c>>8;
	c &= 255;
	x = c/3;
	y = c%3;

	/* lookup entry in two-level base-6 table */
	v = tab[tab[b]*86+x];
	static const int mt[] = { 2048, 342, 57 };
	v = (v*mt[y]>>11)%6;

	/* use the bit vector out of the tables as an index into
	 * a block-specific set of rules and decode the rule into
	 * a type and a case-mapping delta. */
	r = rules[rulebases[b]+v];
	rt = r & 255;
	rd = r >> 8;

	/* rules 0/1 are simple lower/upper case with a delta.
	 * apply according to desired mapping direction. */
	if (rt < 2) return c0 + (rd & -(rt^dir));

	/* binary search. endpoints of the binary search for
	 * this block are stored in the rule delta field. */
	xn = rd & 0xff;
	xb = (unsigned)rd >> 8;
	while (xn) {
		unsigned try = exceptions[xb+xn/2][0];
		if (try == c) {
			r = rules[exceptions[xb+xn/2][1]];
			rt = r & 255;
			rd = r >> 8;
			if (rt < 2) return c0 + (rd & -(rt^dir));
			/* Hard-coded for the four exceptional titlecase */
			return c0 + (dir ? -1 : 1);
		} else if (try > c) {
			xn /= 2;
		} else {
			xb += xn/2;
			xn -= xn/2;
		}
	}
	return c0;
}

#include <x86intrin.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static inline uint64_t timer_start(void) {
    unsigned int dummy;
    __builtin_ia32_lfence();
    return __rdtscp(&dummy);
}

static inline uint64_t timer_end(void) {
    unsigned int dummy;
    uint64_t cycles = __rdtscp(&dummy);
    __builtin_ia32_lfence();
    return cycles;
}

static char hugedata[8000 * 1000];

int main() {
    const int ITERATIONS = 50, BATCHES = 200;
    unsigned test_cp;
    double min_cycles[4] = {2e50,2e50,2e50,2e50};
	int chksums[4] = {0}, chksum = 0;

	for (int j = 0; j < BATCHES; j++) {
		/* 0. Warm-up phase */
		for (int i = 0; i < 100; i++) {
		    volatile int res = casemap_base(i*107, 0) + gap[i*50];
		    res = casemap_small(i*107, 0) + gap[i*50];
		}

		/* 1. Measurement phase - casemap_base */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}

		uint64_t t0 = timer_start();
		for (int i = 0; i < ITERATIONS; i++) {
	    	test_cp = ((test_cp << (chksum&7)) + (chksum & 31) + test_cp) & 0xFFF;
		    chksum += casemap_base(test_cp+0x1200, 0);    
		}
		uint64_t t1 = timer_end();
		uint64_t diff = t1 - t0;
		if (diff < min_cycles[0]) min_cycles[0] = diff;
		chksums[0] = chksum;

		/* 2. Measurement phase - casemap_small */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}
		t0 = timer_start();
		for (int i = 0; i < ITERATIONS; i++) {
	    	test_cp = ((test_cp << (chksum&7)) + (chksum & 31) + test_cp) & 0xFFF;
		    chksum += casemap_small(test_cp+0x1200, 0);    
		}
		t1 = timer_end();
		diff = t1 - t0;
		if (diff < min_cycles[1]) min_cycles[1] = diff;
		chksums[1] = chksum;

		/* 3. Measurement phase - casemap_base */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}
		t0 = timer_start();
		for (int i = 0; i < ITERATIONS; i++) {
	    	test_cp = ((test_cp << (chksum&7)) + (chksum & 31) + test_cp) & 0xFFFF;
		    chksum += casemap_base(test_cp, 0);
		}
		t1 = timer_end();
		diff = t1 - t0;
		if (diff < min_cycles[2]) min_cycles[2] = diff;
		chksums[2] = chksum;

		/* 4. Measurement phase - casemap_small */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}
		t0 = timer_start();
		for (int i = 0; i < ITERATIONS; i++) {
	    	test_cp = ((test_cp << (chksum&7)) + (chksum & 31) + test_cp) & 0xFFFF;
		    chksum += casemap_small(test_cp, 0);
		}
		t1 = timer_end();
		diff = t1 - t0;
		if (diff < min_cycles[3]) min_cycles[3] = diff;
		chksums[3] = chksum;
	}

    printf("casemap_base  (cp sample range 0x0-0xFFF): chksum = %d\n", chksums[0]);
    printf("  %.2f cycles per lookup\n", min_cycles[0] / ITERATIONS);
    printf("casemap_small (cp sample range 0x0-0xFFF): chksum = %d\n", chksums[1]);
    printf("  %.2f cycles per lookup\n", min_cycles[1] / ITERATIONS);
    printf("casemap_base  (cp sample range 0-0xFFFF): chksum = %d\n", chksums[2]);
    printf("  %.2f cycles per lookup\n", min_cycles[2] / ITERATIONS);
    printf("casemap_small (cp sample range 0-0xFFFF): chksum = %d\n", chksums[3]);
    printf("  %.2f cycles per lookup\n", min_cycles[3] / ITERATIONS);

    return 0;
}

