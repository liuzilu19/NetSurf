/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"

static inline css_error parse_important(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint8_t *result);
static inline css_error parse_colour_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint32_t *result);
static css_error parse_named_colour(css_language *c, lwc_string *data, 
		uint32_t *result);
static inline css_error parse_hash_colour(lwc_string *data, uint32_t *result);
static inline css_error parse_unit_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint32_t default_unit,
		css_fixed *length, uint32_t *unit);
static inline css_error parse_unit_keyword(const char *ptr, size_t len, 
		css_unit *unit);

static inline css_error parse_border_side_color(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);
static inline css_error parse_border_side_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);
static inline css_error parse_border_side_width(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);
static inline css_error parse_margin_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);
static inline css_error parse_padding_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result);
static inline css_error parse_list_style_type_value(css_language *c,
		const css_token *token, uint16_t *value);
static inline css_error parse_content_list(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t *value, uint8_t *buffer, uint32_t *buflen);

/**
 * Dispatch table of property handlers, indexed by property enum
 */
const css_prop_handler property_handlers[LAST_PROP + 1 - FIRST_PROP] =
{
	parse_azimuth,
	parse_background_attachment,
	parse_background_color,
	parse_background_image,
	parse_background_position,
	parse_background_repeat,
	parse_border_bottom_color,
	parse_border_bottom_style,
	parse_border_bottom_width,
	parse_border_collapse,
	parse_border_left_color,
	parse_border_left_style,
	parse_border_left_width,
	parse_border_right_color,
	parse_border_right_style,
	parse_border_right_width,
	parse_border_spacing,
	parse_border_top_color,
	parse_border_top_style,
	parse_border_top_width,
	parse_bottom,
	parse_caption_side,
	parse_clear,
	parse_clip,
	parse_color,
	parse_content,
	parse_counter_increment,
	parse_counter_reset,
	parse_cue_after,
	parse_cue_before,
	parse_cursor,
	parse_direction,
	parse_display,
	parse_elevation,
	parse_empty_cells,
	parse_float,
	parse_font_family,
	parse_font_size,
	parse_font_style,
	parse_font_variant,
	parse_font_weight,
	parse_height,
	parse_left,
	parse_letter_spacing,
	parse_line_height,
	parse_list_style_image,
	parse_list_style_position,
	parse_list_style_type,
	parse_margin_bottom,
	parse_margin_left,
	parse_margin_right,
	parse_margin_top,
	parse_max_height,
	parse_max_width,
	parse_min_height,
	parse_min_width,
	parse_orphans,
	parse_outline_color,
	parse_outline_style,
	parse_outline_width,
	parse_overflow,
	parse_padding_bottom,
	parse_padding_left,
	parse_padding_right,
	parse_padding_top,
	parse_page_break_after,
	parse_page_break_before,
	parse_page_break_inside,
	parse_pause_after,
	parse_pause_before,
	parse_pitch_range,
	parse_pitch,
	parse_play_during,
	parse_position,
	parse_quotes,
	parse_richness,
	parse_right,
	parse_speak_header,
	parse_speak_numeral,
	parse_speak_punctuation,
	parse_speak,
	parse_speech_rate,
	parse_stress,
	parse_table_layout,
	parse_text_align,
	parse_text_decoration,
	parse_text_indent,
	parse_text_transform,
	parse_top,
	parse_unicode_bidi,
	parse_vertical_align,
	parse_visibility,
	parse_voice_family,
	parse_volume,
	parse_white_space,
	parse_widows,
	parse_width,
	parse_word_spacing,
	parse_z_index,
};

css_error parse_azimuth(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* angle | [ IDENT(left-side, far-left, left, center-left, center, 
	 *                 center-right, right, far-right, right-side) || 
	 *           IDENT(behind) 
	 *         ] 
	 *       | IDENT(leftwards, rightwards, inherit)
	 */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[LEFTWARDS]) {
		parserutils_vector_iterate(vector, ctx);
		value = AZIMUTH_LEFTWARDS;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[RIGHTWARDS]) {
		parserutils_vector_iterate(vector, ctx);
		value = AZIMUTH_RIGHTWARDS;
	} else if (token->type == CSS_TOKEN_IDENT) {
		parserutils_vector_iterate(vector, ctx);

		/* Now, we may have one of the other keywords or behind,
		 * potentially followed by behind or other keyword, 
		 * respectively */
		if (token->ilower == c->strings[LEFT_SIDE]) {
			value = AZIMUTH_LEFT_SIDE;
		} else if (token->ilower == c->strings[FAR_LEFT]) {
			value = AZIMUTH_FAR_LEFT;
		} else if (token->ilower == c->strings[LEFT]) {
			value = AZIMUTH_LEFT;
		} else if (token->ilower == c->strings[CENTER_LEFT]) {
			value = AZIMUTH_CENTER_LEFT;
		} else if (token->ilower == c->strings[CENTER]) {
			value = AZIMUTH_CENTER;
		} else if (token->ilower == c->strings[CENTER_RIGHT]) {
			value = AZIMUTH_CENTER_RIGHT;
		} else if (token->ilower == c->strings[RIGHT]) {
			value = AZIMUTH_RIGHT;
		} else if (token->ilower == c->strings[FAR_RIGHT]) {
			value = AZIMUTH_FAR_RIGHT;
		} else if (token->ilower == c->strings[RIGHT_SIDE]) {
			value = AZIMUTH_RIGHT_SIDE;
		} else if (token->ilower == c->strings[BEHIND]) {
			value = AZIMUTH_BEHIND;
		} else {
			return CSS_INVALID;
		}

		consumeWhitespace(vector, ctx);

		/* Get potential following token */
		token = parserutils_vector_peek(vector, *ctx);
		if (token != NULL && token->type != CSS_TOKEN_IDENT &&
				tokenIsChar(token, '!') == false)
			return CSS_INVALID;

		if (token != NULL && token->type == CSS_TOKEN_IDENT &&
				value == AZIMUTH_BEHIND) {
			parserutils_vector_iterate(vector, ctx);

			if (token->ilower == c->strings[LEFT_SIDE]) {
				value |= AZIMUTH_LEFT_SIDE;
			} else if (token->ilower == c->strings[FAR_LEFT]) {
				value |= AZIMUTH_FAR_LEFT;
			} else if (token->ilower == c->strings[LEFT]) {
				value |= AZIMUTH_LEFT;
			} else if (token->ilower == c->strings[CENTER-LEFT]) {
				value |= AZIMUTH_CENTER_LEFT;
			} else if (token->ilower == c->strings[CENTER]) {
				value |= AZIMUTH_CENTER;
			} else if (token->ilower == c->strings[CENTER-RIGHT]) {
				value |= AZIMUTH_CENTER_RIGHT;
			} else if (token->ilower == c->strings[RIGHT]) {
				value |= AZIMUTH_RIGHT;
			} else if (token->ilower == c->strings[FAR_RIGHT]) {
				value |= AZIMUTH_FAR_RIGHT;
			} else if (token->ilower == c->strings[RIGHT_SIDE]) {
				value |= AZIMUTH_RIGHT_SIDE;
			} else {
				return CSS_INVALID;
			}
		} else if (token != NULL && token->type == CSS_TOKEN_IDENT &&
				value != AZIMUTH_BEHIND) {
			parserutils_vector_iterate(vector, ctx);

			if (token->ilower == c->strings[BEHIND]) {
				value |= AZIMUTH_BEHIND;
			} else {
				return CSS_INVALID;
			}
		} else if ((token == NULL || token->type != CSS_TOKEN_IDENT) &&
				value == AZIMUTH_BEHIND) {
			value |= AZIMUTH_CENTER;
		}
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_DEG, 
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if ((unit & UNIT_ANGLE) == false)
			return CSS_INVALID;

		/* Valid angles lie between -360 and 360 degrees */
		if (unit == UNIT_DEG) {
			if (length < FMULI(F_360, -1) || length > F_360)
				return CSS_INVALID;
		} else if (unit == UNIT_GRAD) {
			if (length < FMULI(F_400, -1) || length > F_400)
				return CSS_INVALID;
		} else if (unit == UNIT_RAD) {
			if (length < FMULI(F_2PI, -1) || length > F_2PI)
				return CSS_INVALID;
		}

		value = AZIMUTH_ANGLE;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_AZIMUTH, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == AZIMUTH_ANGLE)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == AZIMUTH_ANGLE) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_background_attachment(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (fixed, scroll, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[FIXED]) {
		value = BACKGROUND_ATTACHMENT_FIXED;
	} else if (ident->ilower == c->strings[SCROLL]) {
		value = BACKGROUND_ATTACHMENT_SCROLL;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_BACKGROUND_ATTACHMENT, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_background_color(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t colour = 0;
	uint32_t required_size;

	/* colour | IDENT (transparent, inherit) */
	token= parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[TRANSPARENT]) {
		parserutils_vector_iterate(vector, ctx);
		value = BACKGROUND_COLOR_TRANSPARENT;
	} else {
		error = parse_colour_specifier(c, vector, ctx, &colour);
		if (error != CSS_OK)
			return error;

		value = BACKGROUND_COLOR_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_BACKGROUND_COLOR, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == BACKGROUND_COLOR_SET)
		required_size += sizeof(colour);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == BACKGROUND_COLOR_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&colour, sizeof(colour));
	}

	return CSS_OK;
}

css_error parse_background_image(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size;

	/* URI | IDENT (none, inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_URI))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[NONE]) {
		value = BACKGROUND_IMAGE_NONE;
	} else if (token->type == CSS_TOKEN_URI) {
		value = BACKGROUND_IMAGE_URI;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_BACKGROUND_IMAGE, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == BACKGROUND_IMAGE_URI)
		required_size += sizeof(lwc_string *);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == BACKGROUND_IMAGE_URI) {
                lwc_context_string_ref(c->sheet->dictionary, token->idata);
		memcpy((uint8_t *) (*result)->bytecode + sizeof(opv),
				&token->idata, 
				sizeof(lwc_string *));
	}

	return CSS_OK;
}

css_error parse_background_position(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint32_t opv;
	uint16_t value[2] = { 0 };
	css_fixed length[2] = { 0 };
	uint32_t unit[2] = { 0 };
	uint32_t required_size;

	/* [length | percentage | IDENT(left, right, top, bottom, center)]{1,2}
	 * | IDENT(inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else {
		int i;

		for (i = 0; i < 2; i++) {
			token = parserutils_vector_peek(vector, *ctx);
			/* This can only occur on the second attempt */
			/* Also detect start of !important on second attempt */
			if (token == NULL || 
					(i == 1 && tokenIsChar(token, '!')))
				break;

			if (token->type == CSS_TOKEN_IDENT) {
				parserutils_vector_iterate(vector, ctx);

				if (token->ilower == c->strings[LEFT]) {
					value[i] = 
						BACKGROUND_POSITION_HORZ_LEFT;
				} else if (token->ilower == c->strings[RIGHT]) {
					value[i] = 
						BACKGROUND_POSITION_HORZ_RIGHT;
				} else if (token->ilower == c->strings[TOP]) {
					value[i] = BACKGROUND_POSITION_VERT_TOP;
				} else if (token->ilower == 
						c->strings[BOTTOM]) {
					value[i] = 
						BACKGROUND_POSITION_VERT_BOTTOM;
				} else if (token->ilower == 
						c->strings[CENTER]) {
					/* We'll fix this up later */
					value[i] = 
						BACKGROUND_POSITION_VERT_CENTER;
				} else {
					return CSS_INVALID;
				}
			} else {
				error = parse_unit_specifier(c, vector, ctx, 
						UNIT_PX, &length[i], &unit[i]);
				if (error != CSS_OK)
					return error;

				if (unit[i] & UNIT_ANGLE || 
						unit[i] & UNIT_TIME || 
						unit[i] & UNIT_FREQ)
					return CSS_INVALID;

				/* We'll fix this up later, too */
				value[i] = BACKGROUND_POSITION_VERT_SET;
			}

			consumeWhitespace(vector, ctx);
		}

		/* Now, sort out the mess we've got */
		if (i == 1) {
			assert(BACKGROUND_POSITION_VERT_CENTER ==
					BACKGROUND_POSITION_HORZ_CENTER);

			/* Only one value, so the other is center */
			switch (value[0]) {
			case BACKGROUND_POSITION_HORZ_LEFT:
			case BACKGROUND_POSITION_HORZ_RIGHT:
			case BACKGROUND_POSITION_VERT_CENTER:
			case BACKGROUND_POSITION_VERT_TOP:
			case BACKGROUND_POSITION_VERT_BOTTOM:
				break;
			case BACKGROUND_POSITION_VERT_SET:
				value[0] = BACKGROUND_POSITION_HORZ_SET;
				break;
			default:
				return CSS_INVALID;
			}

			value[1] = BACKGROUND_POSITION_VERT_CENTER;
		} else if (value[0] != BACKGROUND_POSITION_VERT_SET &&
				value[1] != BACKGROUND_POSITION_VERT_SET) {
			/* Two keywords. Verify the axes differ */
			if (((value[0] & 0xf) != 0 && (value[1] & 0xf) != 0) ||
					((value[0] & 0xf0) != 0 && 
						(value[1] & 0xf0) != 0))
				return CSS_INVALID;
		} else {
			/* One or two non-keywords. First is horizontal */
			if (value[0] == BACKGROUND_POSITION_VERT_SET)
				value[0] = BACKGROUND_POSITION_HORZ_SET;

			/* Verify the axes differ */
			if (((value[0] & 0xf) != 0 && (value[1] & 0xf) != 0) ||
					((value[0] & 0xf0) != 0 && 
						(value[1] & 0xf0) != 0))
				return CSS_INVALID;
		}
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_BACKGROUND_POSITION, flags, value[0] | value[1]);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false) { 
		if (value[0] == BACKGROUND_POSITION_HORZ_SET)
			required_size += sizeof(length[0]) + sizeof(unit[0]);
		if (value[1] == BACKGROUND_POSITION_VERT_SET)
			required_size += sizeof(length[1]) + sizeof(unit[1]);
	}

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false) {
		uint8_t *ptr = ((uint8_t *) (*result)->bytecode) + sizeof(opv);
		if (value[0] == BACKGROUND_POSITION_HORZ_SET) {
			memcpy(ptr, &length[0], sizeof(length[0]));
			ptr += sizeof(length[0]);
			memcpy(ptr, &unit[0], sizeof(unit[0]));
			ptr += sizeof(unit[0]);
		}
		if (value[1] == BACKGROUND_POSITION_VERT_SET) {
			memcpy(ptr, &length[1], sizeof(length[1]));
			ptr += sizeof(length[1]);
			memcpy(ptr, &unit[1], sizeof(unit[1]));
		}
	}

	return CSS_OK;
}

