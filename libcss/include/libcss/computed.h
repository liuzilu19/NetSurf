/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef libcss_computed_h_
#define libcss_computed_h_

#include <libcss/errors.h>
#include <libcss/functypes.h>
#include <libcss/properties.h>
#include <libcss/types.h>

typedef struct css_computed_aural {
	/** \todo */
	uint8_t dummy;
} css_computed_aural;

typedef struct css_computed_page {
	/** \todo */
	uint8_t dummy;
} css_computed_page;

typedef struct css_computed_counter {
	const css_string *name;
	int32_t value;
} css_computed_counter;

typedef struct css_computed_clip_rect {
	css_fixed top;
	css_fixed right;
	css_fixed bottom;
	css_fixed left;

	css_unit tunit;
	css_unit runit;
	css_unit bunit;
	css_unit lunit;
} css_computed_clip_rect;

typedef struct css_computed_uncommon {
/*
 * border_spacing		  1 + 2(4)	  2(4)
 * clip				  2 + 4(4)	  4(4)
 * letter_spacing		  2 + 4		  4
 * outline_color		  2		  4
 * outline_width		  3 + 4		  4
 * word_spacing			  2 + 4		  4
 * 				---		---
 * 				 48 bits	 40 bytes
 *
 * Encode counter_increment and _reset as a NULL-terminated array of
 * name, value pairs.
 *
 * counter_increment		  1		  sizeof(ptr)
 * counter_reset		  1		  sizeof(ptr)
 * 				---		---
 * 				  2 bits	  2sizeof(ptr) bytes
 *
 * Encode quotes as a NULL-terminated array of string pointers
 *
 * quotes			  1		  sizeof(ptr)
 * 				---		---
 * 				  1 bit		  sizeof(ptr) bytes
 *
 * Encode cursor uri(s) as a NULL-terminated array of string pointers
 *
 * cursor			  5		   sizeof(ptr)
 * 				---		---
 * 				  5 bits	   sizeof(ptr) bytes
 *
 * content			?
 *
 * 				___		___
 * 				 56 bits	 40 + 4sizeof(ptr) bytes
 *
 * 				  7 bytes	 40 + 4sizeof(ptr) bytes
 * 				===================
 * 				 47 + 4sizeof(ptr) bytes
 *
 * Bit allocations:
 *
 *    76543210
 *  1 llllllcc	letter-spacing | outline-color
 *  2 ooooooob	outline-width  | border-spacing
 *  3 bbbbbbbb	border-spacing
 *  4 wwwwwwir	word-spacing   | counter-increment | counter-reset
 *  5 uuuuuqcc	cursor         | quotes            | clip
 *  6 cccccccc	clip
 *  7 cccccccc	clip
 */
	uint8_t bits[7];

	uint8_t unused[1];

	css_fixed border_spacing[2];

	css_fixed clip[4];

	css_fixed letter_spacing;

	css_color outline_color;
	css_fixed outline_width;

	css_fixed word_spacing;

	css_computed_counter **counter_increment;
	css_computed_counter **counter_reset;

	const css_string **quotes;

	const css_string **cursor;
} css_computed_uncommon;

struct css_computed_style {
/*
 * background_attachment	  2
 * background_repeat		  3
 * border_collapse		  2
 * border_top_style		  4
 * border_right_style		  4
 * border_bottom_style		  4
 * border_left_style		  4
 * caption_side			  2
 * clear			  3
 * direction			  2
 * display			  5
 * empty_cells			  2
 * float			  2
 * font_style			  2
 * font_variant			  2
 * font_weight			  4
 * list_style_position		  2
 * list_style_type		  4
 * overflow			  3
 * outline_style		  4
 * position			  3
 * table_layout			  2
 * text_align			  3
 * text_decoration		  5
 * text_transform		  3
 * unicode_bidi			  2
 * visibility			  2
 * white_space			  3
 *				---
 *				 83 bits
 *
 * Colours are 32bits of RRGGBBAA
 * Dimensions are encoded as a fixed point value + 4 bits of unit data
 *
 * background_color		  2		  4
 * background_image		  1		  sizeof(ptr)
 * background_position		  1 + 2(4)	  2(4)
 * border_top_color		  2		  4
 * border_right_color		  2		  4
 * border_bottom_color		  2		  4
 * border_left_color		  2		  4
 * border_top_width		  3 + 4		  4
 * border_right_width		  3 + 4		  4
 * border_bottom_width		  3 + 4		  4
 * border_left_width		  3 + 4		  4
 * top				  2 + 4		  4
 * right			  2 + 4		  4
 * bottom			  2 + 4		  4
 * left				  2 + 4		  4
 * color			  1		  4
 * font_size			  4 + 4		  4
 * height			  2 + 4		  4
 * line_height			  2 + 4		  4
 * list_style_image		  1		  sizeof(ptr)
 * margin_top			  2 + 4		  4
 * margin_right			  2 + 4		  4
 * margin_bottom		  2 + 4		  4
 * margin_left			  2 + 4		  4
 * max_height			  2 + 4		  4
 * max_width			  2 + 4		  4
 * min_height			  1 + 4		  4
 * min_width			  1 + 4		  4
 * padding_top			  1 + 4		  4
 * padding_right		  1 + 4		  4
 * padding_bottom		  1 + 4		  4
 * padding_left			  1 + 4		  4
 * text_indent			  1 + 4		  4
 * vertical_align		  4 + 4		  4
 * width			  2 + 4		  4
 * z_index			  2		  4
 * 				---		---
 *				181 bits	140 + 2sizeof(ptr) bytes
 *
 * Encode font family as a NULL-terminated array of string pointers
 *
 * font_family			  1		  sizeof(ptr)
 * 				---		---
 * 				  1 bit		  sizeof(ptr)
 *
 * 				___		___
 *				265 bits	140 + 3sizeof(ptr) bytes
 *
 *				 34 bytes	140 + 3sizeof(ptr) bytes
 *				===================
 *				174 + 3sizeof(ptr) bytes
 *
 * Bit allocations:
 *
 *    76543210
 *  1 vvvvvvvv	vertical-align
 *  2 ffffffff	font-size
 *  3 ttttttti	border-top-width    | background-image
 *  4 rrrrrrrc	border-right-width  | color
 *  5 bbbbbbbl	border-bottom-width | list-style-image
 *  6 lllllllf	border-left-width   | font-family
 *  7 ttttttcc	top                 | border-top-color
 *  8 rrrrrrcc	right               | border-right-color
 *  9 bbbbbbcc	bottom              | border-bottom-color
 * 10 llllllcc	left                | border-left-color
 * 11 hhhhhhbb	height              | background-color
 * 12 llllllzz	line-height         | z-index
 * 13 ttttttbb	margin-top          | background-attachment
 * 14 rrrrrrbb	margin-right        | border-collapse
 * 15 bbbbbbcc	margin-bottom       | caption-side
 * 16 lllllldd	margin-left         | direction
 * 17 mmmmmmee	max-height          | empty-cells
 * 18 mmmmmmff	max-width           | float
 * 19 wwwwwwff	width               | font-style
 * 20 mmmmmbbb	min-height          | background-repeat
 * 21 mmmmmccc	min-width           | clear
 * 22 tttttooo	padding-top         | overflow
 * 23 rrrrrppp	padding-right       | position
 * 24 bbbbbttt	padding-bottom      | text-align
 * 25 lllllttt	padding-left        | text-transform
 * 26 tttttwww	text-indent         | white-space
 * 27 bbbbbbbb	background-position
 * 28 bdddddff	background-position | display               | font-variant
 * 29 tttttll.	text-decoration     | list-style-position   | <unused>
 * 30 ttttrrrr	border-top-style    | border-right-style
 * 31 bbbbllll	border-bottom-style | border-left-style
 * 32 ffffllll	font-weight         | list-style-type
 * 33 oooottuu	outline-style       | table-layout          | unicode-bidi
 * 34 vv......	visibility          | <unused>
 */
	uint8_t bits[34];

