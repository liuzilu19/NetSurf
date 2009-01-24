/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007-8 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_utils_h_
#define css_utils_h_

#include <libcss/types.h>

#include "utils/fpmath.h"

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef SLEN
/* Calculate length of a string constant */
#define SLEN(s) (sizeof((s)) - 1) /* -1 for '\0' */
#endif

#ifndef UNUSED
#define UNUSED(x) ((x)=(x))
#endif

static inline fixed number_from_css_string(const css_string *string,
		bool int_only, size_t *consumed)
{
	size_t len;
	const uint8_t *ptr;
	int sign = 1;
	int32_t intpart = 0;
	int32_t fracpart = 0;
	int32_t pwr = 1;

	if (string == NULL || string->len == 0 || consumed == NULL)
		return 0;

	len = string->len;
	ptr = string->data;

	/* number = [+-]? ([0-9]+ | [0-9]* '.' [0-9]+) */

	/* Extract sign, if any */
	if (ptr[0] == '-') {
		sign = -1;
		len--;
		ptr++;
	} else if (ptr[0] == '+') {
		len--;
		ptr++;
	}

	/* Ensure we have either a digit or a '.' followed by a digit */
	if (len == 0) {
		*consumed = 0;
		return 0;
	} else {
		if (ptr[0] == '.') {
			if (len == 1 || ptr[1] < '0' || '9' < ptr[1]) {
				*consumed = 0;
				return 0;
			}
		} else if (ptr[0] < '0' || '9' < ptr[0]) {
			*consumed = 0;
			return 0;
		}
	}

	/* Now extract intpart, assuming base 10 */
	while (len > 0) {
		/* Stop on first non-digit */
		if (ptr[0] < '0' || '9' < ptr[0])
			break;

		/* Prevent overflow of 'intpart'; proper clamping below */
		if (intpart < (1 << 22)) {
			intpart *= 10;
			intpart += ptr[0] - '0';
		}
		ptr++;
		len--;
	}

	/* And fracpart, again, assuming base 10 */
	if (int_only == false && len > 1 && ptr[0] == '.' && 
			('0' <= ptr[1] && ptr[1] <= '9')) {
		ptr++;
		len--;

		while (len > 0) {
			if (ptr[0] < '0' || '9' < ptr[0])
				break;

			if (pwr < 1000000) {
				pwr *= 10;
				fracpart *= 10;
				fracpart += ptr[0] - '0';
			}
			ptr++;
			len--;
		}
		fracpart = ((1 << 10) * fracpart + pwr/2) / pwr;
		if (fracpart >= (1 << 10)) {
			intpart++;
			fracpart &= (1 << 10) - 1;
		}
	}

	*consumed = ptr - string->data;

	if (sign > 0) {
		/* If the result is larger than we can represent,
		 * then clamp to the maximum value we can store. */
		if (intpart >= (1 << 21)) {
			intpart = (1 << 21) - 1;
			fracpart = (1 << 10) - 1;
		}
	}
	else {
		/* If the negated result is smaller than we can represent
		 * then clamp to the minimum value we can store. */
		if (intpart >= (1 << 21)) {
			intpart = -(1 << 21);
			fracpart = 0;
		}
		else {
			intpart = -intpart;
			if (fracpart) {
				fracpart = (1 << 10) - fracpart;
				intpart--;
			}
		}
	}

	return (intpart << 10) | fracpart;
}

static inline bool isDigit(uint8_t c)
{
	return '0' <= c && c <= '9';
}

static inline bool isHex(uint8_t c)
{
	return isDigit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

static inline uint32_t charToHex(uint8_t c)
{
	switch (c) {
	case 'a': case 'A':
		return 0xa;
	case 'b': case 'B':
		return 0xb;
	case 'c': case 'C':
		return 0xc;
	case 'd': case 'D':
		return 0xd;
	case 'e': case 'E':
		return 0xe;
	case 'f': case 'F':
		return 0xf;
	case '0':
		return 0x0;
	case '1':
		return 0x1;
	case '2':
		return 0x2;
	case '3':
		return 0x3;
	case '4':
		return 0x4;
	case '5':
		return 0x5;
	case '6':
		return 0x6;
	case '7':
		return 0x7;
	case '8':
		return 0x8;
	case '9':
		return 0x9;
	}

	return 0;
}

#endif