css_error parse_background_repeat(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (no-repeat, repeat-x, repeat-y, repeat, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[NO_REPEAT]) {
		value = BACKGROUND_REPEAT_NO_REPEAT;
	} else if (ident->ilower == c->strings[REPEAT_X]) {
		value = BACKGROUND_REPEAT_REPEAT_X;
	} else if (ident->ilower == c->strings[REPEAT_Y]) {
		value = BACKGROUND_REPEAT_REPEAT_Y;
	} else if (ident->ilower == c->strings[REPEAT]) {
		value = BACKGROUND_REPEAT_REPEAT;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_BACKGROUND_REPEAT, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_border_bottom_color(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_color(c, vector, ctx, 
			CSS_PROP_BORDER_BOTTOM_COLOR, result);
}

css_error parse_border_bottom_style(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_style(c, vector, ctx, 
			CSS_PROP_BORDER_BOTTOM_STYLE, result);
}

css_error parse_border_bottom_width(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_width(c, vector, ctx, 
			CSS_PROP_BORDER_BOTTOM_WIDTH, result);
}

css_error parse_border_collapse(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (collapse, separate, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[COLLAPSE]) {
		value = BORDER_COLLAPSE_COLLAPSE;
	} else if (ident->ilower == c->strings[SEPARATE]) {
		value = BORDER_COLLAPSE_SEPARATE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_BORDER_COLLAPSE, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_border_left_color(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_color(c, vector, ctx, 
			CSS_PROP_BORDER_LEFT_COLOR, result);
}

css_error parse_border_left_style(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_style(c, vector, ctx, 
			CSS_PROP_BORDER_LEFT_STYLE, result);
}

css_error parse_border_left_width(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_width(c, vector, ctx, 
			CSS_PROP_BORDER_LEFT_WIDTH, result);
}

css_error parse_border_right_color(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_color(c, vector, ctx, 
			CSS_PROP_BORDER_RIGHT_COLOR, result);
}

css_error parse_border_right_style(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_style(c, vector, ctx, 
			CSS_PROP_BORDER_RIGHT_STYLE, result);
}

css_error parse_border_right_width(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_width(c, vector, ctx, 
			CSS_PROP_BORDER_RIGHT_WIDTH, result);
}

css_error parse_border_spacing(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length[2] = { 0 };
	uint32_t unit[2] = { 0 };
	uint32_t required_size;

	/* length length? | IDENT(inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else {
		int num_lengths = 0;

		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length[0], &unit[0]);
		if (error != CSS_OK)
			return error;

		if (unit[0] & UNIT_ANGLE || unit[0] & UNIT_TIME || 
				unit[0] & UNIT_FREQ || unit[0] & UNIT_PCT)
			return CSS_INVALID;

		num_lengths = 1;

		consumeWhitespace(vector, ctx);

		token = parserutils_vector_peek(vector, *ctx);
		if (token != NULL && tokenIsChar(token, '!') == false) {
			error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
					&length[1], &unit[1]);
			if (error != CSS_OK)
				return error;

			if (unit[1] & UNIT_ANGLE || unit[1] & UNIT_TIME ||
					unit[1] & UNIT_FREQ || 
					unit[1] & UNIT_PCT)
				return CSS_INVALID;

			num_lengths = 2;
		}

		if (num_lengths == 1) {
			/* Only one length specified. Use for both axes. */
			length[1] = length[0];
			unit[1] = unit[0];
		}

		/* Lengths must not be negative */
		if (length[0] < 0 || length[1] < 0)
			return CSS_INVALID;

		value = BORDER_SPACING_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_BORDER_SPACING, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == BORDER_SPACING_SET)
		required_size += 2 * (sizeof(length[0]) + sizeof(unit[0]));

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == BORDER_SPACING_SET) {
		uint8_t *ptr = ((uint8_t *) (*result)->bytecode) + sizeof(opv);

		memcpy(ptr, &length[0], sizeof(length[0]));
		ptr += sizeof(length[0]);
		memcpy(ptr, &unit[0], sizeof(unit[0]));
		ptr += sizeof(unit[0]);
		memcpy(ptr, &length[1], sizeof(length[1]));
		ptr += sizeof(length[1]);
		memcpy(ptr, &unit[1], sizeof(unit[1]));
	}

	return CSS_OK;
}

css_error parse_border_top_color(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_color(c, vector, ctx, 
			CSS_PROP_BORDER_TOP_COLOR, result);
}

css_error parse_border_top_style(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_style(c, vector, ctx, 
			CSS_PROP_BORDER_TOP_STYLE, result);
}

css_error parse_border_top_width(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_border_side_width(c, vector, ctx, 
			CSS_PROP_BORDER_TOP_WIDTH, result);
}

css_error parse_bottom(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(auto, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[AUTO]) {
		parserutils_vector_iterate(vector, ctx);
		value = BOTTOM_AUTO;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		value = BOTTOM_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_BOTTOM, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == BOTTOM_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == BOTTOM_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_caption_side(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (top, bottom, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[TOP]) {
		value = CAPTION_SIDE_TOP;
	} else if (ident->ilower == c->strings[BOTTOM]) {
		value = CAPTION_SIDE_BOTTOM;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_CAPTION_SIDE, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_clear(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (left, right, both, none, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[RIGHT]) {
		value = CLEAR_RIGHT;
	} else if (ident->ilower == c->strings[LEFT]) {
		value = CLEAR_LEFT;
	} else if (ident->ilower == c->strings[BOTH]) {
		value = CLEAR_BOTH;
	} else if (ident->ilower == c->strings[NONE]) {
		value = CLEAR_NONE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_CLEAR, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_clip(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	int num_lengths = 0;
	css_fixed length[4] = { 0 };
	uint32_t unit[4] = { 0 };
	uint32_t required_size;

	/* FUNCTION(rect) [ [ IDENT(auto) | length ] CHAR(,)? ]{3} 
	 *                [ IDENT(auto) | length ] CHAR{)} |
	 * IDENT(auto, inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[AUTO]) {
		value = CLIP_AUTO;
	} else if (token->type == CSS_TOKEN_FUNCTION &&
			token->ilower == c->strings[RECT]) {
		int i;
		value = CLIP_SHAPE_RECT;

		for (i = 0; i < 4; i++) {
			consumeWhitespace(vector, ctx);

			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL)
				return CSS_INVALID;

			if (token->type == CSS_TOKEN_IDENT) {
				/* Slightly magical way of generating the auto 
				 * values. These are bits 3-6 of the value. */
				if (token->ilower == c->strings[AUTO])
					value |= 1 << (i + 3);
				else
					return CSS_INVALID;

				parserutils_vector_iterate(vector, ctx);
			} else {
				error = parse_unit_specifier(c, vector, ctx, 
						UNIT_PX, 
						&length[num_lengths], 
						&unit[num_lengths]);
				if (error != CSS_OK)
					return error;

				if (unit[num_lengths] & UNIT_ANGLE || 
						unit[num_lengths] & UNIT_TIME ||
						unit[num_lengths] & UNIT_FREQ ||
						unit[num_lengths] & UNIT_PCT)
					return CSS_INVALID;

				num_lengths++;
			}

			consumeWhitespace(vector, ctx);

			/* Consume optional comma after first 3 parameters */
			if (i < 3) {
				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL)
					return CSS_INVALID;

				if (tokenIsChar(token, ','))
					parserutils_vector_iterate(vector, ctx);
			}
		}

		consumeWhitespace(vector, ctx);

		/* Finally, consume closing parenthesis */
		token = parserutils_vector_iterate(vector, ctx);
		if (token == NULL || tokenIsChar(token, ')') == false)
			return CSS_INVALID;
	} else {
		return CSS_INVALID;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_CLIP, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && 
			(value & CLIP_SHAPE_MASK) == CLIP_SHAPE_RECT) {
		required_size += 
			num_lengths * (sizeof(length[0]) + sizeof(unit[0]));
	}

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && 
			(value & CLIP_SHAPE_MASK) == CLIP_SHAPE_RECT) {
		int i;
		uint8_t *ptr = ((uint8_t *) (*result)->bytecode) + sizeof(opv);

		for (i = 0; i < num_lengths; i++) {
			memcpy(ptr, &length[i], sizeof(length[i]));
			ptr += sizeof(length[i]);
			memcpy(ptr, &unit[i], sizeof(unit[i]));
			ptr += sizeof(unit[i]);
		}
	}

	return CSS_OK;
}

css_error parse_color(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t colour = 0;
	uint32_t required_size;

	/* colour | IDENT (inherit) */
	token= parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags |= FLAG_INHERIT;
	} else {
		error = parse_colour_specifier(c, vector, ctx, &colour);
		if (error != CSS_OK)
			return error;

		value = COLOR_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_COLOR, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == COLOR_SET)
		required_size += sizeof(colour);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == COLOR_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&colour, sizeof(colour));
	}

	return CSS_OK;
}

css_error parse_content(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size = sizeof(opv);
	int temp_ctx = *ctx;
	uint8_t *ptr;

	/* IDENT(normal, none, inherit) |
	 * [
	 *	IDENT(open-quote, close-quote, no-open-quote, no-close-quote) |
	 *	STRING | URI |
	 *	FUNCTION(attr) IDENT ')' |
	 *	FUNCTION(counter) IDENT IDENT? ')' |
	 *	FUNCTION(counters) IDENT STRING IDENT? ')'
	 * ]+
	 */

	/* Pass 1: Calculate required size & validate input */
	token = parserutils_vector_peek(vector, temp_ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		flags = FLAG_INHERIT;
		parserutils_vector_iterate(vector, &temp_ctx);
	} else if (token->type == CSS_TOKEN_IDENT &&
			 token->ilower == c->strings[NORMAL]) {
		value = CONTENT_NORMAL;
		parserutils_vector_iterate(vector, &temp_ctx);
	} else if (token->type == CSS_TOKEN_IDENT &&
			 token->ilower == c->strings[NONE]) {
		value = CONTENT_NONE;
		parserutils_vector_iterate(vector, &temp_ctx);
	} else {
		uint32_t len;

		error = parse_content_list(c, vector, &temp_ctx, &value,
				NULL, &len);
		if (error != CSS_OK)
			return error;

		required_size += len;
	}

	error = parse_important(c, vector, &temp_ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_CONTENT, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy OPV to bytecode */
	ptr = (*result)->bytecode;
	memcpy(ptr, &opv, sizeof(opv));
	ptr += sizeof(opv);

	/* Pass 2: construct bytecode */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			(token->ilower == c->strings[INHERIT] ||
			 token->ilower == c->strings[NORMAL] ||
			 token->ilower == c->strings[NONE])) {
			parserutils_vector_iterate(vector, ctx);
	} else {
		error = parse_content_list(c, vector, ctx, NULL, ptr, NULL);
		if (error != CSS_OK)
			return error;
	}

	/* Ensure we skip past !important */
	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	return CSS_OK;
}

