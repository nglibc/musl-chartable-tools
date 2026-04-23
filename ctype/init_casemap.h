		int rd = 0, rt = 0, hash = 0, prev = 0;

		/* Fix sharp s, micro sign & fix circled letter symbols */
		utab[0x00df] = 0x1e9e - 0x00df;
		ltab[0x1e9e] = 0x00df - 0x1e9e;
		for (a=0x24b6; a<=0x24e9; a++) ltab[a]=utab[a]=0;

		/* Load rules used for run length encoding (max 32 dict words) */
		memcpy(dict, (short[]) {
			1, 0x1, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040,-0x0008,
			0x0030, 0x1c60,-0x0bc0, 0x87d0, 0x0009, 0xfd19, 0xf11a, 0x001c,
			0x0022, 0x0028, 0xffaa,-0x0082, 0x85fc, 0xfff7, 0x85c8, 0xfff7,
			0x03a0, 0xe2a3, 0xfd19}, 56);  /* Must sync to encode_data consts */
		assert(dict[1] == 1 && (short)dict[8] == -8);  /* Don't move 1 and -8 */

		/* Load all other rules */
		for (a = 0, dictnr = 25; a <= 0x1ffff; a++, prev = rd) {
			rd = 0, rt = 0;

#ifdef CASEMAP_SMALL  /* Omit Polytonic Greek, IPA & Cherokee codepoints */
			if (a-0x1f00U < 0x100U || a-0x0250U < 0xb0U) utab[a] = ltab[a] = 0;
			if (a-0x13a0U < 0x060U || a-0xab70U < 0x50U) utab[a] = ltab[a] = 0;
#endif
			/* Check for upper, lower or title case and for +1/-1 alt seq */
			if (ltab[a] || utab[a]) {
				int titlecase = ltab[a] && utab[a];
				rd = titlecase ? 0x8009 : (ltab[a] + utab[a]);
				rt = !!ltab[a];    /* rt=1 if uppercase or titlecase */
			}

			if (!rt && rd == -1 && a && (set[a-1] & 0xff) == 1) {
				rt = (a & 1), rd = rt ? 1:-1;
				set[a]   = set[a] & ~1   | (2 + rt);  /* 2 = Altern +/-1 rule */
				set[a-1] = set[a-1] & ~1 | (2 + rt);
			}

			/* Normalise upper/lower delta sign & hash oversized deltas */
			hash = (rd = rt ? rd:-rd) & 0xffff;
			if (rd > 0x8009 || rd < -0x7000)
				hash = rd & 0xfff | 0x8000;

			/* Lookup or add dict entry, or entry pair if oversized */
			for (b = 0; b < dictnr && dict[b] != hash; b++);
			if (dictnr >= 128 || vflag == 'V' && rd) /* Group: && rd != prev */
				printf("U+%05x\tU+%05x\tP %3d\trule:\t%6d\t%d\tdict %3d<128\n",
				       a, a+(rt?ltab[a]:utab[a]), a/64, rd, rt, dictnr);
			if (b == dictnr) {
				if (rd > 0x8009 || rd < -0x7000)
					dict[dictnr++] = hash, rd >>= 12;
				rd ? (dict[dictnr++] = rd) : (b = 0);
			}
			set[a] |= rt | b << 1 | a+(wanted == 3 ? ltab[a]:utab[a]) << 12;
		}
		dict[0] = 0;
		dict[8] = 0x8000;  /* Signals runs of 8/-8; see 8/-8 detect in encode_data() */
		dictmk = dictnr;

