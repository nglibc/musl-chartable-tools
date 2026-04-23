
		/* Remove overlaps and calc residuals (subtract run lengths) */
		if (exts[1] < exts[3] && exts[2] < exts[0] && rules[1])
			exts[2] = exts[3] = 0, rules[1] = 0;
		for (i = exts[1]; i < exts[0]; buf[i++] -= rules[0]);
		for (i = exts[2]; i < exts[3]; buf[i++] -= rules[1]);

		/* Exceptions array: 16 segments x 32b, ranges subtracted first */
		for (i = 0, e = 4; i < n; i += 4) {
			unsigned lane = i/4 % (a*8);
			unsigned len  = (memcmp(buf+i, "\00\00\00\00", 4) != 0) * 4;

			e += (memcpy(exts+e, buf+i, len), len);
			words[p][0] |= !!len << lane;
			if (vflag && len > 0)
				printf(" [%c: %02x %02x %02x %02x]",
				       65+i/4, buf[i], buf[i+1], buf[i+2], buf[i+3]);
		}
		words[p][0] |= rules[!odd]<<16 | rules[odd]<<24;
		words[p][0] |= wc>>HASH_SH<<30 | wc>>(HASH_SH+2)<<22;