css_error parse_counter_increment(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size = sizeof(opv);
	int temp_ctx = *ctx;
	uint8_t *ptr;

	/* [IDENT <integer>? ]+ | IDENT(none, inherit) */

	/* Pass 1: validate input and calculate bytecode size */
	token = parserutils_vector_iterate(vector, &temp_ctx);
	if (token == NULL || token->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	if (token->ilower == c->strings[INHERIT]) {
		flags = FLAG_INHERIT;
	} else if (token->ilower == c->strings[NONE]) {
		value = COUNTER_INCREMENT_NONE;
	} else {
		bool first = true;

		value = COUNTER_INCREMENT_NAMED;

		while (token != NULL) {
			lwc_string *name = token->idata;
			css_fixed increment = INTTOFIX(1);

			consumeWhitespace(vector, &temp_ctx);

			/* Optional integer */
			token = parserutils_vector_peek(vector, temp_ctx);
			if (token != NULL && token->type != CSS_TOKEN_IDENT &&
					token->type != CSS_TOKEN_NUMBER)
				return CSS_INVALID;

			if (token != NULL && token->type == CSS_TOKEN_NUMBER) {
				size_t consumed = 0;

				increment = number_from_lwc_string(token->ilower,
						true, &consumed);

				if (consumed != lwc_string_length(token->ilower))
					return CSS_INVALID;

				parserutils_vector_iterate(vector, &temp_ctx);

				consumeWhitespace(vector, &temp_ctx);
			}

			if (first == false) {
				required_size += sizeof(opv);
			}
			required_size += sizeof(name) + sizeof(increment);

			token = parserutils_vector_peek(vector, temp_ctx);
			if (token == NULL || tokenIsChar(token, '!')) {
				break;
			}

			first = false;

			token = parserutils_vector_iterate(vector, &temp_ctx);
			if (token != NULL && token->type != CSS_TOKEN_IDENT)
				return CSS_INVALID;
		}

		/* And for the terminator */
		required_size += sizeof(opv);
	}

	error = parse_important(c, vector, &temp_ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_COUNTER_INCREMENT, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy OPV to bytecode */
	ptr = (*result)->bytecode;
	memcpy(ptr, &opv, sizeof(opv));
	ptr += sizeof(opv);

	/* Pass 2: construct bytecode */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || token->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	if (token->ilower == c->strings[INHERIT] ||
			token->ilower == c->strings[NONE]) {
		/* Nothing to do */
	} else {
		bool first = true;

		opv = COUNTER_INCREMENT_NAMED;

		while (token != NULL) {
			lwc_string *name = token->idata;
			css_fixed increment = INTTOFIX(1);

			consumeWhitespace(vector, ctx);

			/* Optional integer */
			token = parserutils_vector_peek(vector, *ctx);
			if (token != NULL && token->type != CSS_TOKEN_IDENT &&
					token->type != CSS_TOKEN_NUMBER)
				return CSS_INVALID;

			if (token != NULL && token->type == CSS_TOKEN_NUMBER) {
				size_t consumed = 0;

				increment = number_from_lwc_string(token->ilower,
						true, &consumed);

				if (consumed != lwc_string_length(token->ilower))
					return CSS_INVALID;

				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);
			}

			if (first == false) {
				memcpy(ptr, &opv, sizeof(opv));
				ptr += sizeof(opv);
			}
                        
                        lwc_context_string_ref(c->sheet->dictionary, name);
			memcpy(ptr, &name, sizeof(name));
			ptr += sizeof(name);
                        
			memcpy(ptr, &increment, sizeof(increment));
			ptr += sizeof(increment);

			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL || tokenIsChar(token, '!')) {
				break;
			}

			first = false;

			token = parserutils_vector_iterate(vector, ctx);
			if (token != NULL && token->type != CSS_TOKEN_IDENT)
				return CSS_INVALID;
		}

		/* And for the terminator */
		opv = COUNTER_INCREMENT_NONE;
		memcpy(ptr, &opv, sizeof(opv));
		ptr += sizeof(opv);
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	return CSS_OK;
}

css_error parse_counter_reset(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size = sizeof(opv);
	int temp_ctx = *ctx;
	uint8_t *ptr;

	/* [IDENT <integer>? ]+ | IDENT(none, inherit) */

	/* Pass 1: validate input and calculate bytecode size */
	token = parserutils_vector_iterate(vector, &temp_ctx);
	if (token == NULL || token->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	if (token->ilower == c->strings[INHERIT]) {
		flags = FLAG_INHERIT;
	} else if (token->ilower == c->strings[NONE]) {
		value = COUNTER_RESET_NONE;
	} else {
		bool first = true;

		value = COUNTER_RESET_NAMED;

		while (token != NULL) {
			lwc_string *name = token->idata;
			css_fixed increment = INTTOFIX(0);

			consumeWhitespace(vector, &temp_ctx);

			/* Optional integer */
			token = parserutils_vector_peek(vector, temp_ctx);
			if (token != NULL && token->type != CSS_TOKEN_IDENT &&
					token->type != CSS_TOKEN_NUMBER)
				return CSS_INVALID;

			if (token != NULL && token->type == CSS_TOKEN_NUMBER) {
				size_t consumed = 0;

				increment = number_from_lwc_string(token->ilower,
						true, &consumed);

				if (consumed != lwc_string_length(token->ilower))
					return CSS_INVALID;

				parserutils_vector_iterate(vector, &temp_ctx);

				consumeWhitespace(vector, &temp_ctx);
			}

			if (first == false) {
				required_size += sizeof(opv);
			}
			required_size += sizeof(name) + sizeof(increment);

			token = parserutils_vector_peek(vector, temp_ctx);
			if (token == NULL || tokenIsChar(token, '!')) {
				break;
			}

			first = false;

			token = parserutils_vector_iterate(vector, &temp_ctx);
			if (token != NULL && token->type != CSS_TOKEN_IDENT)
				return CSS_INVALID;
		}

		/* And for the terminator */
		required_size += sizeof(opv);
	}

	error = parse_important(c, vector, &temp_ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_COUNTER_RESET, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy OPV to bytecode */
	ptr = (*result)->bytecode;
	memcpy(ptr, &opv, sizeof(opv));
	ptr += sizeof(opv);

	/* Pass 2: construct bytecode */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || token->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	if (token->ilower == c->strings[INHERIT] ||
			token->ilower == c->strings[NONE]) {
		/* Nothing to do */
	} else {
		bool first = true;

		opv = COUNTER_RESET_NAMED;

		while (token != NULL) {
			lwc_string *name = token->idata;
			css_fixed increment = INTTOFIX(0);

			consumeWhitespace(vector, ctx);

			/* Optional integer */
			token = parserutils_vector_peek(vector, *ctx);
			if (token != NULL && token->type != CSS_TOKEN_IDENT &&
					token->type != CSS_TOKEN_NUMBER)
				return CSS_INVALID;

			if (token != NULL && token->type == CSS_TOKEN_NUMBER) {
				size_t consumed = 0;

				increment = number_from_lwc_string(token->ilower,
						true, &consumed);

				if (consumed != lwc_string_length(token->ilower))
					return CSS_INVALID;

				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);
			}

			if (first == false) {
				memcpy(ptr, &opv, sizeof(opv));
				ptr += sizeof(opv);
			}
                        
                        lwc_context_string_ref(c->sheet->dictionary, name);
			memcpy(ptr, &name, sizeof(name));
			ptr += sizeof(name);
                        
			memcpy(ptr, &increment, sizeof(increment));
			ptr += sizeof(increment);

			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL || tokenIsChar(token, '!')) {
				break;
			}

			first = false;

			token = parserutils_vector_iterate(vector, ctx);
			if (token != NULL && token->type != CSS_TOKEN_IDENT)
				return CSS_INVALID;
		}

		/* And for the terminator */
		opv = COUNTER_RESET_NONE;
		memcpy(ptr, &opv, sizeof(opv));
		ptr += sizeof(opv);
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	return CSS_OK;
}

css_error parse_cue_after(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size;

	/* URI | IDENT (none, inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_URI))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[NONE]) {
		value = CUE_AFTER_NONE;
	} else if (token->type == CSS_TOKEN_URI) {
		value = CUE_AFTER_URI;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_CUE_AFTER, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == CUE_AFTER_URI)
		required_size += sizeof(lwc_string *);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == CUE_AFTER_URI) {
                lwc_context_string_ref(c->sheet->dictionary, token->idata);
		memcpy((uint8_t *) (*result)->bytecode + sizeof(opv),
				&token->idata, 
				sizeof(lwc_string *));
	}

	return CSS_OK;
}

css_error parse_cue_before(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size;

	/* URI | IDENT (none, inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_URI))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[NONE]) {
		value = CUE_BEFORE_NONE;
	} else if (token->type == CSS_TOKEN_URI) {
		value = CUE_BEFORE_URI;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_CUE_BEFORE, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == CUE_BEFORE_URI)
		required_size += sizeof(lwc_string *);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == CUE_BEFORE_URI) {
                lwc_context_string_ref(c->sheet->dictionary, token->idata);
		memcpy((uint8_t *) (*result)->bytecode + sizeof(opv),
				&token->idata, 
				sizeof(lwc_string *));
	}

	return CSS_OK;
}

css_error parse_cursor(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size = sizeof(opv);
	int temp_ctx = *ctx;
	uint8_t *ptr;

	/* [ (URI ',')* IDENT(auto, crosshair, default, pointer, move, e-resize,
	 *              ne-resize, nw-resize, n-resize, se-resize, sw-resize,
	 *              s-resize, w-resize, text, wait, help, progress) ] 
	 * | IDENT(inherit) 
	 */

	/* Pass 1: validate input and calculate bytecode size */
	token = parserutils_vector_iterate(vector, &temp_ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_URI))
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		flags = FLAG_INHERIT;
	} else {
		bool first = true;

		/* URI* */
		while (token != NULL && token->type == CSS_TOKEN_URI) {
			lwc_string *uri = token->idata;

			if (first == false) {
				required_size += sizeof(opv);
			} else {
				value = CURSOR_URI;
			}
			required_size += sizeof(uri);

			consumeWhitespace(vector, &temp_ctx);

			/* Expect ',' */
			token = parserutils_vector_iterate(vector, &temp_ctx);
			if (token == NULL || tokenIsChar(token, ',') == false)
				return CSS_INVALID;

			consumeWhitespace(vector, &temp_ctx);

			/* Expect either URI or IDENT */
			token = parserutils_vector_iterate(vector, &temp_ctx);
			if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
					token->type != CSS_TOKEN_URI))
				return CSS_INVALID;

			first = false;
		}

		/* IDENT */
		if (token != NULL && token->type == CSS_TOKEN_IDENT) {
			if (token->ilower == c->strings[AUTO]) {
				if (first) {
					value = CURSOR_AUTO;
				}
			} else if (token->ilower == c->strings[CROSSHAIR]) {
				if (first) {
					value = CURSOR_CROSSHAIR;
				}
			} else if (token->ilower == c->strings[DEFAULT]) {
				if (first) {
					value = CURSOR_DEFAULT;
				}
			} else if (token->ilower == c->strings[POINTER]) {
				if (first) {
					value = CURSOR_POINTER;
				}
			} else if (token->ilower == c->strings[MOVE]) {
				if (first) {
					value = CURSOR_MOVE;
				}
			} else if (token->ilower == c->strings[E_RESIZE]) {
				if (first) {
					value = CURSOR_E_RESIZE;
				}
			} else if (token->ilower == c->strings[NE_RESIZE]) {
				if (first) {
					value = CURSOR_NE_RESIZE;
				}
			} else if (token->ilower == c->strings[NW_RESIZE]) {
				if (first) {
					value = CURSOR_NW_RESIZE;
				}
			} else if (token->ilower == c->strings[N_RESIZE]) {
				if (first) {
					value = CURSOR_N_RESIZE;
				}
			} else if (token->ilower == c->strings[SE_RESIZE]) {
				if (first) {
					value = CURSOR_SE_RESIZE;
				}
			} else if (token->ilower == c->strings[SW_RESIZE]) {
				if (first) {
					value = CURSOR_SW_RESIZE;
				}
			} else if (token->ilower == c->strings[S_RESIZE]) {
				if (first) {
					value = CURSOR_S_RESIZE;
				}
			} else if (token->ilower == c->strings[W_RESIZE]) {
				if (first) {
					value = CURSOR_W_RESIZE;
				}
			} else if (token->ilower == c->strings[TEXT]) {
				if (first) {
					value = CURSOR_TEXT;
				}
			} else if (token->ilower == c->strings[WAIT]) {
				if (first) {
					value = CURSOR_WAIT;
				}
			} else if (token->ilower == c->strings[HELP]) {
				if (first) {
					value = CURSOR_HELP;
				}
			} else if (token->ilower == c->strings[PROGRESS]) {
				if (first) {
					value = CURSOR_PROGRESS;
				}
			} else {
				return CSS_INVALID;
			}

			if (first == false) {
				required_size += sizeof(opv);
			}
		}

		consumeWhitespace(vector, &temp_ctx);

		token = parserutils_vector_peek(vector, temp_ctx);
		if (token != NULL && tokenIsChar(token, '!') == false)
			return CSS_INVALID;
	}

	error = parse_important(c, vector, &temp_ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_CURSOR, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy OPV to bytecode */
	ptr = (*result)->bytecode;
	memcpy(ptr, &opv, sizeof(opv));
	ptr += sizeof(opv);

	/* Pass 2: construct bytecode */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_URI))
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		/* Nothing to do */
	} else {
		bool first = true;

		/* URI* */
		while (token != NULL && token->type == CSS_TOKEN_URI) {
			lwc_string *uri = token->idata;

			if (first == false) {
				opv = CURSOR_URI;
				memcpy(ptr, &opv, sizeof(opv));
				ptr += sizeof(opv);
			}
                        
                        lwc_context_string_ref(c->sheet->dictionary, uri);
			memcpy(ptr, &uri, sizeof(uri));
			ptr += sizeof(uri);

			consumeWhitespace(vector, ctx);

			/* Expect ',' */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || tokenIsChar(token, ',') == false)
				return CSS_INVALID;

			consumeWhitespace(vector, ctx);

			/* Expect either URI or IDENT */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
					token->type != CSS_TOKEN_URI))
				return CSS_INVALID;

			first = false;
		}

		/* IDENT */
		if (token != NULL && token->type == CSS_TOKEN_IDENT) {
			if (token->ilower == c->strings[AUTO]) {
				opv = CURSOR_AUTO;
			} else if (token->ilower == c->strings[CROSSHAIR]) {
				opv = CURSOR_CROSSHAIR;
			} else if (token->ilower == c->strings[DEFAULT]) {
				opv = CURSOR_DEFAULT;
			} else if (token->ilower == c->strings[POINTER]) {
				opv = CURSOR_POINTER;
			} else if (token->ilower == c->strings[MOVE]) {
				opv = CURSOR_MOVE;
			} else if (token->ilower == c->strings[E_RESIZE]) {
				opv = CURSOR_E_RESIZE;
			} else if (token->ilower == c->strings[NE_RESIZE]) {
				opv = CURSOR_NE_RESIZE;
			} else if (token->ilower == c->strings[NW_RESIZE]) {
				opv = CURSOR_NW_RESIZE;
			} else if (token->ilower == c->strings[N_RESIZE]) {
				opv = CURSOR_N_RESIZE;
			} else if (token->ilower == c->strings[SE_RESIZE]) {
				opv = CURSOR_SE_RESIZE;
			} else if (token->ilower == c->strings[SW_RESIZE]) {
				opv = CURSOR_SW_RESIZE;
			} else if (token->ilower == c->strings[S_RESIZE]) {
				opv = CURSOR_S_RESIZE;
			} else if (token->ilower == c->strings[W_RESIZE]) {
				opv = CURSOR_W_RESIZE;
			} else if (token->ilower == c->strings[TEXT]) {
				opv = CURSOR_TEXT;
			} else if (token->ilower == c->strings[WAIT]) {
				opv = CURSOR_WAIT;
			} else if (token->ilower == c->strings[HELP]) {
				opv = CURSOR_HELP;
			} else if (token->ilower == c->strings[PROGRESS]) {
				opv = CURSOR_PROGRESS;
			} else {
				return CSS_INVALID;
			}

			if (first == false) {
				memcpy(ptr, &opv, sizeof(opv));
				ptr += sizeof(opv);
			}
		}

		consumeWhitespace(vector, ctx);

		token = parserutils_vector_peek(vector, *ctx);
		if (token != NULL && tokenIsChar(token, '!') == false)
			return CSS_INVALID;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	return CSS_OK;
}

css_error parse_direction(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (ltr, rtl, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[LTR]) {
		value = DIRECTION_LTR;
	} else if (ident->ilower == c->strings[RTL]) {
		value = DIRECTION_RTL;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_DIRECTION, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_display(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (inline, block, list-item, run-in, inline-block, table,
	 * inline-table, table-row-group, table-header-group, 
	 * table-footer-group, table-row, table-column-group, table-column,
	 * table-cell, table-caption, none, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[INLINE]) {
		value = DISPLAY_INLINE;
	} else if (ident->ilower == c->strings[BLOCK]) {
		value = DISPLAY_BLOCK;
	} else if (ident->ilower == c->strings[LIST_ITEM]) {
		value = DISPLAY_LIST_ITEM;
	} else if (ident->ilower == c->strings[RUN_IN]) {
		value = DISPLAY_RUN_IN;
	} else if (ident->ilower == c->strings[INLINE_BLOCK]) {
		value = DISPLAY_INLINE_BLOCK;
	} else if (ident->ilower == c->strings[TABLE]) {
		value = DISPLAY_TABLE;
	} else if (ident->ilower == c->strings[INLINE_TABLE]) {
		value = DISPLAY_INLINE_TABLE;
	} else if (ident->ilower == c->strings[TABLE_ROW_GROUP]) {
		value = DISPLAY_TABLE_ROW_GROUP;
	} else if (ident->ilower == c->strings[TABLE_HEADER_GROUP]) {
		value = DISPLAY_TABLE_HEADER_GROUP;
	} else if (ident->ilower == c->strings[TABLE_FOOTER_GROUP]) {
		value = DISPLAY_TABLE_FOOTER_GROUP;
	} else if (ident->ilower == c->strings[TABLE_ROW]) {
		value = DISPLAY_TABLE_ROW;
	} else if (ident->ilower == c->strings[TABLE_COLUMN_GROUP]) {
		value = DISPLAY_TABLE_COLUMN_GROUP;
	} else if (ident->ilower == c->strings[TABLE_COLUMN]) {
		value = DISPLAY_TABLE_COLUMN;
	} else if (ident->ilower == c->strings[TABLE_CELL]) {
		value = DISPLAY_TABLE_CELL;
	} else if (ident->ilower == c->strings[TABLE_CAPTION]) {
		value = DISPLAY_TABLE_CAPTION;
	} else if (ident->ilower == c->strings[NONE]) {
		value = DISPLAY_NONE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_DISPLAY, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_elevation(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* angle | IDENT(below, level, above, higher, lower, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[BELOW]) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_BELOW;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[LEVEL]) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_LEVEL;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[ABOVE]) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_ABOVE;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[HIGHER]) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_HIGHER;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[LOWER]) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_LOWER;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_DEG,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if ((unit & UNIT_ANGLE) == false)
			return CSS_INVALID;

		/* Valid angles lie between -90 and 90 degrees */
		if (unit == UNIT_DEG) {
			if (length < FMULI(F_90, -1) || length > F_90)
				return CSS_INVALID;
		} else if (unit == UNIT_GRAD) {
			if (length < FMULI(F_100, -1) || length > F_100)
				return CSS_INVALID;
		} else if (unit == UNIT_RAD) {
			if (length < FMULI(F_PI_2, -1) || length > F_PI_2)
				return CSS_INVALID;
		}

		value = ELEVATION_ANGLE;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_ELEVATION, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == ELEVATION_ANGLE)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == ELEVATION_ANGLE) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_empty_cells(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (show, hide, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[SHOW]) {
		value = EMPTY_CELLS_SHOW;
	} else if (ident->ilower == c->strings[HIDE]) {
		value = EMPTY_CELLS_HIDE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_EMPTY_CELLS, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_float(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (left, right, none, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[LEFT]) {
		value = FLOAT_LEFT;
	} else if (ident->ilower == c->strings[RIGHT]) {
		value = FLOAT_RIGHT;
	} else if (ident->ilower == c->strings[NONE]) {
		value = FLOAT_NONE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_FLOAT, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_font_family(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size = sizeof(opv);
	int temp_ctx = *ctx;
	uint8_t *ptr;

	/* [ IDENT+ | STRING ] [ ',' [ IDENT+ | STRING ] ]* | IDENT(inherit)
	 * 
	 * In the case of IDENT+, any whitespace between tokens is collapsed to
	 * a single space
	 *
	 * \todo Mozilla makes the comma optional. 
	 * Perhaps this is a quirk we should inherit?
	 */

	/* Pass 1: validate input and calculate space */
	token = parserutils_vector_iterate(vector, &temp_ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_STRING))
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		flags = FLAG_INHERIT;
	} else {
		bool first = true;

		while (token != NULL) {
			if (token->type == CSS_TOKEN_IDENT) {
				if (first == false) {
					required_size += sizeof(opv);
				}

				if (token->ilower == c->strings[SERIF]) {
					if (first) {
						value = FONT_FAMILY_SERIF;
					}
				} else if (token->ilower == 
						c->strings[SANS_SERIF]) {
					if (first) {
						value = FONT_FAMILY_SANS_SERIF;
					}
				} else if (token->ilower == 
						c->strings[CURSIVE]) {
					if (first) {
						value = FONT_FAMILY_CURSIVE;
					}
				} else if (token->ilower == 
						c->strings[FANTASY]) {
					if (first) {
						value = FONT_FAMILY_FANTASY;
					}
				} else if (token->ilower == 
						c->strings[MONOSPACE]) {
					if (first) {
						value = FONT_FAMILY_MONOSPACE;
					}
				} else {
					if (first) {
						value = FONT_FAMILY_IDENT_LIST;
					}

					required_size +=
						sizeof(lwc_string *);

					/* Skip past [ IDENT* S* ]* */
					while (token != NULL) {
						token = parserutils_vector_peek(
								vector, 
								temp_ctx);
						if (token == NULL ||
							(token->type != 
							CSS_TOKEN_IDENT &&
								token->type != 
								CSS_TOKEN_S)) {
							break;
						}

						/* idents must not match 
						 * generic families */
						if (token->type == CSS_TOKEN_IDENT && 
								(token->ilower == c->strings[SERIF] || 
								token->ilower == c->strings[SANS_SERIF] || 
								token->ilower == c->strings[CURSIVE] || 
								token->ilower == c->strings[FANTASY] || 
								token->ilower == c->strings[MONOSPACE]))
							return CSS_INVALID;
						token = parserutils_vector_iterate(
							vector, &temp_ctx);
					}
				}
			} else if (token->type == CSS_TOKEN_STRING) {
				if (first == false) {
					required_size += sizeof(opv);
				} else {
					value = FONT_FAMILY_STRING;
				}

				required_size += 
					sizeof(lwc_string *);
			} else {
				return CSS_INVALID;
			}

			consumeWhitespace(vector, &temp_ctx);

			token = parserutils_vector_peek(vector, temp_ctx);
			if (token != NULL && tokenIsChar(token, ',') == false &&
					tokenIsChar(token, '!') == false) {
				return CSS_INVALID;
			}

			if (token != NULL && tokenIsChar(token, ',')) {
				parserutils_vector_iterate(vector, &temp_ctx);

				consumeWhitespace(vector, &temp_ctx);

				token = parserutils_vector_peek(vector, 
						temp_ctx);
				if (token == NULL || tokenIsChar(token, '!'))
					return CSS_INVALID;
			}

			first = false;

			token = parserutils_vector_peek(vector, temp_ctx);
			if (token != NULL && tokenIsChar(token, '!'))
				break;

			token = parserutils_vector_iterate(vector, &temp_ctx);
		}

		required_size += sizeof(opv);
	}

	error = parse_important(c, vector, &temp_ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_FONT_FAMILY, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy OPV to bytecode */
	ptr = (*result)->bytecode;
	memcpy(ptr, &opv, sizeof(opv));
	ptr += sizeof(opv);

	/* Pass 2: populate bytecode */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_STRING)) {
		css_stylesheet_style_destroy(c->sheet, *result);
		*result = NULL;
		return CSS_INVALID;
	}

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		/* Nothing to do */
	} else {
		bool first = true;

		while (token != NULL) {
			if (token->type == CSS_TOKEN_IDENT) {
				lwc_string *tok_idata = token->idata;
                                lwc_string *name = tok_idata;
                                lwc_string *newname;
			
				if (token->ilower == c->strings[SERIF]) {
					opv = FONT_FAMILY_SERIF;
				} else if (token->ilower == 
						c->strings[SANS_SERIF]) {
					opv = FONT_FAMILY_SANS_SERIF;
				} else if (token->ilower == 
						c->strings[CURSIVE]) {
					opv = FONT_FAMILY_CURSIVE;
				} else if (token->ilower == 
						c->strings[FANTASY]) {
					opv = FONT_FAMILY_FANTASY;
				} else if (token->ilower == 
						c->strings[MONOSPACE]) {
					opv = FONT_FAMILY_MONOSPACE;
				} else {
					uint16_t len = lwc_string_length(token->idata);
					const css_token *temp_token = token;
					lwc_error lerror;
					uint8_t *buf;
					uint8_t *p;

					temp_ctx = *ctx;

					opv = FONT_FAMILY_IDENT_LIST;

					/* Build string from idents */
					while (temp_token != NULL) {
						temp_token = parserutils_vector_peek(
								vector, temp_ctx);
						if (temp_token == NULL || 
							(temp_token->type != 
							CSS_TOKEN_IDENT &&
								temp_token->type != 
								CSS_TOKEN_S)) {
							break;
						}

						if (temp_token->type == 
							CSS_TOKEN_IDENT) {
							len += lwc_string_length(temp_token->idata);
						} else {
							len += 1;
						}

						temp_token = parserutils_vector_iterate(
								vector, &temp_ctx);
					}

					/** \todo Don't use alloca */
					buf = alloca(len);
					p = buf;

					memcpy(p, lwc_string_data(token->idata), lwc_string_length(token->idata));
					p += lwc_string_length(token->idata);

					while (token != NULL) {
						token = parserutils_vector_peek(
								vector, *ctx);
						if (token == NULL ||
							(token->type != 
							CSS_TOKEN_IDENT &&
								token->type !=
								CSS_TOKEN_S)) {
							break;
						}

						if (token->type == 
								CSS_TOKEN_IDENT) {
							memcpy(p,
								lwc_string_data(token->idata),
								lwc_string_length(token->idata));
							p += lwc_string_length(token->idata);
						} else {
							*p++ = ' ';
						}

						token = parserutils_vector_iterate(
							vector, ctx);
					}

					/* Strip trailing whitespace */
					while (p > buf && p[-1] == ' ')
						p--;

					/* Insert into hash, if it's different
					 * from the name we already have */
                                        
                                        lerror = lwc_context_intern(c->sheet->dictionary,
                                                                    (char *)buf, len, &newname);
                                        if (lerror != lwc_error_ok) {
                                                css_stylesheet_style_destroy(c->sheet, *result);
                                                *result = NULL;
                                                return css_error_from_lwc_error(lerror);
                                        }
                                        
                                        if (newname == name)
                                                lwc_context_string_unref(c->sheet->dictionary,
                                                                         newname);
                                        
                                        name = newname;
                                        
				}

				if (first == false) {
					memcpy(ptr, &opv, sizeof(opv));
					ptr += sizeof(opv);
				}

				if (opv == FONT_FAMILY_IDENT_LIST) {
                                        /* Only ref 'name' again if the token owns it,
                                         * otherwise we already own the only ref to the
                                         * new name generated above.
                                         */
                                        if (name == tok_idata)
                                                lwc_context_string_ref(c->sheet->dictionary, name);
					memcpy(ptr, &name, sizeof(name));
					ptr += sizeof(name);
				}
			} else if (token->type == CSS_TOKEN_STRING) {
				opv = FONT_FAMILY_STRING;

				if (first == false) {
					memcpy(ptr, &opv, sizeof(opv));
					ptr += sizeof(opv);
				}
                                
                                lwc_context_string_ref(c->sheet->dictionary, token->idata);
				memcpy(ptr, &token->idata, 
						sizeof(token->idata));
				ptr += sizeof(token->idata);
			} else {
				css_stylesheet_style_destroy(c->sheet, *result);
				*result = NULL;
				return CSS_INVALID;
			}

			consumeWhitespace(vector, ctx);

			token = parserutils_vector_peek(vector, *ctx);
			if (token != NULL && tokenIsChar(token, ',') == false &&
					tokenIsChar(token, '!') == false) {
				css_stylesheet_style_destroy(c->sheet, *result);
				*result = NULL;
				return CSS_INVALID;
			}

			if (token != NULL && tokenIsChar(token, ',')) {
				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);

				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL || tokenIsChar(token, '!')) {
					css_stylesheet_style_destroy(c->sheet,
							*result);
					*result = NULL;
					return CSS_INVALID;
				}
			}

			first = false;

			token = parserutils_vector_peek(vector, *ctx);
			if (token != NULL && tokenIsChar(token, '!'))
				break;

			token = parserutils_vector_iterate(vector, ctx);
		}

		/* Write terminator */
		opv = FONT_FAMILY_END;
		memcpy(ptr, &opv, sizeof(opv));
		ptr += sizeof(opv);
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK) {
		css_stylesheet_style_destroy(c->sheet, *result);
		*result = NULL;
		return error;
	}

	return CSS_OK;
}

