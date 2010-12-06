/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

css_error cascade_speak_punctuation( 
		uint32_t opv, css_style *style, css_select_state *state)
{
	uint16_t value = 0;

	UNUSED(style);

	if (isInherit(opv) == false) {
		switch (getValue(opv)) {
		case SPEAK_PUNCTUATION_CODE:
		case SPEAK_PUNCTUATION_NONE:
			/** \todo convert to public values */
			value = 0;
			break;
		}
	}

	if (outranks_existing(getOpcode(opv), isImportant(opv), state,
			isInherit(opv))) {
		/** \todo speak-punctuation */
	}

	return CSS_OK;
}

css_error set_speak_punctuation_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	UNUSED(hint);
	UNUSED(style);

	return CSS_OK;
}

css_error initial_speak_punctuation(css_select_state *state)
{
	UNUSED(state);

	return CSS_OK;
}

css_error compose_speak_punctuation(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	UNUSED(parent);
	UNUSED(child);
	UNUSED(result);

	return CSS_OK;
}

uint32_t destroy_speak_punctuation(void *bytecode)
{
	UNUSED(bytecode);
	
	return sizeof(uint32_t);
}