	uint8_t unused[2];

	css_color background_color;
	const css_string *background_image;
	css_fixed background_position[2];

	css_color border_color[4];
	css_fixed border_width[4];

	css_fixed top;
	css_fixed right;
	css_fixed bottom;
	css_fixed left;

	css_color color;

	css_fixed font_size;

	css_fixed height;

	css_fixed line_height;

	const css_string *list_style_image;

	css_fixed margin[4];

	css_fixed max_height;
	css_fixed max_width;

	css_fixed min_height;
	css_fixed min_width;

	css_fixed padding[4];

	css_fixed text_indent;

	css_fixed vertical_align;

	css_fixed width;

	int32_t z_index;

	const css_string **font_family;

	css_computed_uncommon *uncommon;/**< Uncommon properties */
	css_computed_aural *aural;	/**< Aural properties */
	css_computed_page *page;	/**< Page properties */
};

css_error css_computed_style_create(css_alloc alloc, void *pw,
		css_computed_style **result);
css_error css_computed_style_destroy(css_computed_style *style);

css_error css_computed_style_compose(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result);

/******************************************************************************
 * Property accessors below here                                              *
 ******************************************************************************/

#define LETTER_SPACING_INDEX 0
#define LETTER_SPACING_SHIFT 2
#define LETTER_SPACING_MASK  0xfc
static inline uint8_t css_computed_letter_spacing(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[LETTER_SPACING_INDEX];
		bits &= LETTER_SPACING_MASK;
		bits >>= LETTER_SPACING_SHIFT;

		/* 6bits: uuuutt : unit | type */

		if ((bits & 3) == CSS_LETTER_SPACING_SET) {
			*length = style->uncommon->letter_spacing;
			*unit = bits >> 2;
		}

		return (bits & 3);
	}

	return CSS_LETTER_SPACING_NORMAL;
}
#undef LETTER_SPACING_MASK
#undef LETTER_SPACING_SHIFT
#undef LETTER_SPACING_INDEX

#define OUTLINE_COLOR_INDEX 0
#define OUTLINE_COLOR_SHIFT 0
#define OUTLINE_COLOR_MASK  0x3
static inline uint8_t css_computed_outline_color(
		const css_computed_style *style, css_color *color)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[OUTLINE_COLOR_INDEX];
		bits &= OUTLINE_COLOR_MASK;
		bits >>= OUTLINE_COLOR_SHIFT;

		/* 2bits: tt : type */

		if ((bits & 3) == CSS_OUTLINE_COLOR_COLOR) {
			*color = style->uncommon->outline_color;
		}

		return (bits & 3);
	}

	return CSS_OUTLINE_COLOR_INVERT;
}
#undef OUTLINE_COLOR_MASK
#undef OUTLINE_COLOR_SHIFT
#undef OUTLINE_COLOR_INDEX

#define OUTLINE_WIDTH_INDEX 1
#define OUTLINE_WIDTH_SHIFT 1
#define OUTLINE_WIDTH_MASK  0xfe
static inline uint8_t css_computed_outline_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[OUTLINE_WIDTH_INDEX];
		bits &= OUTLINE_WIDTH_MASK;
		bits >>= OUTLINE_WIDTH_SHIFT;

		/* 7bits: uuuuttt : unit | type */

		if ((bits & 7) == CSS_OUTLINE_WIDTH_WIDTH) {
			*length = style->uncommon->outline_width;
			*unit = bits >> 3;
		}

		return (bits & 7);
	}

	return CSS_OUTLINE_WIDTH_MEDIUM;
}
#undef OUTLINE_WIDTH_MASK
#undef OUTLINE_WIDTH_SHIFT
#undef OUTLINE_WIDTH_INDEX

