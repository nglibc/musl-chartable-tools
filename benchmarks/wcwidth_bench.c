#include <wchar.h>
#define weak_alias(a, b)

#define wcwidth wcwidth_small
#include "wcwidth.c"
#undef wcwidth

int gap[8000];

static const unsigned char ntable[] = {
#include "nonspacing.h"
};

static const unsigned char wtable[] = {
#include "wide.h"
};

int wcwidth_base(wchar_t wc)
{
	if (wc < 0xffU)
		return (wc+1 & 0x7f) >= 0x21 ? 1 : wc ? -1 : 0;
	if ((wc & 0xfffeffffU) < 0xfffe) {
		if ((ntable[ntable[wc>>8]*32+((wc&255)>>3)]>>(wc&7))&1)
			return 0;
		if ((wtable[wtable[wc>>8]*32+((wc&255)>>3)]>>(wc&7))&1)
			return 2;
		return 1;
	}
	if ((wc & 0xfffe) == 0xfffe)
		return -1;
	if (wc-0x20000U < 0x20000)
		return 2;
	if (wc == 0xe0001 || wc-0xe0020U < 0x5f || wc-0xe0100U < 0xef)
		return 0;
	return 1;
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
    const int ITERATIONS = 100, BATCHES = 200;
    unsigned test_cp;
    double min_cycles[4] = {2e50,2e50,2e50,2e50};
	int chksums[4] = {0}, chksum = 0;

	for (int j = 0; j < BATCHES; j++) {
		/* 0. Warm-up phase */
		for (int i = 0; i < 100; i++) {
		    volatile int res = wcwidth_base(i*107) + gap[i*50];
		    res = wcwidth_small(i*107) + gap[i*50];
		}

		/* 1. Measurement phase - wcwidth_base */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}

		uint64_t t0 = timer_start();
		for (int i = 0, test_cp = 0; i < ITERATIONS; i++) {
	    	test_cp = (test_cp + (chksum&31) + (chksum & 7)) & 0x3FF;
		    chksum += wcwidth_base(test_cp);    
		}
		uint64_t t1 = timer_end();
		uint64_t diff = t1 - t0;
		if (diff < min_cycles[0]) min_cycles[0] = diff;
		chksums[0] = chksum;

		/* 2. Measurement phase - wcwidth_small */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}
		t0 = timer_start();
		for (int i = 0, test_cp = 0; i < ITERATIONS; i++) {
	    	test_cp = (test_cp + (chksum&31) + (chksum & 7)) & 0x3FF;
		    chksum += wcwidth_small(test_cp);    
		}
		t1 = timer_end();
		diff = t1 - t0;
		if (diff < min_cycles[1]) min_cycles[1] = diff;
		chksums[1] = chksum;

		/* 3. Measurement phase - wcwidth_base */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}
		t0 = timer_start();
		for (int i = 0, test_cp = 0; i < ITERATIONS; i++) {
	    	test_cp = ((test_cp << (chksum&7)) + (chksum & 31) + test_cp) & 0xFFFF;
		    chksum += wcwidth_base(test_cp);    
		}
		t1 = timer_end();
		diff = t1 - t0;
		if (diff < min_cycles[2]) min_cycles[2] = diff;
		chksums[2] = chksum;

		/* 4. Measurement phase - wcwidth_small */
		chksum = 0, test_cp = 0;
		memset(hugedata, 0, sizeof hugedata);
		for (int i = 0; i < sizeof hugedata; i++) {
			hugedata[~(i + i << 8) % sizeof hugedata] = i;
			chksum += hugedata[sizeof hugedata -i];
		}
		t0 = timer_start();
		for (int i = 0, test_cp = 0; i < ITERATIONS; i++) {
	    	test_cp = ((test_cp << (chksum&7)) + (chksum & 31) + test_cp) & 0xFFFF;
		    chksum += wcwidth_small(test_cp);    
		}
		t1 = timer_end();
		diff = t1 - t0;
		if (diff < min_cycles[3]) min_cycles[3] = diff;
		chksums[3] = chksum;
	}

    printf("wcwidth_base  (cp sample range 0x0-0x3FF): chksum = %d\n", chksums[0]);
    printf("  %.2f cycles per lookup\n", min_cycles[0] / ITERATIONS);
    printf("wcwidth_small (cp sample range 0x0-0x3FF): chksum = %d\n", chksums[1]);
    printf("  %.2f cycles per lookup\n", min_cycles[1] / ITERATIONS);
    printf("wcwidth_base  (cp sample range 0-0xFFFF): chksum = %d\n", chksums[2]);
    printf("  %.2f cycles per lookup\n", min_cycles[2] / ITERATIONS);
    printf("wcwidth_small (cp sample range 0-0xFFFF): chksum = %d\n", chksums[3]);
    printf("  %.2f cycles per lookup\n", min_cycles[3] / ITERATIONS);

    return 0;
}