css_error parse_font_size(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(xx-small, x-small, small, medium,
	 * large, x-large, xx-large, larger, smaller, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[XX_SMALL]) {
		parserutils_vector_iterate(vector, ctx);
		value = FONT_SIZE_XX_SMALL;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[X_SMALL]) {
		parserutils_vector_iterate(vector, ctx);
		value = FONT_SIZE_X_SMALL;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[SMALL]) {
		parserutils_vector_iterate(vector, ctx);
		value = FONT_SIZE_SMALL;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[MEDIUM]) {
		parserutils_vector_iterate(vector, ctx);
		value = FONT_SIZE_MEDIUM;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[LARGE]) {
		parserutils_vector_iterate(vector, ctx);
		value = FONT_SIZE_LARGE;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[X_LARGE]) {
		parserutils_vector_iterate(vector, ctx);
		value = FONT_SIZE_X_LARGE;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[XX_LARGE]) {
		parserutils_vector_iterate(vector, ctx);
		value = FONT_SIZE_XX_LARGE;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[LARGER]) {
		parserutils_vector_iterate(vector, ctx);
		value = FONT_SIZE_LARGER;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[SMALLER]) {
		parserutils_vector_iterate(vector, ctx);
		value = FONT_SIZE_SMALLER;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Negative values are not permitted */
		if (length < 0)
			return CSS_INVALID;

		value = FONT_SIZE_DIMENSION;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_FONT_SIZE, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == FONT_SIZE_DIMENSION)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == FONT_SIZE_DIMENSION) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_font_style(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (normal, italic, oblique, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[NORMAL]) {
		value = FONT_STYLE_NORMAL;
	} else if (ident->ilower == c->strings[ITALIC]) {
		value = FONT_STYLE_ITALIC;
	} else if (ident->ilower == c->strings[OBLIQUE]) {
		value = FONT_STYLE_OBLIQUE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_FONT_STYLE, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_font_variant(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (normal, small-caps, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[NORMAL]) {
		value = FONT_VARIANT_NORMAL;
	} else if (ident->ilower == c->strings[SMALL_CAPS]) {
		value = FONT_VARIANT_SMALL_CAPS;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_FONT_VARIANT, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_font_weight(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* NUMBER (100, 200, 300, 400, 500, 600, 700, 800, 900) | 
	 * IDENT (normal, bold, bolder, lighter, inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_NUMBER))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		css_fixed num = number_from_lwc_string(token->ilower, true, &consumed);
		/* Invalid if there are trailing characters */
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;
		switch (FIXTOINT(num)) {
		case 100: value = FONT_WEIGHT_100; break;
		case 200: value = FONT_WEIGHT_200; break;
		case 300: value = FONT_WEIGHT_300; break;
		case 400: value = FONT_WEIGHT_400; break;
		case 500: value = FONT_WEIGHT_500; break;
		case 600: value = FONT_WEIGHT_600; break;
		case 700: value = FONT_WEIGHT_700; break;
		case 800: value = FONT_WEIGHT_800; break;
		case 900: value = FONT_WEIGHT_900; break;
		default: return CSS_INVALID;
		}
	} else if (token->ilower == c->strings[NORMAL]) {
		value = FONT_WEIGHT_NORMAL;
	} else if (token->ilower == c->strings[BOLD]) {
		value = FONT_WEIGHT_BOLD;
	} else if (token->ilower == c->strings[BOLDER]) {
		value = FONT_WEIGHT_BOLDER;
	} else if (token->ilower == c->strings[LIGHTER]) {
		value = FONT_WEIGHT_LIGHTER;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_FONT_WEIGHT, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_height(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(auto, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[AUTO]) {
		parserutils_vector_iterate(vector, ctx);
		value = HEIGHT_AUTO;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Negative height is illegal */
		if (length < 0)
			return CSS_INVALID;

		value = HEIGHT_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_HEIGHT, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == HEIGHT_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == HEIGHT_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_left(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(auto, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[AUTO]) {
		parserutils_vector_iterate(vector, ctx);
		value = LEFT_AUTO;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		value = LEFT_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_LEFT, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == LEFT_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == LEFT_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_letter_spacing(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | IDENT(normal, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[NORMAL]) {
		parserutils_vector_iterate(vector, ctx);
		value = LETTER_SPACING_NORMAL;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ ||
				unit & UNIT_PCT)
			return CSS_INVALID;

		value = LETTER_SPACING_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_LETTER_SPACING, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == LETTER_SPACING_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == LETTER_SPACING_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_line_height(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* number | length | percentage | IDENT(normal, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[NORMAL]) {
		parserutils_vector_iterate(vector, ctx);
		value = LINE_HEIGHT_NORMAL;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		length = number_from_lwc_string(token->ilower, false, &consumed);
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;

		/* Negative values are illegal */
		if (length < 0)
			return CSS_INVALID;

		parserutils_vector_iterate(vector, ctx);
		value = LINE_HEIGHT_NUMBER;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Negative values are illegal */
		if (length < 0)
			return CSS_INVALID;

		value = LINE_HEIGHT_DIMENSION;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_LINE_HEIGHT, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == LINE_HEIGHT_NUMBER)
		required_size += sizeof(length);
	else if ((flags & FLAG_INHERIT) == false && 
			value == LINE_HEIGHT_DIMENSION)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && (value == LINE_HEIGHT_NUMBER || 
			value == LINE_HEIGHT_DIMENSION))
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
	if ((flags & FLAG_INHERIT) == false && value == LINE_HEIGHT_DIMENSION)
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));

	return CSS_OK;
}

css_error parse_list_style_image(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size;

	/* URI | IDENT (none, inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_URI))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[NONE]) {
		value = LIST_STYLE_IMAGE_NONE;
	} else if (token->type == CSS_TOKEN_URI) {
		value = LIST_STYLE_IMAGE_URI;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_LIST_STYLE_IMAGE, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == LIST_STYLE_IMAGE_URI)
		required_size += sizeof(lwc_string *);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == LIST_STYLE_IMAGE_URI) {
                lwc_context_string_ref(c->sheet->dictionary, token->idata);
		memcpy((uint8_t *) (*result)->bytecode + sizeof(opv),
				&token->idata, 
				sizeof(lwc_string *));
	}

	return CSS_OK;
}

css_error parse_list_style_position(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (inside, outside, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[INSIDE]) {
		value = LIST_STYLE_POSITION_INSIDE;
	} else if (ident->ilower == c->strings[OUTSIDE]) {
		value = LIST_STYLE_POSITION_OUTSIDE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_LIST_STYLE_POSITION, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_list_style_type(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (disc, circle, square, decimal, decimal-leading-zero,
	 *        lower-roman, upper-roman, lower-greek, lower-latin,
	 *        upper-latin, armenian, georgian, lower-alpha, upper-alpha,
	 *        none, inherit)
	 */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else {
		error = parse_list_style_type_value(c, ident, &value);
		if (error != CSS_OK)
			return error;
	}

	opv = buildOPV(CSS_PROP_LIST_STYLE_TYPE, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_margin_bottom(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_margin_side(c, vector, ctx, 
			CSS_PROP_MARGIN_BOTTOM, result);
}

css_error parse_margin_left(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_margin_side(c, vector, ctx, CSS_PROP_MARGIN_LEFT, result);
}

css_error parse_margin_right(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_margin_side(c, vector, ctx, CSS_PROP_MARGIN_RIGHT, result);
}

css_error parse_margin_top(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_margin_side(c, vector, ctx, CSS_PROP_MARGIN_TOP, result);
}

css_error parse_max_height(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(none, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[NONE]) {
		parserutils_vector_iterate(vector, ctx);
		value = MAX_HEIGHT_NONE;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Negative values are illegal */
		if (length < 0)
			return CSS_INVALID;

		value = MAX_HEIGHT_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_MAX_HEIGHT, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == MAX_HEIGHT_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == MAX_HEIGHT_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_max_width(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(none, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[NONE]) {
		parserutils_vector_iterate(vector, ctx);
		value = MAX_WIDTH_NONE;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Negative values are illegal */
		if (length < 0)
			return CSS_INVALID;

		value = MAX_WIDTH_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_MAX_WIDTH, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == MAX_WIDTH_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags && FLAG_INHERIT) == false && value == MAX_WIDTH_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_min_height(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Negative values are illegal */
		if (length < 0)
			return CSS_INVALID;

		value = MIN_HEIGHT_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_MIN_HEIGHT, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == MIN_HEIGHT_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == MIN_HEIGHT_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_min_width(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Negative values are illegal */
		if (length < 0)
			return CSS_INVALID;

		value = MIN_WIDTH_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_MIN_WIDTH, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == MIN_WIDTH_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == MIN_WIDTH_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_orphans(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed num = 0;
	uint32_t required_size;

	/* <integer> | IDENT (inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_NUMBER))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		num = number_from_lwc_string(token->ilower, true, &consumed);
		/* Invalid if there are trailing characters */
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;

		/* Negative values are nonsensical */
		if (num < 0)
			return CSS_INVALID;

		value = ORPHANS_SET;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_ORPHANS, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == ORPHANS_SET)
		required_size += sizeof(num);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == ORPHANS_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv), 
				&num, sizeof(num));
	}

	return CSS_OK;
}

css_error parse_outline_color(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t colour = 0;
	uint32_t required_size;

	/* colour | IDENT (invert, inherit) */
	token= parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INVERT]) {
		parserutils_vector_iterate(vector, ctx);
		value = OUTLINE_COLOR_INVERT;
	} else {
		error = parse_colour_specifier(c, vector, ctx, &colour);
		if (error != CSS_OK)
			return error;

		value = OUTLINE_COLOR_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_OUTLINE_COLOR, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == OUTLINE_COLOR_SET)
		required_size += sizeof(colour);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == OUTLINE_COLOR_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&colour, sizeof(colour));
	}

	return CSS_OK;
}

css_error parse_outline_style(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	uint32_t opv;
	uint16_t value;

	/* Parse as a border style  */
	error = parse_border_side_style(c, vector, ctx, 
			CSS_PROP_OUTLINE_STYLE, result);
	if (error != CSS_OK)
		return error;

	opv = *((uint32_t *) (*result)->bytecode);

	value = getValue(opv);

	/* Hidden is invalid */
	if (value == BORDER_STYLE_HIDDEN)
		return CSS_INVALID;

	return CSS_OK;
}

css_error parse_outline_width(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	/* Parse as border width */
	return parse_border_side_width(c, vector, ctx, 
			CSS_PROP_OUTLINE_WIDTH, result);
}

css_error parse_overflow(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (visible, hidden, scroll, auto, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[VISIBLE]) {
		value = OVERFLOW_VISIBLE;
	} else if (ident->ilower == c->strings[HIDDEN]) {
		value = OVERFLOW_HIDDEN;
	} else if (ident->ilower == c->strings[SCROLL]) {
		value = OVERFLOW_SCROLL;
	} else if (ident->ilower == c->strings[AUTO]) {
		value = OVERFLOW_AUTO;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_OVERFLOW, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_padding_bottom(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_padding_side(c, vector, ctx, 
			CSS_PROP_PADDING_BOTTOM, result);
}

css_error parse_padding_left(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_padding_side(c, vector, ctx, 
			CSS_PROP_PADDING_LEFT, result);
}

css_error parse_padding_right(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_padding_side(c, vector, ctx, 
			CSS_PROP_PADDING_RIGHT, result);
}

css_error parse_padding_top(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	return parse_padding_side(c, vector, ctx, 
			CSS_PROP_PADDING_TOP, result);
}

css_error parse_page_break_after(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (auto, always, avoid, left, right, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[AUTO]) {
		value = PAGE_BREAK_AFTER_AUTO;
	} else if (ident->ilower == c->strings[ALWAYS]) {
		value = PAGE_BREAK_AFTER_ALWAYS;
	} else if (ident->ilower == c->strings[AVOID]) {
		value = PAGE_BREAK_AFTER_AVOID;
	} else if (ident->ilower == c->strings[LEFT]) {
		value = PAGE_BREAK_AFTER_LEFT;
	} else if (ident->ilower == c->strings[RIGHT]) {
		value = PAGE_BREAK_AFTER_RIGHT;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_PAGE_BREAK_AFTER, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_page_break_before(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (auto, always, avoid, left, right, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[AUTO]) {
		value = PAGE_BREAK_BEFORE_AUTO;
	} else if (ident->ilower == c->strings[ALWAYS]) {
		value = PAGE_BREAK_BEFORE_ALWAYS;
	} else if (ident->ilower == c->strings[AVOID]) {
		value = PAGE_BREAK_BEFORE_AVOID;
	} else if (ident->ilower == c->strings[LEFT]) {
		value = PAGE_BREAK_BEFORE_LEFT;
	} else if (ident->ilower == c->strings[RIGHT]) {
		value = PAGE_BREAK_BEFORE_RIGHT;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_PAGE_BREAK_BEFORE, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_page_break_inside(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (auto, avoid, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[AUTO]) {
		value = PAGE_BREAK_INSIDE_AUTO;
	} else if (ident->ilower == c->strings[AVOID]) {
		value = PAGE_BREAK_INSIDE_AVOID;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_PAGE_BREAK_INSIDE, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_pause_after(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* time | percentage | IDENT(inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_S,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if ((unit & UNIT_TIME) == false && (unit & UNIT_PCT) == false)
			return CSS_INVALID;

		/* Negative values are illegal */
		if (length < 0)
			return CSS_INVALID;

		value = PAUSE_AFTER_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_PAUSE_AFTER, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == PAUSE_AFTER_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == PAUSE_AFTER_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_pause_before(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* time | percentage | IDENT(inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_S,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if ((unit & UNIT_TIME) == false && (unit & UNIT_PCT) == false)
			return CSS_INVALID;

		/* Negative values are illegal */
		if (length < 0)
			return CSS_INVALID;

		value = PAUSE_BEFORE_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_PAUSE_BEFORE, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == PAUSE_BEFORE_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == PAUSE_BEFORE_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_pitch_range(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed num = 0;
	uint32_t required_size;

	/* number | IDENT (inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_NUMBER))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		num = number_from_lwc_string(token->ilower, false, &consumed);
		/* Invalid if there are trailing characters */
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;

		/* Must be between 0 and 100 */
		if (num < 0 || num > F_100)
			return CSS_INVALID;

		value = PITCH_RANGE_SET;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_PITCH_RANGE, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == PITCH_RANGE_SET)
		required_size += sizeof(num);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == PITCH_RANGE_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv), 
				&num, sizeof(num));
	}

	return CSS_OK;
}

css_error parse_pitch(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* frequency | IDENT(x-low, low, medium, high, x-high, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[X_LOW]) {
		parserutils_vector_iterate(vector, ctx);
		value = PITCH_X_LOW;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[LOW]) {
		parserutils_vector_iterate(vector, ctx);
		value = PITCH_LOW;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[MEDIUM]) {
		parserutils_vector_iterate(vector, ctx);
		value = PITCH_MEDIUM;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[HIGH]) {
		parserutils_vector_iterate(vector, ctx);
		value = PITCH_HIGH;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[X_HIGH]) {
		parserutils_vector_iterate(vector, ctx);
		value = PITCH_X_HIGH;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_HZ,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if ((unit & UNIT_FREQ) == false)
			return CSS_INVALID;

		/* Negative values are invalid */
		if (length < 0)
			return CSS_INVALID;

		value = PITCH_FREQUENCY;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_PITCH, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == PITCH_FREQUENCY)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == PITCH_FREQUENCY) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_play_during(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size;
	lwc_string *uri;

	/* URI [ IDENT(mix) || IDENT(repeat) ]? | IDENT(auto,none,inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_URI))
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT) {
		if (token->ilower == c->strings[INHERIT]) {
			flags |= FLAG_INHERIT;
		} else if (token->ilower == c->strings[NONE]) {
			value = PLAY_DURING_NONE;
		} else if (token->ilower == c->strings[AUTO]) {
			value = PLAY_DURING_AUTO;
		} else
			return CSS_INVALID;
	} else {
		int flags;

		value = PLAY_DURING_URI;
		uri = token->idata;

		for (flags = 0; flags < 2; flags++) {
			consumeWhitespace(vector, ctx);

			token = parserutils_vector_peek(vector, *ctx);
			if (token != NULL && token->type == CSS_TOKEN_IDENT) {
				if (token->ilower == c->strings[MIX]) {
					if ((value & PLAY_DURING_MIX) == 0)
						value |= PLAY_DURING_MIX;
					else
						return CSS_INVALID;
				} else if (token->ilower == 
						c->strings[REPEAT]) {
					if ((value & PLAY_DURING_REPEAT) == 0)
						value |= PLAY_DURING_REPEAT;
					else
						return CSS_INVALID;
				} else
					return CSS_INVALID;

				parserutils_vector_iterate(vector, ctx);
			}
		}
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_PLAY_DURING, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && 
			(value & PLAY_DURING_TYPE_MASK) == PLAY_DURING_URI)
		required_size += sizeof(lwc_string *);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && 
			(value & PLAY_DURING_TYPE_MASK)  == PLAY_DURING_URI) {
		lwc_context_string_ref(c->sheet->dictionary, uri);
                memcpy((uint8_t *) (*result)->bytecode + sizeof(opv),
				&uri, sizeof(lwc_string *));
	}

	return CSS_OK;
}

css_error parse_position(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (static, relative, absolute, fixed, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[STATIC]) {
		value = POSITION_STATIC;
	} else if (ident->ilower == c->strings[RELATIVE]) {
		value = POSITION_RELATIVE;
	} else if (ident->ilower == c->strings[ABSOLUTE]) {
		value = POSITION_ABSOLUTE;
	} else if (ident->ilower == c->strings[FIXED]) {
		value = POSITION_FIXED;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_POSITION, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_quotes(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size = sizeof(opv);
	int temp_ctx = *ctx;
	uint8_t *ptr;

	/* [ STRING STRING ]+ | IDENT(none,inherit) */ 

	/* Pass 1: validate input and calculate bytecode size */
	token = parserutils_vector_iterate(vector, &temp_ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_STRING))
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT) {
		if (token->ilower == c->strings[INHERIT]) {
			flags = FLAG_INHERIT;
		} else if (token->ilower == c->strings[NONE]) {
			value = QUOTES_NONE;
		} else
			return CSS_INVALID;
	} else {
		bool first = true;

		/* [ STRING STRING ] + */
		while (token != NULL && token->type == CSS_TOKEN_STRING) {
			lwc_string *open = token->idata;
			lwc_string *close;

			consumeWhitespace(vector, &temp_ctx);

			token = parserutils_vector_peek(vector, temp_ctx);
			if (token == NULL || token->type != CSS_TOKEN_STRING)
				return CSS_INVALID;

			close = token->idata;

			token = parserutils_vector_iterate(vector, &temp_ctx);

			consumeWhitespace(vector, &temp_ctx);

			if (first == false) {
				required_size += sizeof(opv);
			} else {
				value = QUOTES_STRING;
			}
			required_size += sizeof(open) + sizeof(close);

			first = false;

			token = parserutils_vector_peek(vector, temp_ctx);
			if (token == NULL || token->type != CSS_TOKEN_STRING)
				break;
			token = parserutils_vector_iterate(vector, &temp_ctx);
		}

		consumeWhitespace(vector, &temp_ctx);

		token = parserutils_vector_peek(vector, temp_ctx);
		if (token != NULL && tokenIsChar(token, '!') == false)
			return CSS_INVALID;

		/* Terminator */
		required_size += sizeof(opv);
	}

	error = parse_important(c, vector, &temp_ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_QUOTES, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy OPV to bytecode */
	ptr = (*result)->bytecode;
	memcpy(ptr, &opv, sizeof(opv));
	ptr += sizeof(opv);

	/* Pass 2: construct bytecode */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_STRING))
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT) {
		/* Nothing to do */
	} else {
		bool first = true;

		/* [ STRING STRING ]+ */
		while (token != NULL && token->type == CSS_TOKEN_STRING) {
			lwc_string *open = token->idata;
			lwc_string *close;

			consumeWhitespace(vector, ctx);

			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL || token->type != CSS_TOKEN_STRING)
				return CSS_INVALID;

			close = token->idata;

			token = parserutils_vector_iterate(vector, ctx);

			consumeWhitespace(vector, ctx);

			if (first == false) {
				opv = QUOTES_STRING;
				memcpy(ptr, &opv, sizeof(opv));
				ptr += sizeof(opv);
			}
                        
                        lwc_context_string_ref(c->sheet->dictionary, open);
			memcpy(ptr, &open, sizeof(open));
			ptr += sizeof(open);
			
                        lwc_context_string_ref(c->sheet->dictionary, close);
                        memcpy(ptr, &close, sizeof(close));
			ptr += sizeof(close);

			first = false;

			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL || token->type != CSS_TOKEN_STRING)
				break;
			token = parserutils_vector_iterate(vector, ctx);
		}

		consumeWhitespace(vector, ctx);

		token = parserutils_vector_peek(vector, *ctx);
		if (token != NULL && tokenIsChar(token, '!') == false)
			return CSS_INVALID;

		/* Terminator */
		opv = QUOTES_NONE;
		memcpy(ptr, &opv, sizeof(opv));
		ptr += sizeof(opv);
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	return CSS_OK;
}

css_error parse_richness(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed num = 0;
	uint32_t required_size;

	/* number | IDENT (inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_NUMBER))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		num = number_from_lwc_string(token->ilower, false, &consumed);
		/* Invalid if there are trailing characters */
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;

		/* Must be between 0 and 100 */
		if (num < 0 || num > F_100)
			return CSS_INVALID;

		value = RICHNESS_SET;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_RICHNESS, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == RICHNESS_SET)
		required_size += sizeof(num);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == RICHNESS_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv), 
				&num, sizeof(num));
	}

	return CSS_OK;
}

css_error parse_right(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(auto, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[AUTO]) {
		parserutils_vector_iterate(vector, ctx);
		value = RIGHT_AUTO;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		value = RIGHT_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_RIGHT, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == RIGHT_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == RIGHT_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_speak_header(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (once, always, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[ONCE]) {
		value = SPEAK_HEADER_ONCE;
	} else if (ident->ilower == c->strings[ALWAYS]) {
		value = SPEAK_HEADER_ALWAYS;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_SPEAK_HEADER, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_speak_numeral(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (digits, continuous, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[DIGITS]) {
		value = SPEAK_NUMERAL_DIGITS;
	} else if (ident->ilower == c->strings[CONTINUOUS]) {
		value = SPEAK_NUMERAL_CONTINUOUS;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_SPEAK_NUMERAL, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_speak_punctuation(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (code, none, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[CODE]) {
		value = SPEAK_PUNCTUATION_CODE;
	} else if (ident->ilower == c->strings[NONE]) {
		value = SPEAK_PUNCTUATION_NONE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_SPEAK_PUNCTUATION, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_speak(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (normal, none, spell-out, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[NORMAL]) {
		value = SPEAK_NORMAL;
	} else if (ident->ilower == c->strings[NONE]) {
		value = SPEAK_NONE;
	} else if (ident->ilower == c->strings[SPELL_OUT]) {
		value = SPEAK_SPELL_OUT;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_SPEAK, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_speech_rate(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed num = 0;
	uint32_t required_size;

	/* number | IDENT (x-slow, slow, medium, fast, x-fast, faster, slower, 
	 *                 inherit)
	 */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_NUMBER))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[X_SLOW]) {
		value = SPEECH_RATE_X_SLOW;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[SLOW]) {
		value = SPEECH_RATE_SLOW;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[MEDIUM]) {
		value = SPEECH_RATE_MEDIUM;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[FAST]) {
		value = SPEECH_RATE_FAST;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[X_FAST]) {
		value = SPEECH_RATE_X_FAST;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[FASTER]) {
		value = SPEECH_RATE_FASTER;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[SLOWER]) {
		value = SPEECH_RATE_SLOWER;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		num = number_from_lwc_string(token->ilower, false, &consumed);
		/* Invalid if there are trailing characters */
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;

		/* Make negative values invalid */
		if (num < 0)
			return CSS_INVALID;

		value = SPEECH_RATE_SET;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_SPEECH_RATE, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == SPEECH_RATE_SET)
		required_size += sizeof(num);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == SPEECH_RATE_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv), 
				&num, sizeof(num));
	}

	return CSS_OK;
}

css_error parse_stress(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed num = 0;
	uint32_t required_size;

	/* number | IDENT (inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_NUMBER))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		num = number_from_lwc_string(token->ilower, false, &consumed);
		/* Invalid if there are trailing characters */
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;

		if (num < 0 || num > INTTOFIX(100))
			return CSS_INVALID;

		value = STRESS_SET;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_STRESS, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == STRESS_SET)
		required_size += sizeof(num);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == STRESS_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv), 
				&num, sizeof(num));
	}

	return CSS_OK;
}

css_error parse_table_layout(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (auto, fixed, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[AUTO]) {
		value = TABLE_LAYOUT_AUTO;
	} else if (ident->ilower == c->strings[FIXED]) {
		value = TABLE_LAYOUT_FIXED;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_TABLE_LAYOUT, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_text_align(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (left, right, center, justify, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[LEFT]) {
		value = TEXT_ALIGN_LEFT;
	} else if (ident->ilower == c->strings[RIGHT]) {
		value = TEXT_ALIGN_RIGHT;
	} else if (ident->ilower == c->strings[CENTER]) {
		value = TEXT_ALIGN_CENTER;
	} else if (ident->ilower == c->strings[JUSTIFY]) {
		value = TEXT_ALIGN_JUSTIFY;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_TEXT_ALIGN, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_text_decoration(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT([ underline || overline || line-through || blink ])
	 * | IDENT (none, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[NONE]) {
		value = TEXT_DECORATION_NONE;
	} else {
		while (ident != NULL) {
			if (ident->ilower == c->strings[UNDERLINE]) {
				if ((value & TEXT_DECORATION_UNDERLINE) == 0)
					value |= TEXT_DECORATION_UNDERLINE;
				else
					return CSS_INVALID;
			} else if (ident->ilower == c->strings[OVERLINE]) {
				if ((value & TEXT_DECORATION_OVERLINE) == 0)
					value |= TEXT_DECORATION_OVERLINE;
				else
					return CSS_INVALID;
			} else if (ident->ilower == c->strings[LINE_THROUGH]) {
				if ((value & TEXT_DECORATION_LINE_THROUGH) == 0)
					value |= TEXT_DECORATION_LINE_THROUGH;
				else
					return CSS_INVALID;
			} else if (ident->ilower == c->strings[BLINK]) {
				if ((value & TEXT_DECORATION_BLINK) == 0)
					value |= TEXT_DECORATION_BLINK;
				else
					return CSS_INVALID;
			} else
				return CSS_INVALID;

			consumeWhitespace(vector, ctx);

			ident = parserutils_vector_peek(vector, *ctx);
			if (ident != NULL && ident->type != CSS_TOKEN_IDENT)
				break;
			ident = parserutils_vector_iterate(vector, ctx);
		}
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_TEXT_DECORATION, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_text_indent(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		value = TEXT_INDENT_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_TEXT_INDENT, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == TEXT_INDENT_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == TEXT_INDENT_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_text_transform(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (capitalize, uppercase, lowercase, none, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[CAPITALIZE]) {
		value = TEXT_TRANSFORM_CAPITALIZE;
	} else if (ident->ilower == c->strings[UPPERCASE]) {
		value = TEXT_TRANSFORM_UPPERCASE;
	} else if (ident->ilower == c->strings[LOWERCASE]) {
		value = TEXT_TRANSFORM_LOWERCASE;
	} else if (ident->ilower == c->strings[NONE]) {
		value = TEXT_TRANSFORM_NONE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_TEXT_TRANSFORM, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_top(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(auto, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[AUTO]) {
		parserutils_vector_iterate(vector, ctx);
		value = TOP_AUTO;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		value = TOP_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_TOP, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == TOP_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == TOP_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_unicode_bidi(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (normal, embed, bidi-override, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[NORMAL]) {
		value = UNICODE_BIDI_NORMAL;
	} else if (ident->ilower == c->strings[EMBED]) {
		value = UNICODE_BIDI_EMBED;
	} else if (ident->ilower == c->strings[BIDI_OVERRIDE]) {
		value = UNICODE_BIDI_BIDI_OVERRIDE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_UNICODE_BIDI, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_vertical_align(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(baseline, sub, super, top, text-top,
	 *                             middle, bottom, text-bottom, inherit)
	 */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[BASELINE]) {
		parserutils_vector_iterate(vector, ctx);
		value = VERTICAL_ALIGN_BASELINE;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[SUB]) {
		parserutils_vector_iterate(vector, ctx);
		value = VERTICAL_ALIGN_SUB;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[SUPER]) {
		parserutils_vector_iterate(vector, ctx);
		value = VERTICAL_ALIGN_SUPER;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[TOP]) {
		parserutils_vector_iterate(vector, ctx);
		value = VERTICAL_ALIGN_TOP;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[TEXT_TOP]) {
		parserutils_vector_iterate(vector, ctx);
		value = VERTICAL_ALIGN_TEXT_TOP;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[MIDDLE]) {
		parserutils_vector_iterate(vector, ctx);
		value = VERTICAL_ALIGN_MIDDLE;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[BOTTOM]) {
		parserutils_vector_iterate(vector, ctx);
		value = VERTICAL_ALIGN_BOTTOM;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[TEXT_BOTTOM]) {
		parserutils_vector_iterate(vector, ctx);
		value = VERTICAL_ALIGN_TEXT_BOTTOM;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		value = VERTICAL_ALIGN_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_VERTICAL_ALIGN, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == VERTICAL_ALIGN_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == VERTICAL_ALIGN_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_visibility(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (visible, hidden, collapse, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[VISIBLE]) {
		value = VISIBILITY_VISIBLE;
	} else if (ident->ilower == c->strings[HIDDEN]) {
		value = VISIBILITY_HIDDEN;
	} else if (ident->ilower == c->strings[COLLAPSE]) {
		value = VISIBILITY_COLLAPSE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_VISIBILITY, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_voice_family(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	uint32_t required_size = sizeof(opv);
	int temp_ctx = *ctx;
	uint8_t *ptr;

	/* [ IDENT+ | STRING ] [ ',' [ IDENT+ | STRING ] ]* | IDENT(inherit)
	 * 
	 * In the case of IDENT+, any whitespace between tokens is collapsed to
	 * a single space
	 */

	/* Pass 1: validate input and calculate space */
	token = parserutils_vector_iterate(vector, &temp_ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_STRING))
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		flags = FLAG_INHERIT;
	} else {
		bool first = true;

		while (token != NULL) {
			if (token->type == CSS_TOKEN_IDENT) {
				if (first == false) {
					required_size += sizeof(opv);
				}

				if (token->ilower == c->strings[MALE]) {
					if (first) {
						value = VOICE_FAMILY_MALE;
					}
				} else if (token->ilower == 
						c->strings[FEMALE]) {
					if (first) {
						value = VOICE_FAMILY_FEMALE;
					}
				} else if (token->ilower == c->strings[CHILD]) {
					if (first) {
						value = VOICE_FAMILY_CHILD;
					}
				} else {
					if (first) {
						value = VOICE_FAMILY_IDENT_LIST;
					}

					required_size +=
						sizeof(lwc_string *);

					/* Skip past [ IDENT* S* ]* */
					while (token != NULL) {
						token = parserutils_vector_peek(
								vector, 
								temp_ctx);
						if (token != NULL && 
							token->type != 
							CSS_TOKEN_IDENT &&
								token->type != 
								CSS_TOKEN_S) {
							break;
						}

						/* idents must not match 
						 * generic families */
						if (token != NULL && token->type == CSS_TOKEN_IDENT && 
								(token->ilower == c->strings[MALE] || 
								token->ilower == c->strings[FEMALE] || 
								token->ilower == c->strings[CHILD]))
							return CSS_INVALID;
						token = parserutils_vector_iterate(
							vector, &temp_ctx);
					}
				}
			} else if (token->type == CSS_TOKEN_STRING) {
				if (first == false) {
					required_size += sizeof(opv);
				} else {
					value = VOICE_FAMILY_STRING;
				}

				required_size += 
					sizeof(lwc_string *);
			} else {
				return CSS_INVALID;
			}

			consumeWhitespace(vector, &temp_ctx);

			token = parserutils_vector_peek(vector, temp_ctx);
			if (token != NULL && tokenIsChar(token, ',') == false &&
					tokenIsChar(token, '!') == false) {
				return CSS_INVALID;
			}

			if (token != NULL && tokenIsChar(token, ',')) {
				parserutils_vector_iterate(vector, &temp_ctx);

				consumeWhitespace(vector, &temp_ctx);

				token = parserutils_vector_peek(vector, 
						temp_ctx);
				if (token == NULL || tokenIsChar(token, '!'))
					return CSS_INVALID;
			}

			first = false;

			token = parserutils_vector_peek(vector, temp_ctx);
			if (token != NULL && tokenIsChar(token, '!'))
				break;

			token = parserutils_vector_iterate(vector, &temp_ctx);
		}

		required_size += sizeof(opv);
	}

	error = parse_important(c, vector, &temp_ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_VOICE_FAMILY, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy OPV to bytecode */
	ptr = (*result)->bytecode;
	memcpy(ptr, &opv, sizeof(opv));
	ptr += sizeof(opv);

	/* Pass 2: populate bytecode */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_STRING)) {
		css_stylesheet_style_destroy(c->sheet, *result);
		*result = NULL;
		return CSS_INVALID;
	}

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		/* Nothing to do */
	} else {
		bool first = true;

		while (token != NULL) {
			if (token->type == CSS_TOKEN_IDENT) {
                                lwc_string *tok_idata = token->idata;
				lwc_string *name = tok_idata;
                                lwc_string *newname;
			
				if (token->ilower == c->strings[MALE]) {
					opv = VOICE_FAMILY_MALE;
				} else if (token->ilower == 
						c->strings[FEMALE]) {
					opv = VOICE_FAMILY_FEMALE;
				} else if (token->ilower == c->strings[CHILD]) {
					opv = VOICE_FAMILY_CHILD;
				} else {
					uint16_t len = lwc_string_length(token->idata);
					const css_token *temp_token = token;
                                        lwc_error lerror;
					uint8_t *buf;
					uint8_t *p;

					temp_ctx = *ctx;

					opv = VOICE_FAMILY_IDENT_LIST;

					/* Build string from idents */
					while (temp_token != NULL) {
						temp_token = parserutils_vector_peek(
								vector, temp_ctx);
						if (temp_token != NULL && 
							temp_token->type != 
							CSS_TOKEN_IDENT &&
								temp_token->type != 
								CSS_TOKEN_S) {
							break;
						}

						if (temp_token != NULL && temp_token->type == CSS_TOKEN_IDENT) {
							len += lwc_string_length(temp_token->idata);
						} else if (temp_token != NULL) {
							len += 1;
						}

						temp_token = parserutils_vector_iterate(
								vector, &temp_ctx);
					}

					/** \todo Don't use alloca */
					buf = alloca(len);
					p = buf;

					memcpy(p, lwc_string_data(token->idata), lwc_string_length(token->idata));
					p += lwc_string_length(token->idata);

					while (token != NULL) {
						token = parserutils_vector_peek(
								vector, *ctx);
						if (token != NULL &&
							token->type != 
							CSS_TOKEN_IDENT &&
								token->type !=
								CSS_TOKEN_S) {
							break;
						}

						if (token != NULL && 
							token->type == 
								CSS_TOKEN_IDENT) {
							memcpy(p,
								lwc_string_data(token->idata),
								lwc_string_length(token->idata));
							p += lwc_string_length(token->idata);
						} else if (token != NULL) {
							*p++ = ' ';
						}

						token = parserutils_vector_iterate(
							vector, ctx);
					}

					/* Strip trailing whitespace */
					while (p > buf && p[-1] == ' ')
						p--;

					/* Insert into hash, if it's different
					 * from the name we already have */
                                        lerror = lwc_context_intern(c->sheet->dictionary,
                                                                    (char *)buf, len, &newname);
                                        if (lerror != lwc_error_ok) {
                                                css_stylesheet_style_destroy(c->sheet, *result);
                                                *result = NULL;
                                                return css_error_from_lwc_error(lerror);
                                        }
                                        
                                        if (newname == name)
                                                lwc_context_string_unref(c->sheet->dictionary,
                                                                         newname);
                                        
                                        name = newname;
                                }

				if (first == false) {
					memcpy(ptr, &opv, sizeof(opv));
					ptr += sizeof(opv);
				}

				if (opv == VOICE_FAMILY_IDENT_LIST) {
                                        /* Only ref 'name' again if the token owns it,
                                         * otherwise we already own the only ref to the
                                         * new name generated above.
                                         */
                                        if (name == tok_idata)
                                                lwc_context_string_ref(c->sheet->dictionary, name);
					memcpy(ptr, &name, sizeof(name));
					ptr += sizeof(name);
				}
			} else if (token->type == CSS_TOKEN_STRING) {
				opv = VOICE_FAMILY_STRING;

				if (first == false) {
					memcpy(ptr, &opv, sizeof(opv));
					ptr += sizeof(opv);
				}
                                
                                lwc_context_string_ref(c->sheet->dictionary, token->idata);
				memcpy(ptr, &token->idata, 
						sizeof(token->idata));
				ptr += sizeof(token->idata);
			} else {
				css_stylesheet_style_destroy(c->sheet, *result);
				*result = NULL;
				return CSS_INVALID;
			}

			consumeWhitespace(vector, ctx);

			token = parserutils_vector_peek(vector, *ctx);
			if (token != NULL && tokenIsChar(token, ',') == false &&
					tokenIsChar(token, '!') == false) {
				css_stylesheet_style_destroy(c->sheet, *result);
				*result = NULL;
				return CSS_INVALID;
			}

			if (token != NULL && tokenIsChar(token, ',')) {
				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);

				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL || tokenIsChar(token, '!')) {
					css_stylesheet_style_destroy(c->sheet,
							*result);
					*result = NULL;
					return CSS_INVALID;
				}
			}

			first = false;

			token = parserutils_vector_peek(vector, *ctx);
			if (token != NULL && tokenIsChar(token, '!'))
				break;

			token = parserutils_vector_iterate(vector, ctx);
		}

		/* Write terminator */
		opv = VOICE_FAMILY_END;
		memcpy(ptr, &opv, sizeof(opv));
		ptr += sizeof(opv);
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK) {
		css_stylesheet_style_destroy(c->sheet, *result);
		*result = NULL;
		return error;
	}

	return CSS_OK;
}

css_error parse_volume(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* number | percentage | IDENT(silent, x-soft, soft, medium, loud, 
	 *                             x-loud, inherit)
	 */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[SILENT]) {
		parserutils_vector_iterate(vector, ctx);
		value = VOLUME_SILENT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[X_SOFT]) {
		parserutils_vector_iterate(vector, ctx);
		value = VOLUME_X_SOFT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[SOFT]) {
		parserutils_vector_iterate(vector, ctx);
		value = VOLUME_SOFT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[MEDIUM]) {
		parserutils_vector_iterate(vector, ctx);
		value = VOLUME_MEDIUM;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[LOUD]) {
		parserutils_vector_iterate(vector, ctx);
		value = VOLUME_LOUD;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[X_LOUD]) {
		parserutils_vector_iterate(vector, ctx);
		value = VOLUME_X_LOUD;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		length = number_from_lwc_string(token->ilower, false, &consumed);
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;

		/* Must be between 0 and 100 */
		if (length < 0 || length > F_100)
			return CSS_INVALID;

		parserutils_vector_iterate(vector, ctx);
		value = VOLUME_NUMBER;
	} else {
		/* Yes, really UNIT_PX -- percentages MUST have a % sign */
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if ((unit & UNIT_PCT) == false)
			return CSS_INVALID;

		/* Must be positive */
		if (length < 0)
			return CSS_INVALID;

		value = VOLUME_DIMENSION;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_VOLUME, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == VOLUME_NUMBER)
		required_size += sizeof(length);
	else if ((flags & FLAG_INHERIT) == false && value == VOLUME_DIMENSION)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && (value == VOLUME_NUMBER || 
			value == VOLUME_DIMENSION))
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
	if ((flags & FLAG_INHERIT) == false && value == VOLUME_DIMENSION)
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));

	return CSS_OK;
}

css_error parse_white_space(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (normal, pre, nowrap, pre-wrap, pre-line, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[NORMAL]) {
		value = WHITE_SPACE_NORMAL;
	} else if (ident->ilower == c->strings[PRE]) {
		value = WHITE_SPACE_PRE;
	} else if (ident->ilower == c->strings[NOWRAP]) {
		value = WHITE_SPACE_NOWRAP;
	} else if (ident->ilower == c->strings[PRE_WRAP]) {
		value = WHITE_SPACE_PRE_WRAP;
	} else if (ident->ilower == c->strings[PRE_LINE]) {
		value = WHITE_SPACE_PRE_LINE;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_WHITE_SPACE, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_widows(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed num = 0;
	uint32_t required_size;

	/* <integer> | IDENT (inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_NUMBER))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		num = number_from_lwc_string(token->ilower, true, &consumed);
		/* Invalid if there are trailing characters */
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;

		/* Negative values are nonsensical */
		if (num < 0)
			return CSS_INVALID;

		value = WIDOWS_SET;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_WIDOWS, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == WIDOWS_SET)
		required_size += sizeof(num);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == WIDOWS_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv), 
				&num, sizeof(num));
	}

	return CSS_OK;
}

css_error parse_width(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(auto, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[AUTO]) {
		parserutils_vector_iterate(vector, ctx);
		value = WIDTH_AUTO;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Must be positive */
		if (length < 0)
			return CSS_INVALID;

		value = WIDTH_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_WIDTH, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == WIDTH_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == WIDTH_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_word_spacing(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | IDENT(normal, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[NORMAL]) {
		parserutils_vector_iterate(vector, ctx);
		value = WORD_SPACING_NORMAL;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ ||
				unit & UNIT_PCT)
			return CSS_INVALID;

		value = WORD_SPACING_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(CSS_PROP_WORD_SPACING, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == WORD_SPACING_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == WORD_SPACING_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_z_index(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed num = 0;
	uint32_t required_size;

	/* <integer> | IDENT (auto, inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_NUMBER))
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[AUTO]) {
		value = Z_INDEX_AUTO;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		num = number_from_lwc_string(token->ilower, true, &consumed);
		/* Invalid if there are trailing characters */
		if (consumed != lwc_string_length(token->ilower))
			return CSS_INVALID;

		value = Z_INDEX_SET;
	} else
		return CSS_INVALID;

	opv = buildOPV(CSS_PROP_Z_INDEX, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == Z_INDEX_SET)
		required_size += sizeof(num);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == Z_INDEX_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv), 
				&num, sizeof(num));
	}

	return CSS_OK;
}

css_error parse_important(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint8_t *result)
{
	const css_token *token;

	consumeWhitespace(vector, ctx);

	token = parserutils_vector_iterate(vector, ctx);
	if (token != NULL && tokenIsChar(token, '!')) {
		consumeWhitespace(vector, ctx);

		token = parserutils_vector_iterate(vector, ctx);
		if (token == NULL || token->type != CSS_TOKEN_IDENT)
			return CSS_INVALID;

		if (token->ilower == c->strings[IMPORTANT])
			*result |= FLAG_IMPORTANT;
	} else if (token != NULL)
		return CSS_INVALID;

	return CSS_OK;
}

css_error parse_colour_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint32_t *result)
{
	const css_token *token;
	uint8_t r = 0, g = 0, b = 0;
	css_error error;

	UNUSED(c);

	consumeWhitespace(vector, ctx);

	/* IDENT(<colour name>) | HASH(rgb | rrggbb) |
	 * FUNCTION(rgb) [ [ NUMBER | PERCENTAGE ] ',' ] {3} ')'
	 *
	 * For quirks, NUMBER | DIMENSION | IDENT, too
	 * I.E. "123456" -> NUMBER, "1234f0" -> DIMENSION, "f00000" -> IDENT
	 */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_HASH &&
			token->type != CSS_TOKEN_FUNCTION)) {
		if (c->sheet->quirks_allowed == false ||
				(token->type != CSS_TOKEN_NUMBER && 
				token->type != CSS_TOKEN_DIMENSION))
			return CSS_INVALID;
	}

	if (token->type == CSS_TOKEN_IDENT) {
		error = parse_named_colour(c, token->idata, result);
		if (error != CSS_OK && c->sheet->quirks_allowed) {
			error = parse_hash_colour(token->idata, result);
			if (error == CSS_OK)
				c->sheet->quirks_used = true;
		}
			
		return error;
	} else if (token->type == CSS_TOKEN_HASH) {
		return parse_hash_colour(token->idata, result);
	} else if (c->sheet->quirks_allowed && 
			token->type == CSS_TOKEN_NUMBER) {
		error = parse_hash_colour(token->idata, result);
		if (error == CSS_OK)
			c->sheet->quirks_used = true;
		return error;
	} else if (c->sheet->quirks_allowed && 
			token->type == CSS_TOKEN_DIMENSION) {
		error = parse_hash_colour(token->idata, result);
		if (error == CSS_OK)
			c->sheet->quirks_used = true;
		return error;
	} else if (token->type == CSS_TOKEN_FUNCTION) {
		if (token->ilower == c->strings[RGB]) {
			int i;
			css_token_type valid = CSS_TOKEN_NUMBER;

			for (i = 0; i < 3; i++) {
				css_fixed num;
				size_t consumed = 0;
				uint8_t *component = i == 0 ? &r 
							    : i == 1 ? &b : &g;
				int32_t intval;

				consumeWhitespace(vector, ctx);

				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL || (token->type != 
						CSS_TOKEN_NUMBER && 
						token->type != 
						CSS_TOKEN_PERCENTAGE))
					return CSS_INVALID;

				if (i == 0)
					valid = token->type;
				else if (token->type != valid)
					return CSS_INVALID;

				num = number_from_lwc_string(token->idata,
						valid == CSS_TOKEN_NUMBER, 
						&consumed);
				if (consumed != lwc_string_length(token->idata))
					return CSS_INVALID;

				if (valid == CSS_TOKEN_NUMBER) {
					intval = FIXTOINT(num);
				} else {
					intval = FIXTOINT(
						FDIVI(FMULI(num, 255), 100));
				}

				if (intval > 255)
					*component = 255;
				else if (intval < 0)
					*component = 0;
				else
					*component = intval;

				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);

				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL)
					return CSS_INVALID;

				if (i != 2 && tokenIsChar(token, ','))
					parserutils_vector_iterate(vector, ctx);
				else if (i == 2 && tokenIsChar(token, ')'))
					parserutils_vector_iterate(vector, ctx);
				else
					return CSS_INVALID;
			}
		} else
			return CSS_INVALID;
	}

	*result = (r << 24) | (g << 16) | (b << 8);

	return CSS_OK;
}

css_error parse_named_colour(css_language *c, lwc_string *data, 
		uint32_t *result)
{
	static const uint32_t colourmap[LAST_COLOUR + 1 - FIRST_COLOUR] = {
		0xf0f8ff00, /* ALICEBLUE */
		0xfaebd700, /* ANTIQUEWHITE */
		0x00ffff00, /* AQUA */
		0x7fffd400, /* AQUAMARINE */
		0xf0ffff00, /* AZURE */
		0xf5f5dc00, /* BEIGE */
		0xffe4c400, /* BISQUE */
		0x00000000, /* BLACK */
		0xffebcd00, /* BLANCHEDALMOND */
		0x0000ff00, /* BLUE */
		0x8a2be200, /* BLUEVIOLET */
		0xa52a2a00, /* BROWN */
		0xdeb88700, /* BURLYWOOD */
		0x5f9ea000, /* CADETBLUE */
		0x7fff0000, /* CHARTREUSE */
		0xd2691e00, /* CHOCOLATE */
		0xff7f5000, /* CORAL */
		0x6495ed00, /* CORNFLOWERBLUE */
		0xfff8dc00, /* CORNSILK */
		0xdc143c00, /* CRIMSON */
		0x00ffff00, /* CYAN */
		0x00008b00, /* DARKBLUE */
		0x008b8b00, /* DARKCYAN */
		0xb8860b00, /* DARKGOLDENROD */
		0xa9a9a900, /* DARKGRAY */
		0x00640000, /* DARKGREEN */
		0xa9a9a900, /* DARKGREY */
		0xbdb76b00, /* DARKKHAKI */
		0x8b008b00, /* DARKMAGENTA */
		0x556b2f00, /* DARKOLIVEGREEN */
		0xff8c0000, /* DARKORANGE */
		0x9932cc00, /* DARKORCHID */
		0x8b000000, /* DARKRED */
		0xe9967a00, /* DARKSALMON */
		0x8fbc8f00, /* DARKSEAGREEN */
		0x483d8b00, /* DARKSLATEBLUE */
		0x2f4f4f00, /* DARKSLATEGRAY */
		0x2f4f4f00, /* DARKSLATEGREY */
		0x00ced100, /* DARKTURQUOISE */
		0x9400d300, /* DARKVIOLET */
		0xff149300, /* DEEPPINK */
		0x00bfff00, /* DEEPSKYBLUE */
		0x69696900, /* DIMGRAY */
		0x69696900, /* DIMGREY */
		0x1e90ff00, /* DODGERBLUE */
		0xd1927500, /* FELDSPAR */
		0xb2222200, /* FIREBRICK */
		0xfffaf000, /* FLORALWHITE */
		0x228b2200, /* FORESTGREEN */
		0xff00ff00, /* FUCHSIA */
		0xdcdcdc00, /* GAINSBORO */
		0xf8f8ff00, /* GHOSTWHITE */
		0xffd70000, /* GOLD */
		0xdaa52000, /* GOLDENROD */
		0x80808000, /* GRAY */
		0x00800000, /* GREEN */
		0xadff2f00, /* GREENYELLOW */
		0x80808000, /* GREY */
		0xf0fff000, /* HONEYDEW */
		0xff69b400, /* HOTPINK */
		0xcd5c5c00, /* INDIANRED */
		0x4b008200, /* INDIGO */
		0xfffff000, /* IVORY */
		0xf0e68c00, /* KHAKI */
		0xe6e6fa00, /* LAVENDER */
		0xfff0f500, /* LAVENDERBLUSH */
		0x7cfc0000, /* LAWNGREEN */
		0xfffacd00, /* LEMONCHIFFON */
		0xadd8e600, /* LIGHTBLUE */
		0xf0808000, /* LIGHTCORAL */
		0xe0ffff00, /* LIGHTCYAN */
		0xfafad200, /* LIGHTGOLDENRODYELLOW */
		0xd3d3d300, /* LIGHTGRAY */
		0x90ee9000, /* LIGHTGREEN */
		0xd3d3d300, /* LIGHTGREY */
		0xffb6c100, /* LIGHTPINK */
		0xffa07a00, /* LIGHTSALMON */
		0x20b2aa00, /* LIGHTSEAGREEN */
		0x87cefa00, /* LIGHTSKYBLUE */
		0x8470ff00, /* LIGHTSLATEBLUE */
		0x77889900, /* LIGHTSLATEGRAY */
		0x77889900, /* LIGHTSLATEGREY */
		0xb0c4de00, /* LIGHTSTEELBLUE */
		0xffffe000, /* LIGHTYELLOW */
		0x00ff0000, /* LIME */
		0x32cd3200, /* LIMEGREEN */
		0xfaf0e600, /* LINEN */
		0xff00ff00, /* MAGENTA */
		0x80000000, /* MAROON */
		0x66cdaa00, /* MEDIUMAQUAMARINE */
		0x0000cd00, /* MEDIUMBLUE */
		0xba55d300, /* MEDIUMORCHID */
		0x9370db00, /* MEDIUMPURPLE */
		0x3cb37100, /* MEDIUMSEAGREEN */
		0x7b68ee00, /* MEDIUMSLATEBLUE */
		0x00fa9a00, /* MEDIUMSPRINGGREEN */
		0x48d1cc00, /* MEDIUMTURQUOISE */
		0xc7158500, /* MEDIUMVIOLETRED */
		0x19197000, /* MIDNIGHTBLUE */
		0xf5fffa00, /* MINTCREAM */
		0xffe4e100, /* MISTYROSE */
		0xffe4b500, /* MOCCASIN */
		0xffdead00, /* NAVAJOWHITE */
		0x00008000, /* NAVY */
		0xfdf5e600, /* OLDLACE */
		0x80800000, /* OLIVE */
		0x6b8e2300, /* OLIVEDRAB */
		0xffa50000, /* ORANGE */
		0xff450000, /* ORANGERED */
		0xda70d600, /* ORCHID */
		0xeee8aa00, /* PALEGOLDENROD */
		0x98fb9800, /* PALEGREEN */
		0xafeeee00, /* PALETURQUOISE */
		0xdb709300, /* PALEVIOLETRED */
		0xffefd500, /* PAPAYAWHIP */
		0xffdab900, /* PEACHPUFF */
		0xcd853f00, /* PERU */
		0xffc0cb00, /* PINK */
		0xdda0dd00, /* PLUM */
		0xb0e0e600, /* POWDERBLUE */
		0x80008000, /* PURPLE */
		0xff000000, /* RED */
		0xbc8f8f00, /* ROSYBROWN */
		0x4169e100, /* ROYALBLUE */
		0x8b451300, /* SADDLEBROWN */
		0xfa807200, /* SALMON */
		0xf4a46000, /* SANDYBROWN */
		0x2e8b5700, /* SEAGREEN */
		0xfff5ee00, /* SEASHELL */
		0xa0522d00, /* SIENNA */
		0xc0c0c000, /* SILVER */
		0x87ceeb00, /* SKYBLUE */
		0x6a5acd00, /* SLATEBLUE */
		0x70809000, /* SLATEGRAY */
		0x70809000, /* SLATEGREY */
		0xfffafa00, /* SNOW */
		0x00ff7f00, /* SPRINGGREEN */
		0x4682b400, /* STEELBLUE */
		0xd2b48c00, /* TAN */
		0x00808000, /* TEAL */
		0xd8bfd800, /* THISTLE */
		0xff634700, /* TOMATO */
		0x40e0d000, /* TURQUOISE */
		0xee82ee00, /* VIOLET */
		0xd0209000, /* VIOLETRED */
		0xf5deb300, /* WHEAT */
		0xffffff00, /* WHITE */
		0xf5f5f500, /* WHITESMOKE */
		0xffff0000, /* YELLOW */
		0x9acd3200  /* YELLOWGREEN */
	};
	int i;

	for (i = FIRST_COLOUR; i <= LAST_COLOUR; i++) {
		if (data == c->strings[i])
			break;
	}
	if (i == LAST_COLOUR + 1)
		return CSS_INVALID;

	*result = colourmap[i - FIRST_COLOUR];

	return CSS_OK;
}

css_error parse_hash_colour(lwc_string *data, uint32_t *result)
{
	uint8_t r = 0, g = 0, b = 0;
	size_t len = lwc_string_length(data);
	const char *input = lwc_string_data(data);

	if (len == 3 &&	isHex(input[0]) && isHex(input[1]) &&
			isHex(input[2])) {
		r = charToHex(input[0]);
		g = charToHex(input[1]);
		b = charToHex(input[2]);

		r |= (r << 4);
		g |= (g << 4);
		b |= (b << 4);
	} else if (len == 6 && isHex(input[0]) && isHex(input[1]) &&
			isHex(input[2]) && isHex(input[3]) &&
			isHex(input[4]) && isHex(input[5])) {
		r = (charToHex(input[0]) << 4);
		r |= charToHex(input[1]);
		g = (charToHex(input[2]) << 4);
		g |= charToHex(input[3]);
		b = (charToHex(input[4]) << 4);
		b |= charToHex(input[5]);
	} else
		return CSS_INVALID;

	*result = (r << 24) | (g << 16) | (b << 8);

	return CSS_OK;
}

css_error parse_unit_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint32_t default_unit,
		css_fixed *length, uint32_t *unit)
{
	const css_token *token;
	css_fixed num;
	size_t consumed = 0;
	css_error error;

	UNUSED(c);

	consumeWhitespace(vector, ctx);

	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_DIMENSION &&
			token->type != CSS_TOKEN_NUMBER &&
			token->type != CSS_TOKEN_PERCENTAGE))
		return CSS_INVALID;

	num = number_from_lwc_string(token->idata, false, &consumed);

	if (token->type == CSS_TOKEN_DIMENSION) {
		size_t len = lwc_string_length(token->idata);
		const char *data = lwc_string_data(token->idata);

		error = parse_unit_keyword(data + consumed, len - consumed, 
				unit);
		if (error != CSS_OK)
			return error;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		/* Non-zero values are permitted in quirks mode */
		if (num != 0) {
			if (c->sheet->quirks_allowed)
				c->sheet->quirks_used = true;
			else
				return CSS_INVALID;
		}

		*unit = default_unit;

		if (c->sheet->quirks_allowed) {
			/* Also, in quirks mode, we need to cater for 
			 * dimensions separated from their units by whitespace
			 * (e.g. "0 px")
			 */
			int temp_ctx = *ctx;
			uint32_t temp_unit;

			consumeWhitespace(vector, &temp_ctx);

			token = parserutils_vector_iterate(vector, &temp_ctx);
			if (token != NULL && token->type == CSS_TOKEN_IDENT) {
				error = parse_unit_keyword(
						lwc_string_data(token->idata),
						lwc_string_length(token->idata),
						&temp_unit);
				if (error == CSS_OK) {
					c->sheet->quirks_used = true;
					*ctx = temp_ctx;
					*unit = temp_unit;
				}
			}
		}

	} else {
		if (consumed != lwc_string_length(token->idata))
			return CSS_INVALID;
		*unit = UNIT_PCT;
	}

	*length = num;

	return CSS_OK;
}