#define BORDER_SPACING_INDEX 1
#define BORDER_SPACING_SHIFT 0
#define BORDER_SPACING_MASK  0x1
#define BORDER_SPACING_INDEX1 2
#define BORDER_SPACING_SHIFT1 0
#define BORDER_SPACING_MASK1 0xff
static inline uint8_t css_computed_border_spacing(
		const css_computed_style *style, 
		css_fixed *hlength, css_unit *hunit,
		css_fixed *vlength, css_unit *vunit)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[BORDER_SPACING_INDEX];
		bits &= BORDER_SPACING_MASK;
		bits >>= BORDER_SPACING_SHIFT;

		/* 1 bit: type */
		if (bits == CSS_BORDER_SPACING_SET) {
			uint8_t bits1 = 
				style->uncommon->bits[BORDER_SPACING_INDEX1];
			bits1 &= BORDER_SPACING_MASK1;
			bits1 >>= BORDER_SPACING_SHIFT1;

			/* 8bits: hhhhvvvv : hunit | vunit */

			*hlength = style->uncommon->border_spacing[0];
			*hunit = bits1 >> 4;

			*vlength = style->uncommon->border_spacing[1];
			*vunit = bits1 & 0xf;
		}

		return bits;
	} else {
		*hlength = *vlength = 0;
		*hunit = *vunit = CSS_UNIT_PX;
	}

	return CSS_BORDER_SPACING_SET;
}
#undef BORDER_SPACING_MASK1
#undef BORDER_SPACING_SHIFT1
#undef BORDER_SPACING_INDEX1
#undef BORDER_SPACING_MASK
#undef BORDER_SPACING_SHIFT
#undef BORDER_SPACING_INDEX

#define WORD_SPACING_INDEX 3
#define WORD_SPACING_SHIFT 2
#define WORD_SPACING_MASK  0xfc
static inline uint8_t css_computed_word_spacing(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[WORD_SPACING_INDEX];
		bits &= WORD_SPACING_MASK;
		bits >>= WORD_SPACING_SHIFT;

		/* 6bits: uuuutt : unit | type */

		if ((bits & 3) == CSS_WORD_SPACING_SET) {
			*length = style->uncommon->word_spacing;
			*unit = bits >> 2;
		}

		return (bits & 3);
	}

	return CSS_WORD_SPACING_NORMAL;
}
#undef WORD_SPACING_MASK
#undef WORD_SPACING_SHIFT
#undef WORD_SPACING_INDEX

#define COUNTER_INCREMENT_INDEX 3
#define COUNTER_INCREMENT_SHIFT 1
#define COUNTER_INCREMENT_MASK  0x2
static inline uint8_t css_computed_counter_increment(
		const css_computed_style *style, 
		css_computed_counter ***counters)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[COUNTER_INCREMENT_INDEX];
		bits &= COUNTER_INCREMENT_MASK;
		bits >>= COUNTER_INCREMENT_SHIFT;

		/* 1bit: type */
		*counters = style->uncommon->counter_increment;

		return bits;
	}

	return CSS_COUNTER_INCREMENT_NONE;
}
#undef COUNTER_INCREMENT_MASK
#undef COUNTER_INCREMENT_SHIFT
#undef COUNTER_INCREMENT_INDEX

#define COUNTER_RESET_INDEX 3
#define COUNTER_RESET_SHIFT 0
#define COUNTER_RESET_MASK  0x1
static inline uint8_t css_computed_counter_reset(
		const css_computed_style *style, 
		css_computed_counter ***counters)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[COUNTER_RESET_INDEX];
		bits &= COUNTER_RESET_MASK;
		bits >>= COUNTER_RESET_SHIFT;

		/* 1bit: type */
		*counters = style->uncommon->counter_reset;

		return bits;
	}

	return CSS_COUNTER_RESET_NONE;
}
#undef COUNTER_RESET_MASK
#undef COUNTER_RESET_SHIFT
#undef COUNTER_RESET_INDEX

#define CURSOR_INDEX 4
#define CURSOR_SHIFT 3
#define CURSOR_MASK  0xf8
static inline uint8_t css_computed_cursor(
		const css_computed_style *style, 
		const css_string ***urls)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[CURSOR_INDEX];
		bits &= CURSOR_MASK;
		bits >>= CURSOR_SHIFT;

		/* 5bits: type */
		*urls = style->uncommon->cursor;

		return bits;
	}

	return CSS_CURSOR_AUTO;
}
#undef CURSOR_MASK
#undef CURSOR_SHIFT
#undef CURSOR_INDEX

#define QUOTES_INDEX 4
#define QUOTES_SHIFT 2
#define QUOTES_MASK  0x4
static inline uint8_t css_computed_quotes(
		const css_computed_style *style, 
		const css_string ***quotes)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[QUOTES_INDEX];
		bits &= QUOTES_MASK;
		bits >>= QUOTES_SHIFT;

		/* 1bit: type */
		*quotes = style->uncommon->quotes;

		return bits;
	}

	return CSS_QUOTES_NONE;
}
#undef QUOTES_MASK
#undef QUOTES_SHIFT
#undef QUOTES_INDEX

#define CLIP_INDEX 4
#define CLIP_SHIFT 0
#define CLIP_MASK  0x3
#define CLIP_INDEX1 5
#define CLIP_SHIFT1 0
#define CLIP_MASK1 0xff
#define CLIP_INDEX2 6
#define CLIP_SHIFT2 0
#define CLIP_MASK2 0xff
static inline uint8_t css_computed_clip(
		const css_computed_style *style, 
		css_computed_clip_rect *rect)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[CLIP_INDEX];
		bits &= CLIP_MASK;
		bits >>= CLIP_SHIFT;

		/* 2bits: type */
		if (bits == CSS_CLIP_RECT) {
			uint8_t bits1 = style->uncommon->bits[CLIP_INDEX1];
			uint8_t bits2 = style->uncommon->bits[CLIP_INDEX2];

			/* 8bits: ttttrrrr : top | right */
			bits1 &= CLIP_MASK1;
			bits1 >>= CLIP_SHIFT1;

			/* 8bits: bbbbllll : bottom | left */
			bits2 &= CLIP_MASK2;
			bits2 >>= CLIP_SHIFT2;

			rect->top = style->uncommon->clip[0];
			rect->tunit = bits1 >> 4;

			rect->right = style->uncommon->clip[1];
			rect->runit = bits1 & 0xf;

			rect->bottom = style->uncommon->clip[2];
			rect->bunit = bits2 >> 4;

			rect->left = style->uncommon->clip[3];
			rect->lunit = bits2 & 0xf;
		}

		return bits;
	}

	return CSS_CLIP_AUTO;
}
#undef CLIP_MASK2
#undef CLIP_SHIFT2
#undef CLIP_INDEX2
#undef CLIP_MASK1
#undef CLIP_SHIFT1
#undef CLIP_INDEX1
#undef CLIP_MASK
#undef CLIP_SHIFT
#undef CLIP_INDEX

