/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

void mymsg(uint32_t severity, void *ctx, const char *msg, ...)
{
	va_list l;

	UNUSED(ctx);

	va_start(l, msg);

	fprintf(stderr, "%d: ", severity);
	vfprintf(stderr, msg, l);
	fprintf(stderr, "\n");
}


