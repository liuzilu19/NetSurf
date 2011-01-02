/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_parse_properties_utils_h_
#define css_parse_properties_utils_h_

#include "parse/language.h"

css_error parse_pause_common(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
			     uint16_t op, css_style **result);

css_error parse_list_style_type_value(css_language *c,
		const css_token *token, uint16_t *value);
css_error parse_content_list(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t *value, uint8_t *buffer, uint32_t *buflen);
css_error parse_counter_common(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);


enum { BORDER_SIDE_TOP = 0, BORDER_SIDE_RIGHT = 1, BORDER_SIDE_BOTTOM = 2, BORDER_SIDE_LEFT = 3 };

css_error parse_border_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint32_t side, css_style **result);
css_error parse_border_side_color(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);
css_error parse_border_side_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);
css_error parse_border_side_width(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);

css_error parse_margin_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);

css_error parse_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);

css_error parse_padding_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);

css_error parse_colour_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint32_t *result);
css_error parse_named_colour(css_language *c, lwc_string *data, 
		uint32_t *result);
css_error parse_hash_colour(lwc_string *data, uint32_t *result);

css_error parse_unit_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint32_t default_unit,
		css_fixed *length, uint32_t *unit);
css_error parse_unit_keyword(const char *ptr, size_t len, 
		css_unit *unit);

css_error comma_list_length(css_language *c,
		const parserutils_vector *vector, int *ctx,
		const css_token *token, 
		bool (*reserved)(css_language *c, const css_token *ident),
		uint32_t *size);
css_error comma_list_to_bytecode(css_language *c,
		const parserutils_vector *vector, int *ctx,
		const css_token *token, 
		bool (*reserved)(css_language *c, const css_token *ident),
		uint16_t (*get_value)(css_language *c, const css_token *token),
		uint8_t **bytecode);

#endif