#define VERTICAL_ALIGN_INDEX 0
#define VERTICAL_ALIGN_SHIFT 0
#define VERTICAL_ALIGN_MASK  0xff
static inline uint8_t css_computed_vertical_align(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[VERTICAL_ALIGN_INDEX];
	bits &= VERTICAL_ALIGN_MASK;
	bits >>= VERTICAL_ALIGN_SHIFT;

	/* 8bits: uuuutttt : units | type */
	if ((bits & 0xf) == CSS_VERTICAL_ALIGN_SET) {
		*length = style->vertical_align;
		*unit = bits >> 4;
	}

	return (bits & 0xf);
}
#undef VERTICAL_ALIGN_MASK
#undef VERTICAL_ALIGN_SHIFT
#undef VERTICAL_ALIGN_INDEX

#define FONT_SIZE_INDEX 1
#define FONT_SIZE_SHIFT 0
#define FONT_SIZE_MASK  0xff
static inline uint8_t css_computed_font_size(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[FONT_SIZE_INDEX];
	bits &= FONT_SIZE_MASK;
	bits >>= FONT_SIZE_SHIFT;

	/* 8bits: uuuutttt : units | type */
	if ((bits & 0xf) == CSS_FONT_SIZE_DIMENSION) {
		*length = style->font_size;
		*unit = bits >> 4;
	}

	return (bits & 0xf);
}
#undef FONT_SIZE_MASK
#undef FONT_SIZE_SHIFT
#undef FONT_SIZE_INDEX

#define BORDER_TOP_WIDTH_INDEX 2
#define BORDER_TOP_WIDTH_SHIFT 1
#define BORDER_TOP_WIDTH_MASK  0xfe
static inline uint8_t css_computed_border_top_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BORDER_TOP_WIDTH_INDEX];
	bits &= BORDER_TOP_WIDTH_MASK;
	bits >>= BORDER_TOP_WIDTH_SHIFT;

	/* 7bits: uuuuttt : units | type */
	if ((bits & 0x7) == CSS_BORDER_WIDTH_WIDTH) {
		*length = style->border_width[0];
		*unit = bits >> 3;
	}

	return (bits & 0x7);
}
#undef BORDER_TOP_WIDTH_MASK
#undef BORDER_TOP_WIDTH_SHIFT
#undef BORDER_TOP_WIDTH_INDEX

#define BORDER_RIGHT_WIDTH_INDEX 3
#define BORDER_RIGHT_WIDTH_SHIFT 1
#define BORDER_RIGHT_WIDTH_MASK  0xfe
static inline uint8_t css_computed_border_right_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BORDER_RIGHT_WIDTH_INDEX];
	bits &= BORDER_RIGHT_WIDTH_MASK;
	bits >>= BORDER_RIGHT_WIDTH_SHIFT;

	/* 7bits: uuuuttt : units | type */
	if ((bits & 0x7) == CSS_BORDER_WIDTH_WIDTH) {
		*length = style->border_width[1];
		*unit = bits >> 3;
	}

	return (bits & 0x7);
}
#undef BORDER_RIGHT_WIDTH_MASK
#undef BORDER_RIGHT_WIDTH_SHIFT
#undef BORDER_RIGHT_WIDTH_INDEX

#define BORDER_BOTTOM_WIDTH_INDEX 4
#define BORDER_BOTTOM_WIDTH_SHIFT 1
#define BORDER_BOTTOM_WIDTH_MASK  0xfe
static inline uint8_t css_computed_border_bottom_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BORDER_BOTTOM_WIDTH_INDEX];
	bits &= BORDER_BOTTOM_WIDTH_MASK;
	bits >>= BORDER_BOTTOM_WIDTH_SHIFT;

	/* 7bits: uuuuttt : units | type */
	if ((bits & 0x7) == CSS_BORDER_WIDTH_WIDTH) {
		*length = style->border_width[2];
		*unit = bits >> 3;
	}

	return (bits & 0x7);
}
#undef BORDER_BOTTOM_WIDTH_MASK
#undef BORDER_BOTTOM_WIDTH_SHIFT
#undef BORDER_BOTTOM_WIDTH_INDEX

#define BORDER_LEFT_WIDTH_INDEX 5
#define BORDER_LEFT_WIDTH_SHIFT 1
#define BORDER_LEFT_WIDTH_MASK  0xfe
static inline uint8_t css_computed_border_left_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BORDER_LEFT_WIDTH_INDEX];
	bits &= BORDER_LEFT_WIDTH_MASK;
	bits >>= BORDER_LEFT_WIDTH_SHIFT;

	/* 7bits: uuuuttt : units | type */
	if ((bits & 0x7) == CSS_BORDER_WIDTH_WIDTH) {
		*length = style->border_width[3];
		*unit = bits >> 3;
	}

	return (bits & 0x7);
}
#undef BORDER_LEFT_WIDTH_MASK
#undef BORDER_LEFT_WIDTH_SHIFT
#undef BORDER_LEFT_WIDTH_INDEX

#define BACKGROUND_IMAGE_INDEX 2
#define BACKGROUND_IMAGE_SHIFT 0
#define BACKGROUND_IMAGE_MASK  0x1
static inline uint8_t css_computed_background_image(
		const css_computed_style *style, 
		const css_string **url)
{
	uint8_t bits = style->bits[BACKGROUND_IMAGE_INDEX];
	bits &= BACKGROUND_IMAGE_MASK;
	bits >>= BACKGROUND_IMAGE_SHIFT;

	/* 1bit: type */
	*url = style->background_image;

	return bits;
}
#undef BACKGROUND_IMAGE_MASK
#undef BACKGROUND_IMAGE_SHIFT
#undef BACKGROUND_IMAGE_INDEX

