/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_core_text_h_
#define dom_core_text_h_

#include <stdbool.h>

#include <dom/core/exceptions.h>

struct dom_characterdata;
struct dom_string;
struct dom_text;

dom_exception dom_text_split_text(struct dom_text *text,
		unsigned long offset, struct dom_text **result);
dom_exception dom_text_get_is_element_content_whitespace(
		struct dom_text *text, bool *result);
dom_exception dom_text_get_whole_text(struct dom_text *text,
		struct dom_string **result);
dom_exception dom_text_replace_whole_text(struct dom_text *text,
		struct dom_string *content, struct dom_text **result);

#endif
