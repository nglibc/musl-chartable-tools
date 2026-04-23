
	/* Control chars: flag = 3 (maps to type = ~ns*2|wide^ns = 0) */
	for (a=0x00; a<=0x1f; a++) set[a]=3 << 12;
	for (a=0x7f; a<=0x9f; a++) set[a]=3 << 12;
	for (a=0xfffe; a<=0xffff; a++) set[a]=3, set[a|0x10000]=3;

	/* Misc fixes */
	if (wanted <= 1) {
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
			set[a] = (set[a]==wanted+1) * (1 | 1 << 12);
	} else {
		/* Nonspacing: Hangul vowel/trailing, NULL, 0xad */
		for (a=0x1160; a<=0x11ff; a++) set[a] = 2;
		for (a=0xd7b0; a<=0xd7ff; a++) set[a] = 2;
		set[0]=2 << 12; set[0xad]=0;

		for (a=0; a<=0x10ffff; a++)
			set[a] |= (set[a] & 0xff) << 12;  /* type = ~ns*2|wide^ns */
	}