#define COLOR_INDEX 3
#define COLOR_SHIFT 0
#define COLOR_MASK  0x1
static inline uint8_t css_computed_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[COLOR_INDEX];
	bits &= COLOR_MASK;
	bits >>= COLOR_SHIFT;

	/* 1bit: type */
	*color = style->color;

	return bits;
}
#undef COLOR_MASK
#undef COLOR_SHIFT
#undef COLOR_INDEX

#define LIST_STYLE_IMAGE_INDEX 4
#define LIST_STYLE_IMAGE_SHIFT 0
#define LIST_STYLE_IMAGE_MASK  0x1
static inline uint8_t css_computed_list_style_image(
		const css_computed_style *style, 
		const css_string **url)
{
	uint8_t bits = style->bits[LIST_STYLE_IMAGE_INDEX];
	bits &= LIST_STYLE_IMAGE_MASK;
	bits >>= LIST_STYLE_IMAGE_SHIFT;

	/* 1bit: type */
	*url = style->list_style_image;

	return bits;
}
#undef LIST_STYLE_IMAGE_MASK
#undef LIST_STYLE_IMAGE_SHIFT
#undef LIST_STYLE_IMAGE_INDEX

#define FONT_FAMILY_INDEX 5
#define FONT_FAMILY_SHIFT 0
#define FONT_FAMILY_MASK  0x1
static inline uint8_t css_computed_font_family(
		const css_computed_style *style, 
		const css_string ***names)
{
	uint8_t bits = style->bits[FONT_FAMILY_INDEX];
	bits &= FONT_FAMILY_MASK;
	bits >>= FONT_FAMILY_SHIFT;

	/* 1bit: type */
	*names = style->font_family;

	return bits;
}
#undef FONT_FAMILY_MASK
#undef FONT_FAMILY_SHIFT
#undef FONT_FAMILY_INDEX

#define TOP_INDEX 6
#define TOP_SHIFT 2
#define TOP_MASK  0xfc
static inline uint8_t css_computed_top(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[TOP_INDEX];
	bits &= TOP_MASK;
	bits >>= TOP_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_TOP_SET) {
		*length = style->top;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef TOP_MASK
#undef TOP_SHIFT
#undef TOP_INDEX

#define RIGHT_INDEX 7
#define RIGHT_SHIFT 2
#define RIGHT_MASK  0xfc
static inline uint8_t css_computed_right(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[RIGHT_INDEX];
	bits &= RIGHT_MASK;
	bits >>= RIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_RIGHT_SET) {
		*length = style->right;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef RIGHT_MASK
#undef RIGHT_SHIFT
#undef RIGHT_INDEX

#define BOTTOM_INDEX 8
#define BOTTOM_SHIFT 2
#define BOTTOM_MASK  0xfc
static inline uint8_t css_computed_bottom(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BOTTOM_INDEX];
	bits &= BOTTOM_MASK;
	bits >>= BOTTOM_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_BOTTOM_SET) {
		*length = style->bottom;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef BOTTOM_MASK
#undef BOTTOM_SHIFT
#undef BOTTOM_INDEX

#define LEFT_INDEX 9
#define LEFT_SHIFT 2
#define LEFT_MASK  0xfc
static inline uint8_t css_computed_left(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[LEFT_INDEX];
	bits &= LEFT_MASK;
	bits >>= LEFT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_LEFT_SET) {
		*length = style->left;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef LEFT_MASK
#undef LEFT_SHIFT
#undef LEFT_INDEX

#define BORDER_TOP_COLOR_INDEX 6
#define BORDER_TOP_COLOR_SHIFT 0
#define BORDER_TOP_COLOR_MASK  0x3
static inline uint8_t css_computed_border_top_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BORDER_TOP_COLOR_INDEX];
	bits &= BORDER_TOP_COLOR_MASK;
	bits >>= BORDER_TOP_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->border_color[0];

	return bits;
}
#undef BORDER_TOP_COLOR_MASK
#undef BORDER_TOP_COLOR_SHIFT
#undef BORDER_TOP_COLOR_INDEX

#define BORDER_RIGHT_COLOR_INDEX 7
#define BORDER_RIGHT_COLOR_SHIFT 0
#define BORDER_RIGHT_COLOR_MASK  0x3
static inline uint8_t css_computed_border_right_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BORDER_RIGHT_COLOR_INDEX];
	bits &= BORDER_RIGHT_COLOR_MASK;
	bits >>= BORDER_RIGHT_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->border_color[1];

	return bits;
}
#undef BORDER_RIGHT_COLOR_MASK
#undef BORDER_RIGHT_COLOR_SHIFT
#undef BORDER_RIGHT_COLOR_INDEX

#define BORDER_BOTTOM_COLOR_INDEX 8
#define BORDER_BOTTOM_COLOR_SHIFT 0
#define BORDER_BOTTOM_COLOR_MASK  0x3
static inline uint8_t css_computed_border_bottom_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BORDER_BOTTOM_COLOR_INDEX];
	bits &= BORDER_BOTTOM_COLOR_MASK;
	bits >>= BORDER_BOTTOM_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->border_color[2];

	return bits;
}
#undef BORDER_BOTTOM_COLOR_MASK
#undef BORDER_BOTTOM_COLOR_SHIFT
#undef BORDER_BOTTOM_COLOR_INDEX

#define BORDER_LEFT_COLOR_INDEX 9
#define BORDER_LEFT_COLOR_SHIFT 0
#define BORDER_LEFT_COLOR_MASK  0x3
static inline uint8_t css_computed_border_left_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BORDER_LEFT_COLOR_INDEX];
	bits &= BORDER_LEFT_COLOR_MASK;
	bits >>= BORDER_LEFT_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->border_color[3];

	return bits;
}
#undef BORDER_LEFT_COLOR_MASK
#undef BORDER_LEFT_COLOR_SHIFT
#undef BORDER_LEFT_COLOR_INDEX