css_error parse_unit_keyword(const char *ptr, size_t len, css_unit *unit)
{
	if (len == 4) {
		if (strncasecmp(ptr, "grad", 4) == 0)
			*unit = UNIT_GRAD;
		else
			return CSS_INVALID;
	} else if (len == 3) {
		if (strncasecmp(ptr, "kHz", 3) == 0)
			*unit = UNIT_KHZ;
		else if (strncasecmp(ptr, "deg", 3) == 0)
			*unit = UNIT_DEG;
		else if (strncasecmp(ptr, "rad", 3) == 0)
			*unit = UNIT_RAD;
		else
			return CSS_INVALID;
	} else if (len == 2) {
		if (strncasecmp(ptr, "Hz", 2) == 0)
			*unit = UNIT_HZ;
		else if (strncasecmp(ptr, "ms", 2) == 0)
			*unit = UNIT_MS;
		else if (strncasecmp(ptr, "px", 2) == 0)
			*unit = UNIT_PX;
		else if (strncasecmp(ptr, "ex", 2) == 0)
			*unit = UNIT_EX;
		else if (strncasecmp(ptr, "em", 2) == 0)
			*unit = UNIT_EM;
		else if (strncasecmp(ptr, "in", 2) == 0)
			*unit = UNIT_IN;
		else if (strncasecmp(ptr, "cm", 2) == 0)
			*unit = UNIT_CM;
		else if (strncasecmp(ptr, "mm", 2) == 0)
			*unit = UNIT_MM;
		else if (strncasecmp(ptr, "pt", 2) == 0)
			*unit = UNIT_PT;
		else if (strncasecmp(ptr, "pc", 2) == 0)
			*unit = UNIT_PC;
		else
			return CSS_INVALID;
	} else if (len == 1) {
		if (strncasecmp(ptr, "s", 1) == 0)
			*unit = UNIT_S;
		else
			return CSS_INVALID;
	} else
		return CSS_INVALID;

	return CSS_OK;
}

