
		/* Mixed block: split page into 16 cp sets, create dict & exts entries */
		for (i = 0, e = 0; i < n/16; i++) {
			unsigned word = 0, wide = 0, ns = 0;
			unsigned quad = 8, ext = 0, shfl = i*2 % (a*8);

			for (j = 0; j < 16; j++) {
				ns   |= buf[i*16 + j]/2 << j; /* Ns 0=>type 2, 2=>type0/1 */
				wide |= buf[i*16 + j]%2 << j; /* wctype attr/Asian width  */
			}

			word = ns << 16 | wide;  /* If all buf[0-15] <= 1, then word=wide */
			if (wanted <= 1) {
				unsigned pat_lo = wanted ? 0:255, pat_hi = word & 0xff80;

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
					continue;    /* After this, Ns or Wide or both are nonzero */
				}

				for (j = wide ? DICTM:-1; !err; ) {
					if (ns && ++j>=dictmk && (!wide || ++j>254))  /* j+=1 if Ns, j+=2 if Ns_W */
						err |= 2;
					if (!dict[j] || dict[j]==(wide ? wide:~ns & 0xffff))
						break;
					if (!ns && --j<dictmk && dict[dictmk=j])  /* j-=1 if Wide */
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
				printf(" 0%c%04x [%c %02x]", 'x' - !!ns*32, ns | wide, 'A'+i, ext);
			else if (vflag)
				printf(" 0x%08x [%c %02x]", ns<<16 | wide, 'A'+i, ext);
		}