#define HEIGHT_INDEX 10
#define HEIGHT_SHIFT 2
#define HEIGHT_MASK  0xfc
static inline uint8_t css_computed_height(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[HEIGHT_INDEX];
	bits &= HEIGHT_MASK;
	bits >>= HEIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_HEIGHT_SET) {
		*length = style->height;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef HEIGHT_MASK
#undef HEIGHT_SHIFT
#undef HEIGHT_INDEX

#define LINE_HEIGHT_INDEX 11
#define LINE_HEIGHT_SHIFT 2
#define LINE_HEIGHT_MASK  0xfc
static inline uint8_t css_computed_line_height(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[LINE_HEIGHT_INDEX];
	bits &= LINE_HEIGHT_MASK;
	bits >>= LINE_HEIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_LINE_HEIGHT_NUMBER || 
			(bits & 0x3) == CSS_LINE_HEIGHT_DIMENSION) {
		*length = style->line_height;
	}

	if ((bits & 0x3) == CSS_LINE_HEIGHT_DIMENSION) {
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef LINE_HEIGHT_MASK
#undef LINE_HEIGHT_SHIFT
#undef LINE_HEIGHT_INDEX

#define BACKGROUND_COLOR_INDEX 10
#define BACKGROUND_COLOR_SHIFT 0
#define BACKGROUND_COLOR_MASK  0x3
static inline uint8_t css_computed_background_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BACKGROUND_COLOR_INDEX];
	bits &= BACKGROUND_COLOR_MASK;
	bits >>= BACKGROUND_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->background_color;

	return bits;
}
#undef BACKGROUND_COLOR_MASK
#undef BACKGROUND_COLOR_SHIFT
#undef BACKGROUND_COLOR_INDEX

#define Z_INDEX_INDEX 11
#define Z_INDEX_SHIFT 0
#define Z_INDEX_MASK  0x3
static inline uint8_t css_computed_z_index(
		const css_computed_style *style, 
		int32_t *z_index)
{
	uint8_t bits = style->bits[Z_INDEX_INDEX];
	bits &= Z_INDEX_MASK;
	bits >>= Z_INDEX_SHIFT;

	/* 2bits: type */
	*z_index = style->z_index;

	return bits;
}
#undef Z_INDEX_MASK
#undef Z_INDEX_SHIFT
#undef Z_INDEX_INDEX