css_error parse_border_side_color(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result)
{
	css_error error;
	const css_token *token;
	uint32_t opv;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t colour = 0;
	uint32_t required_size;

	/* colour | IDENT (transparent, inherit) */
	token= parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[TRANSPARENT]) {
		parserutils_vector_iterate(vector, ctx);
		value = BORDER_COLOR_TRANSPARENT;
	} else {
		error = parse_colour_specifier(c, vector, ctx, &colour);
		if (error != CSS_OK)
			return error;

		value = BORDER_COLOR_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(op, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == BORDER_COLOR_SET)
		required_size += sizeof(colour);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == BORDER_COLOR_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&colour, sizeof(colour));
	}

	return CSS_OK;
}

css_error parse_border_side_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result)
{
	css_error error;
	const css_token *ident;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	/* IDENT (none, hidden, dotted, dashed, solid, double, groove, 
	 * ridge, inset, outset, inherit) */
	ident = parserutils_vector_iterate(vector, ctx);
	if (ident == NULL || ident->type != CSS_TOKEN_IDENT)
		return CSS_INVALID;

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	if (ident->ilower == c->strings[INHERIT]) {
		flags |= FLAG_INHERIT;
	} else if (ident->ilower == c->strings[NONE]) {
		value = BORDER_STYLE_NONE;
	} else if (ident->ilower == c->strings[HIDDEN]) {
		value = BORDER_STYLE_HIDDEN;
	} else if (ident->ilower == c->strings[DOTTED]) {
		value = BORDER_STYLE_DOTTED;
	} else if (ident->ilower == c->strings[DASHED]) {
		value = BORDER_STYLE_DASHED;
	} else if (ident->ilower == c->strings[SOLID]) {
		value = BORDER_STYLE_SOLID;
	} else if (ident->ilower == c->strings[DOUBLE]) {
		value = BORDER_STYLE_DOUBLE;
	} else if (ident->ilower == c->strings[GROOVE]) {
		value = BORDER_STYLE_GROOVE;
	} else if (ident->ilower == c->strings[RIDGE]) {
		value = BORDER_STYLE_RIDGE;
	} else if (ident->ilower == c->strings[INSET]) {
		value = BORDER_STYLE_INSET;
	} else if (ident->ilower == c->strings[OUTSET]) {
		value = BORDER_STYLE_OUTSET;
	} else
		return CSS_INVALID;

	opv = buildOPV(op, flags, value);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, sizeof(opv), result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));

	return CSS_OK;
}

