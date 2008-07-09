/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 Andrew Sidwell <takkaria@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"
#include "utils/utils.h"


/**
 * Handle token in "after body" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
bool handle_after_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	bool reprocess = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
	{
		/* mostly cribbed from process_characters_expect_whitespace */

		const uint8_t *data = treebuilder->input_buffer +
				token->data.character.data.off;

		size_t len = token->data.character.len;
		size_t c;

		/** \todo utf-16 */

		/* Scan for whitespace */
		for (c = 0; c < len; c++) {
			if (data[c] != 0x09 && data[c] != 0x0A &&
					data[c] != 0x0C && data[c] != 0x20)
				break;
		}

		/* Non-whitespace characters in token, so handle as in body */
		if (c > 0) {
			hubbub_token temp = *token;
			temp.data.character.len = c;

			handle_in_body(treebuilder, &temp);
		}

		/* Anything else, switch to in body */
		if (c != len) {
			/* Update token data to strip leading whitespace */
			((hubbub_token *) token)->data.character.data.off += c;
			((hubbub_token *) token)->data.character.len -= c;

			treebuilder->context.mode = IN_BODY;
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_COMMENT:
		process_comment_append(treebuilder, token,
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);
		break;
	case HUBBUB_TOKEN_DOCTYPE:
		/** \todo parse error */
		break;
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HTML) {
			/* Process as if "in body" */
			process_tag_in_body(treebuilder, token);
		} else {
			/** \todo parse error */
			treebuilder->context.mode = IN_BODY;
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HTML) {
			/** \todo fragment case */
			/** \todo parse error */
			treebuilder->context.mode = AFTER_AFTER_BODY;
		} else {
			/** \todo parse error */
			treebuilder->context.mode = IN_BODY;
			reprocess = true;
		}
	}
		break;
	case HUBBUB_TOKEN_EOF:
		break;
	}

	return reprocess;
}