#define MARGIN_TOP_INDEX 12
#define MARGIN_TOP_SHIFT 2
#define MARGIN_TOP_MASK  0xfc
static inline uint8_t css_computed_margin_top(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MARGIN_TOP_INDEX];
	bits &= MARGIN_TOP_MASK;
	bits >>= MARGIN_TOP_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MARGIN_SET) {
		*length = style->margin[0];
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MARGIN_TOP_MASK
#undef MARGIN_TOP_SHIFT
#undef MARGIN_TOP_INDEX

#define MARGIN_RIGHT_INDEX 13
#define MARGIN_RIGHT_SHIFT 2
#define MARGIN_RIGHT_MASK  0xfc
static inline uint8_t css_computed_margin_right(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MARGIN_RIGHT_INDEX];
	bits &= MARGIN_RIGHT_MASK;
	bits >>= MARGIN_RIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MARGIN_SET) {
		*length = style->margin[1];
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MARGIN_RIGHT_MASK
#undef MARGIN_RIGHT_SHIFT
#undef MARGIN_RIGHT_INDEX

#define MARGIN_BOTTOM_INDEX 14
#define MARGIN_BOTTOM_SHIFT 2
#define MARGIN_BOTTOM_MASK  0xfc
static inline uint8_t css_computed_margin_bottom(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MARGIN_BOTTOM_INDEX];
	bits &= MARGIN_BOTTOM_MASK;
	bits >>= MARGIN_BOTTOM_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MARGIN_SET) {
		*length = style->margin[2];
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MARGIN_BOTTOM_MASK
#undef MARGIN_BOTTOM_SHIFT
#undef MARGIN_BOTTOM_INDEX

#define MARGIN_LEFT_INDEX 15
#define MARGIN_LEFT_SHIFT 2
#define MARGIN_LEFT_MASK  0xfc
static inline uint8_t css_computed_margin_left(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MARGIN_LEFT_INDEX];
	bits &= MARGIN_LEFT_MASK;
	bits >>= MARGIN_LEFT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MARGIN_SET) {
		*length = style->margin[3];
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MARGIN_LEFT_MASK
#undef MARGIN_LEFT_SHIFT
#undef MARGIN_LEFT_INDEX

#define BACKGROUND_ATTACHMENT_INDEX 12
#define BACKGROUND_ATTACHMENT_SHIFT 0
#define BACKGROUND_ATTACHMENT_MASK  0x3
static inline uint8_t css_computed_background_attachment(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BACKGROUND_ATTACHMENT_INDEX];
	bits &= BACKGROUND_ATTACHMENT_MASK;
	bits >>= BACKGROUND_ATTACHMENT_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef BACKGROUND_ATTACHMENT_MASK
#undef BACKGROUND_ATTACHMENT_SHIFT
#undef BACKGROUND_ATTACHMENT_INDEX

#define BORDER_COLLAPSE_INDEX 13
#define BORDER_COLLAPSE_SHIFT 0
#define BORDER_COLLAPSE_MASK  0x3
static inline uint8_t css_computed_border_collapse(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BORDER_COLLAPSE_INDEX];
	bits &= BORDER_COLLAPSE_MASK;
	bits >>= BORDER_COLLAPSE_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef BORDER_COLLAPSE_MASK
#undef BORDER_COLLAPSE_SHIFT
#undef BORDER_COLLAPSE_INDEX

#define CAPTION_SIDE_INDEX 14
#define CAPTION_SIDE_SHIFT 0
#define CAPTION_SIDE_MASK  0x3
static inline uint8_t css_computed_caption_side(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[CAPTION_SIDE_INDEX];
	bits &= CAPTION_SIDE_MASK;
	bits >>= CAPTION_SIDE_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef CAPTION_SIDE_MASK
#undef CAPTION_SIDE_SHIFT
#undef CAPTION_SIDE_INDEX

#define DIRECTION_INDEX 15
#define DIRECTION_SHIFT 0
#define DIRECTION_MASK  0x3
static inline uint8_t css_computed_direction(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[DIRECTION_INDEX];
	bits &= DIRECTION_MASK;
	bits >>= DIRECTION_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef DIRECTION_MASK
#undef DIRECTION_SHIFT
#undef DIRECTION_INDEX

#define MAX_HEIGHT_INDEX 16
#define MAX_HEIGHT_SHIFT 2
#define MAX_HEIGHT_MASK  0xfc
static inline uint8_t css_computed_max_height(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MAX_HEIGHT_INDEX];
	bits &= MAX_HEIGHT_MASK;
	bits >>= MAX_HEIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MAX_HEIGHT_SET) {
		*length = style->max_height;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MAX_HEIGHT_MASK
#undef MAX_HEIGHT_SHIFT
#undef MAX_HEIGHT_INDEX

#define MAX_WIDTH_INDEX 17
#define MAX_WIDTH_SHIFT 2
#define MAX_WIDTH_MASK  0xfc
static inline uint8_t css_computed_max_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MAX_WIDTH_INDEX];
	bits &= MAX_WIDTH_MASK;
	bits >>= MAX_WIDTH_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MAX_WIDTH_SET) {
		*length = style->max_width;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MAX_WIDTH_MASK
#undef MAX_WIDTH_SHIFT
#undef MAX_WIDTH_INDEX

#define WIDTH_INDEX 18
#define WIDTH_SHIFT 2
#define WIDTH_MASK  0xfc
static inline uint8_t css_computed_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[WIDTH_INDEX];
	bits &= WIDTH_MASK;
	bits >>= WIDTH_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_WIDTH_SET) {
		*length = style->width;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef WIDTH_MASK
#undef WIDTH_SHIFT
#undef WIDTH_INDEX

#define EMPTY_CELLS_INDEX 16
#define EMPTY_CELLS_SHIFT 0
#define EMPTY_CELLS_MASK  0x3
static inline uint8_t css_computed_empty_cells(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[EMPTY_CELLS_INDEX];
	bits &= EMPTY_CELLS_MASK;
	bits >>= EMPTY_CELLS_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef EMPTY_CELLS_MASK
#undef EMPTY_CELLS_SHIFT
#undef EMPTY_CELLS_INDEX

#define FLOAT_INDEX 17
#define FLOAT_SHIFT 0
#define FLOAT_MASK  0x3
static inline uint8_t css_computed_float(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[FLOAT_INDEX];
	bits &= FLOAT_MASK;
	bits >>= FLOAT_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef FLOAT_MASK
#undef FLOAT_SHIFT
#undef FLOAT_INDEX

#define FONT_STYLE_INDEX 18
#define FONT_STYLE_SHIFT 0
#define FONT_STYLE_MASK  0x3
static inline uint8_t css_computed_font_style(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[FONT_STYLE_INDEX];
	bits &= FONT_STYLE_MASK;
	bits >>= FONT_STYLE_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef FONT_STYLE_MASK
#undef FONT_STYLE_SHIFT
#undef FONT_STYLE_INDEX

#define MIN_HEIGHT_INDEX 19
#define MIN_HEIGHT_SHIFT 3
#define MIN_HEIGHT_MASK  0xf8
static inline uint8_t css_computed_min_height(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MIN_HEIGHT_INDEX];
	bits &= MIN_HEIGHT_MASK;
	bits >>= MIN_HEIGHT_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_MIN_HEIGHT_SET) {
		*length = style->min_height;
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef MIN_HEIGHT_MASK
#undef MIN_HEIGHT_SHIFT
#undef MIN_HEIGHT_INDEX

#define MIN_WIDTH_INDEX 20
#define MIN_WIDTH_SHIFT 3
#define MIN_WIDTH_MASK  0xf8
static inline uint8_t css_computed_min_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MIN_WIDTH_INDEX];
	bits &= MIN_WIDTH_MASK;
	bits >>= MIN_WIDTH_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_MIN_WIDTH_SET) {
		*length = style->min_width;
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef MIN_WIDTH_MASK
#undef MIN_WIDTH_SHIFT
#undef MIN_WIDTH_INDEX

#define BACKGROUND_REPEAT_INDEX 19
#define BACKGROUND_REPEAT_SHIFT 0
#define BACKGROUND_REPEAT_MASK  0x7
static inline uint8_t css_computed_background_repeat(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BACKGROUND_REPEAT_INDEX];
	bits &= BACKGROUND_REPEAT_MASK;
	bits >>= BACKGROUND_REPEAT_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef BACKGROUND_REPEAT_MASK
#undef BACKGROUND_REPEAT_SHIFT
#undef BACKGROUND_REPEAT_INDEX

#define CLEAR_INDEX 20
#define CLEAR_SHIFT 0
#define CLEAR_MASK  0x7
static inline uint8_t css_computed_clear(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[CLEAR_INDEX];
	bits &= CLEAR_MASK;
	bits >>= CLEAR_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef CLEAR_MASK
#undef CLEAR_SHIFT
#undef CLEAR_INDEX

#define PADDING_TOP_INDEX 21
#define PADDING_TOP_SHIFT 3
#define PADDING_TOP_MASK  0xf8
static inline uint8_t css_computed_padding_top(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[PADDING_TOP_INDEX];
	bits &= PADDING_TOP_MASK;
	bits >>= PADDING_TOP_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_PADDING_SET) {
		*length = style->padding[0];
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef PADDING_TOP_MASK
#undef PADDING_TOP_SHIFT
#undef PADDING_TOP_INDEX

#define PADDING_RIGHT_INDEX 22
#define PADDING_RIGHT_SHIFT 3
#define PADDING_RIGHT_MASK  0xf8
static inline uint8_t css_computed_padding_right(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[PADDING_RIGHT_INDEX];
	bits &= PADDING_RIGHT_MASK;
	bits >>= PADDING_RIGHT_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_PADDING_SET) {
		*length = style->padding[1];
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef PADDING_RIGHT_MASK
#undef PADDING_RIGHT_SHIFT
#undef PADDING_RIGHT_INDEX

#define PADDING_BOTTOM_INDEX 23
#define PADDING_BOTTOM_SHIFT 3
#define PADDING_BOTTOM_MASK  0xf8
static inline uint8_t css_computed_padding_bottom(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[PADDING_BOTTOM_INDEX];
	bits &= PADDING_BOTTOM_MASK;
	bits >>= PADDING_BOTTOM_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_PADDING_SET) {
		*length = style->padding[2];
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef PADDING_BOTTOM_MASK
#undef PADDING_BOTTOM_SHIFT
#undef PADDING_BOTTOM_INDEX

#define PADDING_LEFT_INDEX 24
#define PADDING_LEFT_SHIFT 3
#define PADDING_LEFT_MASK  0xf8
static inline uint8_t css_computed_padding_left(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[PADDING_LEFT_INDEX];
	bits &= PADDING_LEFT_MASK;
	bits >>= PADDING_LEFT_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_PADDING_SET) {
		*length = style->padding[3];
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef PADDING_LEFT_MASK
#undef PADDING_LEFT_SHIFT
#undef PADDING_LEFT_INDEX

#define OVERFLOW_INDEX 21
#define OVERFLOW_SHIFT 0
#define OVERFLOW_MASK  0x7
static inline uint8_t css_computed_overflow(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[OVERFLOW_INDEX];
	bits &= OVERFLOW_MASK;
	bits >>= OVERFLOW_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef OVERFLOW_MASK
#undef OVERFLOW_SHIFT
#undef OVERFLOW_INDEX

#define POSITION_INDEX 22
#define POSITION_SHIFT 0
#define POSITION_MASK  0x7
static inline uint8_t css_computed_position(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[POSITION_INDEX];
	bits &= POSITION_MASK;
	bits >>= POSITION_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef POSITION_MASK
#undef POSITION_SHIFT
#undef POSITION_INDEX

#define TEXT_ALIGN_INDEX 23
#define TEXT_ALIGN_SHIFT 0
#define TEXT_ALIGN_MASK  0x7
static inline uint8_t css_computed_text_align(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[TEXT_ALIGN_INDEX];
	bits &= TEXT_ALIGN_MASK;
	bits >>= TEXT_ALIGN_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef TEXT_ALIGN_MASK
#undef TEXT_ALIGN_SHIFT
#undef TEXT_ALIGN_INDEX

#define TEXT_TRANSFORM_INDEX 24
#define TEXT_TRANSFORM_SHIFT 0
#define TEXT_TRANSFORM_MASK  0x7
static inline uint8_t css_computed_text_transform(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[TEXT_TRANSFORM_INDEX];
	bits &= TEXT_TRANSFORM_MASK;
	bits >>= TEXT_TRANSFORM_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef TEXT_TRANSFORM_MASK
#undef TEXT_TRANSFORM_SHIFT
#undef TEXT_TRANSFORM_INDEX

#define TEXT_INDENT_INDEX 25
#define TEXT_INDENT_SHIFT 3
#define TEXT_INDENT_MASK  0xf8
static inline uint8_t css_computed_text_indent(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[TEXT_INDENT_INDEX];
	bits &= TEXT_INDENT_MASK;
	bits >>= TEXT_INDENT_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_TEXT_INDENT_SET) {
		*length = style->text_indent;
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef TEXT_INDENT_MASK
#undef TEXT_INDENT_SHIFT
#undef TEXT_INDENT_INDEX

#define WHITE_SPACE_INDEX 25
#define WHITE_SPACE_SHIFT 0
#define WHITE_SPACE_MASK  0x7
static inline uint8_t css_computed_white_space(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[WHITE_SPACE_INDEX];
	bits &= WHITE_SPACE_MASK;
	bits >>= WHITE_SPACE_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef WHITE_SPACE_MASK
#undef WHITE_SPACE_SHIFT
#undef WHITE_SPACE_INDEX

#define BACKGROUND_POSITION_INDEX 27
#define BACKGROUND_POSITION_SHIFT 7
#define BACKGROUND_POSITION_MASK  0x80
#define BACKGROUND_POSITION_INDEX1 26
#define BACKGROUND_POSITION_SHIFT1 0
#define BACKGROUND_POSITION_MASK1 0xff
static inline uint8_t css_computed_background_position(
		const css_computed_style *style, 
		css_fixed *hlength, css_unit *hunit,
		css_fixed *vlength, css_unit *vunit)
{
	uint8_t bits = style->bits[BACKGROUND_POSITION_INDEX];
	bits &= BACKGROUND_POSITION_MASK;
	bits >>= BACKGROUND_POSITION_SHIFT;

	/* 1bit: type */
	if (bits == CSS_BACKGROUND_POSITION_SET) {
		uint8_t bits1 = style->bits[BACKGROUND_POSITION_INDEX1];
		bits1 &= BACKGROUND_POSITION_MASK1;
		bits1 >>= BACKGROUND_POSITION_SHIFT1;

		/* 8bits: hhhhvvvv : hunit | vunit */
		*hlength = style->background_position[0];
		*hunit = bits1 >> 4;

		*vlength = style->background_position[1];
		*hunit = bits1 & 0xf;
	}

	return bits;
}
#undef BACKGROUND_POSITION_MASK1
#undef BACKGROUND_POSITION_SHIFT1
#undef BACKGROUND_POSITION_INDEX1
#undef BACKGROUND_POSITION_MASK
#undef BACKGROUND_POSITION_SHIFT
#undef BACKGROUND_POSITION_INDEX


#endif