css_error parse_border_side_width(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | IDENT(thin, medium, thick, inherit) */
	token= parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT && 
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[THIN]) {
		parserutils_vector_iterate(vector, ctx);
		value = BORDER_WIDTH_THIN;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[MEDIUM]) {
		parserutils_vector_iterate(vector, ctx);
		value = BORDER_WIDTH_MEDIUM;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[THICK]) {
		parserutils_vector_iterate(vector, ctx);
		value = BORDER_WIDTH_THICK;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit == UNIT_PCT || unit & UNIT_ANGLE ||
				unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Length must be positive */
		if (length < 0)
			return CSS_INVALID;

		value = BORDER_WIDTH_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(op, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == BORDER_WIDTH_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == BORDER_WIDTH_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_margin_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(auto, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[AUTO]) {
		parserutils_vector_iterate(vector, ctx);
		value = MARGIN_AUTO;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		value = MARGIN_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(op, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == MARGIN_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == MARGIN_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_padding_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t op, css_style **result)
{
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;
	css_fixed length = 0;
	uint32_t unit = 0;
	uint32_t required_size;

	/* length | percentage | IDENT(inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT &&
			token->ilower == c->strings[INHERIT]) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else {
		error = parse_unit_specifier(c, vector, ctx, UNIT_PX,
				&length, &unit);
		if (error != CSS_OK)
			return error;

		if (unit & UNIT_ANGLE || unit & UNIT_TIME || unit & UNIT_FREQ)
			return CSS_INVALID;

		/* Negative lengths are invalid */
		if (length < 0)
			return CSS_INVALID;

		value = PADDING_SET;
	}

	error = parse_important(c, vector, ctx, &flags);
	if (error != CSS_OK)
		return error;

	opv = buildOPV(op, flags, value);

	required_size = sizeof(opv);
	if ((flags & FLAG_INHERIT) == false && value == PADDING_SET)
		required_size += sizeof(length) + sizeof(unit);

	/* Allocate result */
	error = css_stylesheet_style_create(c->sheet, required_size, result);
	if (error != CSS_OK)
		return error;

	/* Copy the bytecode to it */
	memcpy((*result)->bytecode, &opv, sizeof(opv));
	if ((flags & FLAG_INHERIT) == false && value == PADDING_SET) {
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv),
				&length, sizeof(length));
		memcpy(((uint8_t *) (*result)->bytecode) + sizeof(opv) +
				sizeof(length), &unit, sizeof(unit));
	}

	return CSS_OK;
}

