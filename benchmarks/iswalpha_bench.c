#include <wctype.h>
#define weak_alias(a, b)

#define iswalpha iswalpha_small
#include "iswalpha_small.c"
#undef iswalpha

static const unsigned char atable[] = {
#include "alpha.h"
};

int gap[8000];

int iswalpha_base(wint_t wc)
{
	if (wc<0x20000U)
		return (atable[atable[wc>>8]*32+((wc&255)>>3)]>>(wc&7))&1;
	if (wc<0x2fffeU)
		return 1;
	return 0;
}

#define _GNU_SOURCE
#include <x86intrin.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* --- Intel Benchmarking Harness --- */
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
    const int ITERATIONS = 50, BATCHES = 500;
    uint32_t test_cp = 0x1AC00; // Example CP in the RLE range
    double min_cycles[4] = {2e50,2e50,2e50,2e50};
	int chksums[4] = {0}, chksum = 0;

	for (int j = 0; j < BATCHES; j++) {
		/* 0. Warm-up phase */
		for (int i = 0; i < 100; i++) {
		    volatile int res = iswalpha_base(test_cp*107) + gap[i*50];
		    res = iswalpha_small(test_cp*107) + gap[i*50];
		}

		/* 1. Measurement phase - iswalpha_base */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}

		uint64_t t0 = timer_start();
		for (int i = 0; i < ITERATIONS; i++) {
			if (i & 1)
				test_cp  = (test_cp + 64 + (i & 15)) & 0xFFF;
			else
		    	test_cp = (test_cp + (chksum&31) + (chksum & 3)) & 0xFFF;
		    chksum += iswalpha_base(test_cp+0x1200);    
		}
		uint64_t t1 = timer_end();
		uint64_t diff = t1 - t0;
		if (diff < min_cycles[0]) min_cycles[0] = diff;
		chksums[0] = chksum;

		/* 2. Measurement phase - iswalpha_small */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}
		t0 = timer_start();
		for (int i = 0; i < ITERATIONS; i++) {
			if (i & 1)
				test_cp  = (test_cp + 64 + (i & 15)) & 0xFFF;
			else
		    	test_cp = (test_cp + (chksum&31) + (chksum & 3)) & 0xFFF;
		    chksum += iswalpha_small(test_cp+0x1200);    
		}
		t1 = timer_end();
		diff = t1 - t0;
		if (diff < min_cycles[1]) min_cycles[1] = diff;
		chksums[1] = chksum;

		/* 3. Measurement phase - iswalpha_base */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}
		t0 = timer_start();
		for (int i = 0; i < ITERATIONS; i++) {
			if (i & 1)
				test_cp  = (test_cp + 64 + (i & 15)) & 0xFFF;
			else
		    	test_cp = (test_cp + (chksum&31) + (chksum & 3)) & 0xFFF;
		    chksum += iswalpha_base(test_cp);    
		}
		t1 = timer_end();
		diff = t1 - t0;
		if (diff < min_cycles[2]) min_cycles[2] = diff;
		chksums[2] = chksum;

		/* 4. Measurement phase - iswalpha_small */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}
		t0 = timer_start();
		for (int i = 0; i < ITERATIONS; i++) {
			if (i & 1)
				test_cp  = (test_cp + 64 + (i & 15)) & 0xFFF;
			else
		    	test_cp = (test_cp + (chksum&31) + (chksum & 3)) & 0xFFF;
		    chksum += iswalpha_small(test_cp);    
		}
		t1 = timer_end();
		diff = t1 - t0;
		if (diff < min_cycles[3]) min_cycles[3] = diff;
		chksums[3] = chksum;
	}

    printf("iswalpha_base  (cp sample range 0x1200-0x21FF): chksum = %d\n", chksums[0]);
    printf("  %.2f cycles per lookup\n", min_cycles[0] / ITERATIONS);
    printf("iswalpha_small (cp sample range 0x1200-0x21FF): chksum = %d\n", chksums[1]);
    printf("  %.2f cycles per lookup\n", min_cycles[1] / ITERATIONS);
    printf("iswalpha_base  (cp sample range 0-0xFFF): chksum = %d\n", chksums[2]);
    printf("  %.2f cycles per lookup\n", min_cycles[2] / ITERATIONS);
    printf("iswalpha_small (cp sample range 0-0xFFF): chksum = %d\n", chksums[3]);
    printf("  %.2f cycles per lookup\n", min_cycles[3] / ITERATIONS);

    return 0;
}