css_error parse_list_style_type_value(css_language *c, const css_token *ident,
		uint16_t *value)
{
	/* IDENT (disc, circle, square, decimal, decimal-leading-zero,
	 *        lower-roman, upper-roman, lower-greek, lower-latin,
	 *        upper-latin, armenian, georgian, lower-alpha, upper-alpha,
	 *        none)
	 */
	if (ident->ilower == c->strings[DISC]) {
		*value = LIST_STYLE_TYPE_DISC;
	} else if (ident->ilower == c->strings[CIRCLE]) {
		*value = LIST_STYLE_TYPE_CIRCLE;
	} else if (ident->ilower == c->strings[SQUARE]) {
		*value = LIST_STYLE_TYPE_SQUARE;
	} else if (ident->ilower == c->strings[DECIMAL]) {
		*value = LIST_STYLE_TYPE_DECIMAL;
	} else if (ident->ilower == c->strings[DECIMAL_LEADING_ZERO]) {
		*value = LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO;
	} else if (ident->ilower == c->strings[LOWER_ROMAN]) {
		*value = LIST_STYLE_TYPE_LOWER_ROMAN;
	} else if (ident->ilower == c->strings[UPPER_ROMAN]) {
		*value = LIST_STYLE_TYPE_UPPER_ROMAN;
	} else if (ident->ilower == c->strings[LOWER_GREEK]) {
		*value = LIST_STYLE_TYPE_LOWER_GREEK;
	} else if (ident->ilower == c->strings[LOWER_LATIN]) {
		*value = LIST_STYLE_TYPE_LOWER_LATIN;
	} else if (ident->ilower == c->strings[UPPER_LATIN]) {
		*value = LIST_STYLE_TYPE_UPPER_LATIN;
	} else if (ident->ilower == c->strings[ARMENIAN]) {
		*value = LIST_STYLE_TYPE_ARMENIAN;
	} else if (ident->ilower == c->strings[GEORGIAN]) {
		*value = LIST_STYLE_TYPE_GEORGIAN;
	} else if (ident->ilower == c->strings[LOWER_ALPHA]) {
		*value = LIST_STYLE_TYPE_LOWER_ALPHA;
	} else if (ident->ilower == c->strings[UPPER_ALPHA]) {
		*value = LIST_STYLE_TYPE_UPPER_ALPHA;
	} else if (ident->ilower == c->strings[NONE]) {
		*value = LIST_STYLE_TYPE_NONE;
	} else
		return CSS_INVALID;

	return CSS_OK;
}

css_error parse_content_list(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t *value, uint8_t *buffer, uint32_t *buflen)
{
	css_error error;
	const css_token *token;
	bool first = true;
	uint32_t offset = 0;
	uint32_t opv;

	/* [
	 *	IDENT(open-quote, close-quote, no-open-quote, no-close-quote) |
	 *	STRING | URI |
	 *	FUNCTION(attr) IDENT ')' |
	 *	FUNCTION(counter) IDENT (',' IDENT)? ')' |
	 *	FUNCTION(counters) IDENT ',' STRING (',' IDENT)? ')'
	 * ]+
	 */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL)
		return CSS_INVALID;

	while (token != NULL) {
		if (token->type == CSS_TOKEN_IDENT &&
				token->ilower == c->strings[OPEN_QUOTE]) {
			opv = CONTENT_OPEN_QUOTE;

			if (first == false) {
				if (buffer != NULL) {
					memcpy(buffer + offset, 
							&opv, sizeof(opv));
				}

				offset += sizeof(opv);
			} 
		} else if (token->type == CSS_TOKEN_IDENT &&
				token->ilower == c->strings[CLOSE_QUOTE]) {
			opv = CONTENT_CLOSE_QUOTE;

			if (first == false) {				
				if (buffer != NULL) {
					memcpy(buffer + offset, 
							&opv, sizeof(opv));
				}

				offset += sizeof(opv);
			}
		} else if (token->type == CSS_TOKEN_IDENT &&
				token->ilower == c->strings[NO_OPEN_QUOTE]) {
			opv = CONTENT_NO_OPEN_QUOTE;

			if (first == false) {
				if (buffer != NULL) {
					memcpy(buffer + offset, 
							&opv, sizeof(opv));
				}

				offset += sizeof(opv);
			}
		} else if (token->type == CSS_TOKEN_IDENT &&
				token->ilower == c->strings[NO_CLOSE_QUOTE]) {
			opv = CONTENT_NO_CLOSE_QUOTE;

			if (first == false) {
				if (buffer != NULL) {
					memcpy(buffer + offset, 
							&opv, sizeof(opv));
				}

				offset += sizeof(opv);
			}
		} else if (token->type == CSS_TOKEN_STRING) {
			opv = CONTENT_STRING;

			if (first == false) {
				if (buffer != NULL) {
					memcpy(buffer + offset, 
							&opv, sizeof(opv));
				}

				offset += sizeof(opv);
			}

			if (buffer != NULL) {
                                lwc_context_string_ref(c->sheet->dictionary, token->idata);
				memcpy(buffer + offset, &token->idata,
						sizeof(token->idata));
			}

			offset += sizeof(token->idata);
		} else if (token->type == CSS_TOKEN_URI) {
			opv = CONTENT_URI;

			if (first == false) {
				if (buffer != NULL) {
					memcpy(buffer + offset, 
							&opv, sizeof(opv));
				}

				offset += sizeof(opv);
			}

			if (buffer != NULL) {
                                lwc_context_string_ref(c->sheet->dictionary, token->idata);
				memcpy(buffer + offset, &token->idata,
						sizeof(token->idata));
			}

			offset += sizeof(token->idata);
		} else if (token->type == CSS_TOKEN_FUNCTION &&
				token->ilower == c->strings[ATTR]) {
			opv = CONTENT_ATTR;

			if (first == false) {
				if (buffer != NULL) {
					memcpy(buffer + offset, 
							&opv, sizeof(opv));
				}

				offset += sizeof(opv);
			}

			consumeWhitespace(vector, ctx);

			/* Expect IDENT */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || token->type != CSS_TOKEN_IDENT)
				return CSS_INVALID;

			if (buffer != NULL) {
                                lwc_context_string_ref(c->sheet->dictionary, token->idata);
				memcpy(buffer + offset, &token->idata, 
						sizeof(token->idata));
			}

			offset += sizeof(token->idata);

			consumeWhitespace(vector, ctx);

			/* Expect ')' */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || tokenIsChar(token, ')') == false)
				return CSS_INVALID;
		} else if (token->type == CSS_TOKEN_FUNCTION &&
				token->ilower == c->strings[COUNTER]) {
			lwc_string *name;

			opv = CONTENT_COUNTER;

			consumeWhitespace(vector, ctx);

			/* Expect IDENT */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || token->type != CSS_TOKEN_IDENT)
				return CSS_INVALID;

			name = token->idata;

			consumeWhitespace(vector, ctx);

			/* Possible ',' */
			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL || 
					(tokenIsChar(token, ',') == false &&
					tokenIsChar(token, ')') == false))
				return CSS_INVALID;

			if (tokenIsChar(token, ',')) {
				uint16_t v;

				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);

				/* Expect IDENT */
				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL || 
						token->type != CSS_TOKEN_IDENT)
				return CSS_INVALID;

				error = parse_list_style_type_value(c,
						token, &v);
				if (error != CSS_OK)
					return error;

				opv |= v << CONTENT_COUNTER_STYLE_SHIFT;

				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);
			} else {
				opv |= LIST_STYLE_TYPE_DECIMAL << 
						CONTENT_COUNTER_STYLE_SHIFT;
			}

			/* Expect ')' */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || tokenIsChar(token,	')') == false)
				return CSS_INVALID;

			if (first == false) {
				if (buffer != NULL) {
					memcpy(buffer + offset, 
							&opv, sizeof(opv));
				}

				offset += sizeof(opv);
			}

			if (buffer != NULL) {
                                lwc_context_string_ref(c->sheet->dictionary, name);
				memcpy(buffer + offset, &name, sizeof(name));
			}

			offset += sizeof(name);
		} else if (token->type == CSS_TOKEN_FUNCTION &&
				token->ilower == c->strings[COUNTERS]) {
			lwc_string *name;
			lwc_string *sep;

			opv = CONTENT_COUNTERS;

			consumeWhitespace(vector, ctx);

			/* Expect IDENT */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || token->type != CSS_TOKEN_IDENT)
				return CSS_INVALID;

			name = token->idata;

			consumeWhitespace(vector, ctx);

			/* Expect ',' */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || tokenIsChar(token, ',') == false)
				return CSS_INVALID;

			consumeWhitespace(vector, ctx);

			/* Expect STRING */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || token->type != CSS_TOKEN_STRING)
				return CSS_INVALID;

			sep = token->idata;

			consumeWhitespace(vector, ctx);

			/* Possible ',' */
			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL || 
					(tokenIsChar(token, ',') == false && 
					tokenIsChar(token, ')') == false))
				return CSS_INVALID;

			if (tokenIsChar(token, ',')) {
				uint16_t v;

				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);

				/* Expect IDENT */
				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL || 
						token->type != CSS_TOKEN_IDENT)
				return CSS_INVALID;

				error = parse_list_style_type_value(c,
						token, &v);
				if (error != CSS_OK)
					return error;

				opv |= v << CONTENT_COUNTERS_STYLE_SHIFT;

				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);
			} else {
				opv |= LIST_STYLE_TYPE_DECIMAL <<
						CONTENT_COUNTERS_STYLE_SHIFT;
			}

			/* Expect ')' */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || tokenIsChar(token, ')') == false)
				return CSS_INVALID;

			if (first == false) {
				if (buffer != NULL) {
					memcpy(buffer + offset, 
							&opv, sizeof(opv));
				}

				offset += sizeof(opv);
			}

			if (buffer != NULL) {
                                lwc_context_string_ref(c->sheet->dictionary, name);
				memcpy(buffer + offset, &name, sizeof(name));
			}

			offset += sizeof(name);

			if (buffer != NULL) {
                                lwc_context_string_ref(c->sheet->dictionary, sep);
				memcpy(buffer + offset, &sep, sizeof(sep));
			}

			offset += sizeof(sep);
		} else {
			return CSS_INVALID;
		}

		if (first && value != NULL) {
			*value = opv;
		}
		first = false;

		consumeWhitespace(vector, ctx);

		token = parserutils_vector_peek(vector, *ctx);
		if (token != NULL && tokenIsChar(token, '!'))
			break;

		token = parserutils_vector_iterate(vector, ctx);
	}

	/* Write list terminator */
	opv = CONTENT_NORMAL;

	if (buffer != NULL) {
		memcpy(buffer + offset, &opv, sizeof(opv));
	}

	offset += sizeof(opv);

	if (buflen != NULL) {
		*buflen = offset;
	}

	return CSS_OK;
}

