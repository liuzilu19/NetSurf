/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

/** \file CSS lexer
 * 
 * See docs/Tokens for the production rules used by this lexer.
 *
 * See docs/Lexer for the inferred first characters for each token.
 *
 * See also CSS3 Syntax module and CSS2.1 $4.1.1 + errata
 *
 * The lexer assumes that all invalid Unicode codepoints have been converted
 * to U+FFFD by the input stream.
 *
 * The lexer comprises a state machine, the top-level of which is derived from
 * the First sets in docs/Lexer. Each top-level state may contain a number of
 * sub states. These enable restarting of the parser.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include <parserutils/charset/utf8.h>
#include <parserutils/input/inputstream.h>
#include <parserutils/utils/buffer.h>

#include <libcss/errors.h>

#include "lex/lex.h"
#include "utils/parserutilserror.h"

/** \todo Optimisation -- we're currently revisiting a bunch of input 
 *	  characters (Currently, we're calling parserutils_inputstream_peek about
 *	  1.5x the number of characters in the input stream). Ideally, 
 *	  we'll visit each character in the input exactly once. In reality, 
 *	  the upper bound is twice, due to the need, in some cases, to read 
 *	  one character beyond the end of a token's input to detect the end 
 *	  of the token. Resumability adds a little overhead here, unless 
 *	  we're somewhat more clever when it comes to having support for 
 *	  restarting mid-escape sequence. Currently, we rewind back to the 
 *	  start of the sequence and process the whole thing again.
 */

enum {
	sSTART		=  0,
	sATKEYWORD	=  1,
	sSTRING		=  2,
	sHASH		=  3,
	sNUMBER		=  4, 
	sCDO		=  5,
	sCDC		=  6,
	sS		=  7,
	sCOMMENT	=  8,
	sMATCH		=  9, 
	sURI		= 10,
	sIDENT		= 11,
	sESCAPEDIDENT	= 12,
	sURL		= 13,
	sUCR		= 14 
};

/**
 * CSS lexer object
 */
struct css_lexer
{
	parserutils_inputstream *input;	/**< Inputstream containing CSS */

	size_t bytesReadForToken;	/**< Total bytes read from the 
					 * inputstream for the current token */

	css_token token;		/**< The current token */

	bool escapeSeen;		/**< Whether an escape sequence has 
					 * been seen while processing the input
					 * for the current token */
	parserutils_buffer *unescapedTokenData;	/**< Buffer containing 
					 	 * unescaped token data 
						 * (used iff escapeSeen == true)
						 */

	uint32_t state    : 4,		/**< Current state */
		 substate : 4;		/**< Current substate */

	struct {
		uint8_t first;		/**< First character read for token */
		size_t origBytes;	/**< Storage of current number of 
					 * bytes read, for rewinding */
		bool lastWasStar;	/**< Whether the previous character 
					 * was an asterisk */
		bool lastWasCR;		/**< Whether the previous character
					 * was CR */
		size_t bytesForURL;	/**< Input bytes read for "url(", for 
					 * rewinding */
		size_t dataLenForURL;	/**< Output length for "url(", for
					 * rewinding */
		int hexCount;		/**< Counter for reading hex digits */
	} context;			/**< Context for the current state */

	bool emit_comments;		/**< Whether to emit comment tokens */

	uint32_t currentCol;		/**< Current column in source */
	uint32_t currentLine;		/**< Current line in source */

	css_alloc alloc;		/**< Memory (de)allocation function */
	void *pw;			/**< Pointer to client-specific data */
};

#define APPEND(lexer, data, len)					\
do {									\
	css_error error;						\
	error = appendToTokenData((lexer), 				\
			(const uint8_t*) (data), (len));		\
	if (error != CSS_OK)						\
		return error;						\
	(lexer)->bytesReadForToken += (len);				\
	(lexer)->currentCol += (len);					\
} while(0)								\

static inline css_error appendToTokenData(css_lexer *lexer, 
		const uint8_t *data, size_t len);
static inline css_error emitToken(css_lexer *lexer, css_token_type type,
		const css_token **token);

static inline css_error AtKeyword(css_lexer *lexer, const css_token **token);
static inline css_error CDCOrIdentOrFunction(css_lexer *lexer,
		const css_token **token);
static inline css_error CDO(css_lexer *lexer, const css_token **token);
static inline css_error Comment(css_lexer *lexer, const css_token **token);
static inline css_error EscapedIdentOrFunction(css_lexer *lexer,
		const css_token **token);
static inline css_error Hash(css_lexer *lexer, const css_token **token);
static inline css_error IdentOrFunction(css_lexer *lexer,
		const css_token **token);
static inline css_error Match(css_lexer *lexer, const css_token **token);
static inline css_error NumberOrPercentageOrDimension(css_lexer *lexer,
		const css_token **token);
static inline css_error S(css_lexer *lexer, const css_token **token);
static inline css_error Start(css_lexer *lexer, const css_token **token);
static inline css_error String(css_lexer *lexer, const css_token **token);
static inline css_error URIOrUnicodeRangeOrIdentOrFunction(
		css_lexer *lexer, const css_token **token);
static inline css_error URI(css_lexer *lexer, const css_token **token);
static inline css_error UnicodeRange(css_lexer *lexer, const css_token **token);

static inline css_error consumeDigits(css_lexer *lexer);
static inline css_error consumeEscape(css_lexer *lexer, bool nl);
static inline css_error consumeNMChars(css_lexer *lexer);
static inline css_error consumeString(css_lexer *lexer);
static inline css_error consumeStringChars(css_lexer *lexer);
static inline css_error consumeUnicode(css_lexer *lexer, uint32_t ucs);
static inline css_error consumeURLChars(css_lexer *lexer);
static inline css_error consumeWChars(css_lexer *lexer);

static inline uint32_t charToHex(uint8_t c);
static inline bool startNMChar(uint8_t c);
static inline bool startNMStart(uint8_t c);
static inline bool startStringChar(uint8_t c);
static inline bool startURLChar(uint8_t c);
static inline bool isDigit(uint8_t c);
static inline bool isHex(uint8_t c);
static inline bool isSpace(uint8_t c);

/**
 * Create a lexer instance
 *
 * \param input  The inputstream to read from
 * \param alloc  Memory (de)allocation function
 * \param pw     Pointer to client-specific private data
 * \return Pointer to instance, or NULL on memory exhaustion
 */
css_lexer *css_lexer_create(parserutils_inputstream *input, 
		css_alloc alloc, void *pw)
{
	css_lexer *lex;

	if (input == NULL || alloc == NULL)
		return NULL;

	lex = alloc(NULL, sizeof(css_lexer), pw);
	if (lex == NULL)
		return NULL;

	lex->input = input;
	lex->bytesReadForToken = 0;
	lex->token.type = CSS_TOKEN_EOF;
	lex->token.data.ptr = NULL;
	lex->token.data.len = 0;
	lex->escapeSeen = false;
	lex->unescapedTokenData = NULL;
	lex->state = sSTART;
	lex->substate = 0;
	lex->emit_comments = false;
	lex->currentCol = 1;
	lex->currentLine = 1;
	lex->alloc = alloc;
	lex->pw = pw;

	return lex;
}

/**
 * Destroy a lexer instance
 *
 * \param lexer  The instance to destroy
 */
void css_lexer_destroy(css_lexer *lexer)
{
	if (lexer == NULL)
		return;

	if (lexer->unescapedTokenData != NULL)
		parserutils_buffer_destroy(lexer->unescapedTokenData);

	lexer->alloc(lexer, 0, lexer->pw);
}

/**
 * Configure a lexer instance
 *
 * \param lexer   The lexer to configure
 * \param type    The option type to modify
 * \param params  Option-specific parameters
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css_lexer_setopt(css_lexer *lexer, css_lexer_opttype type,
		css_lexer_optparams *params)
{
	if (lexer == NULL || params == NULL)
		return CSS_BADPARM;

	switch (type) {
	case CSS_LEXER_EMIT_COMMENTS:
		lexer->emit_comments = params->emit_comments;
		break;
	default:
		return CSS_BADPARM;
	}

	return CSS_OK;
}

/**
 * Retrieve a token from a lexer
 *
 * \param lexer  The lexer instance to read from
 * \param token  Pointer to location to receive pointer to token
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css_lexer_get_token(css_lexer *lexer, const css_token **token)
{
	css_error error;

	if (lexer == NULL || token == NULL)
		return CSS_BADPARM;

	switch (lexer->state)
	{
	case sSTART:
	start:
		return Start(lexer, token);
	case sATKEYWORD:
		return AtKeyword(lexer, token);
	case sSTRING:
		return String(lexer, token);
	case sHASH:
		return Hash(lexer, token);
	case sNUMBER:
		return NumberOrPercentageOrDimension(lexer, token);
	case sCDO:
		return CDO(lexer, token);
	case sCDC:
		return CDCOrIdentOrFunction(lexer, token);
	case sS:
		return S(lexer, token);
	case sCOMMENT:
		error = Comment(lexer, token);
		if (!lexer->emit_comments && error == CSS_OK)
			goto start;
		return error;
	case sMATCH:
		return Match(lexer, token);
	case sURI:
		return URI(lexer, token);
	case sIDENT:
		return IdentOrFunction(lexer, token);
	case sESCAPEDIDENT:
		return EscapedIdentOrFunction(lexer, token);
	case sURL:
		return URI(lexer, token);
	case sUCR:
		return UnicodeRange(lexer, token);
	}

	/* Should never be reached */
	assert(0);

	return CSS_OK;
}

/******************************************************************************
 * Utility routines                                                           *
 ******************************************************************************/

/**
 * Append some data to the current token
 *
 * \param lexer  The lexer instance
 * \param data   Pointer to data to append
 * \param len    Length, in bytes, of data
 * \return CSS_OK on success, appropriate error otherwise
 *
 * This should not be called directly without good reason. Use the APPEND()
 * macro instead. 
 */
css_error appendToTokenData(css_lexer *lexer, const uint8_t *data, size_t len)
{
	css_token *token = &lexer->token;

	if (lexer->escapeSeen) {
		css_error error = css_error_from_parserutils_error(
				parserutils_buffer_append(
					lexer->unescapedTokenData, data, len));
		if (error != CSS_OK)
			return error;
	}

	token->data.len += len;

	return CSS_OK;
}

/**
 * Prepare a token for consumption and emit it to the client
 *
 * \param lexer  The lexer instance
 * \param type   The type of token to emit
 * \param token  Pointer to location to receive pointer to token
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error emitToken(css_lexer *lexer, css_token_type type,
		const css_token **token)
{
	css_token *t = &lexer->token;

	t->type = type;

	/* Calculate token data start pointer. We have to do this here as 
	 * the inputstream's buffer may have moved under us. */
	if (lexer->escapeSeen) {
		t->data.ptr = lexer->unescapedTokenData->data;
	} else {
		size_t clen;
		uintptr_t ptr = parserutils_inputstream_peek(
				lexer->input, 0, &clen);

		assert(type == CSS_TOKEN_EOF || 
				(ptr != PARSERUTILS_INPUTSTREAM_EOF && 
				ptr != PARSERUTILS_INPUTSTREAM_OOD));

		t->data.ptr = (type == CSS_TOKEN_EOF) ? NULL : (uint8_t *) ptr;
	}

	switch (type) {
	case CSS_TOKEN_ATKEYWORD:
		/* Strip the '@' from the front */
		t->data.ptr += 1;
		t->data.len -= 1;
		break;
	case CSS_TOKEN_STRING:
		/* Strip the leading quote */
		t->data.ptr += 1;
		t->data.len -= 1;

		/* Strip the trailing quote */
		t->data.len -= 1;
		break;
	case CSS_TOKEN_HASH:
		/* Strip the '#' from the front */
		t->data.ptr += 1;
		t->data.len -= 1;
		break;
	case CSS_TOKEN_PERCENTAGE:
		/* Strip the '%' from the end */
		t->data.len -= 1;
		break;
	case CSS_TOKEN_DIMENSION:
		/** \todo Do we want to separate the value from the units? */
		break;
	case CSS_TOKEN_URI:
		/* Strip the "url(" from the start */
		t->data.ptr += sizeof("url(") - 1;
		t->data.len -= sizeof("url(") - 1;

		/* Strip any leading whitespace */
		while (isSpace(t->data.ptr[0])) {
			t->data.ptr++;
			t->data.len--;
		}

		/* Strip any leading quote */
		if (t->data.ptr[0] == '"' || t->data.ptr[0] == '\'') {
			t->data.ptr += 1;
			t->data.len -= 1;
		}

		/* Strip the trailing ')' */
		t->data.len -= 1;

		/* Strip any trailing whitespace */
		while (isSpace(t->data.ptr[t->data.len - 1])) {
			t->data.len--;
		}

		/* Strip any trailing quote */
		if (t->data.ptr[t->data.len - 1] == '"' || 
				t->data.ptr[t->data.len - 1] == '\'') {
			t->data.len -= 1;
		}
		break;
	case CSS_TOKEN_UNICODE_RANGE:
		/* Remove "U+" from the start */
		t->data.ptr += sizeof("U+") - 1;
		t->data.len -= sizeof("U+") - 1;
		break;
	case CSS_TOKEN_COMMENT:
		/* Strip the leading '/' and '*' */
		t->data.ptr += sizeof("/*") - 1;
		t->data.len -= sizeof("/*") - 1;

		/* Strip the trailing '*' and '/' */
		t->data.len -= sizeof("*/") - 1;
		break;
	case CSS_TOKEN_FUNCTION:
		/* Strip the trailing '(' */
		t->data.len -= 1;
		break;
	default:
		break;
	}

	*token = t;

	/* Reset the lexer's state */
	lexer->state = sSTART;
	lexer->substate = 0;

	return CSS_OK;
}

/******************************************************************************
 * State machine components                                                   *
 ******************************************************************************/

css_error AtKeyword(css_lexer *lexer, const css_token **token)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	enum { Initial = 0, Escape = 1, NMChar = 2 };

	/* ATKEYWORD = '@' ident 
	 * 
	 * The '@' has been consumed.
	 */

	switch (lexer->substate) {
	case Initial:
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		c = *((uint8_t *) cptr);

		if (!startNMStart(c))
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		if (c != '\\') {
			APPEND(lexer, cptr, clen);
		} else {
			lexer->bytesReadForToken += clen;
			goto escape;
		}

		/* Fall through */
	case NMChar:
	nmchar:
		lexer->substate = NMChar;
		error = consumeNMChars(lexer);
		if (error != CSS_OK)
			return error;
		break;

	case Escape:
	escape:
		lexer->substate = Escape;
		error = consumeEscape(lexer, false);
		if (error != CSS_OK) {
			if (error == CSS_EOF || error == CSS_INVALID) {
				/* Rewind the '\\' */
				lexer->bytesReadForToken -= 1;

				return emitToken(lexer, CSS_TOKEN_CHAR, token);
			}

			return error;
		}

		goto nmchar;
	}

	return emitToken(lexer, CSS_TOKEN_ATKEYWORD, token);
}

css_error CDCOrIdentOrFunction(css_lexer *lexer, const css_token **token)
{
	css_token *t = &lexer->token;
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	enum { Initial = 0, Escape = 1, Gt = 2 };

	/* CDC = "-->"
	 * IDENT = [-]? nmstart nmchar*
	 * FUNCTION = [-]? nmstart nmchar* '('
	 *
	 * The first dash has been consumed. Thus, we must consume the next 
	 * character in the stream. If it's a dash, then we're dealing with 
	 * CDC. Otherwise, we're dealing with IDENT/FUNCTION.
	 */

	switch (lexer->substate) {
	case Initial:
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/* We can only match char with what we've read so far */
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *((uint8_t *) cptr);

		if (c != '-' && !startNMStart(c)) {
			/* Can only be CHAR */
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}


		if (c != '\\') {
			APPEND(lexer, cptr, clen);
		}

		if (c != '-') {
			if (c == '\\') {
				lexer->bytesReadForToken += clen;
				goto escape;
			}

			lexer->state = sIDENT;
			lexer->substate = 0;
			return IdentOrFunction(lexer, token);
		}

		/* Fall through */
	case Gt:
		lexer->substate = Gt;

		/* Ok, so we're dealing with CDC. Expect a '>' */
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/* CHAR is the only match here */
			/* Remove the '-' we read above */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *((uint8_t *) cptr);

		if (c == '>') {
			APPEND(lexer, cptr, clen);

			t->type = CSS_TOKEN_CDC;
		} else {
			/* Remove the '-' we read above */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			t->type = CSS_TOKEN_CHAR;
		}
		break;

	case Escape:
	escape:
		lexer->substate = Escape;
		error = consumeEscape(lexer, false);
		if (error != CSS_OK) {
			if (error == CSS_EOF || error == CSS_INVALID) {
				/* Rewind the '\\' */
				lexer->bytesReadForToken -= 1;

				return emitToken(lexer, CSS_TOKEN_CHAR, token);
			}

			return error;
		}

		lexer->state = sIDENT;
		lexer->substate = 0;
		return IdentOrFunction(lexer, token);
	}

	return emitToken(lexer, t->type, token);
}

css_error CDO(css_lexer *lexer, const css_token **token)
{
	css_token *t = &lexer->token;
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	enum { Initial = 0, Dash1 = 1, Dash2 = 2 };

	/* CDO = "<!--"
	 * 
	 * The '<' has been consumed
	 */

	switch (lexer->substate) {
	case Initial:
		/* Expect '!' */
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/* CHAR is the only match here */
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *((uint8_t *) cptr);

		if (c == '!') {
			APPEND(lexer, cptr, clen);
		} else {
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		/* Fall Through */
	case Dash1:
		lexer->substate = Dash1;

		/* Expect '-' */
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/* CHAR is the only match here */
			/* Remove the '!' we read above */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *((uint8_t *) cptr);

		if (c == '-') {
			APPEND(lexer, cptr, clen);
		} else {
			/* Remove the '!' we read above */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		/* Fall through */
	case Dash2:
		lexer->substate = Dash2;

		/* Expect '-' */
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/* CHAR is the only match here */
			/* Remove the '-' and the '!' we read above */
			lexer->bytesReadForToken -= 2;
			t->data.len -= 2;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		c = *((uint8_t *) cptr);

		if (c == '-') {
			APPEND(lexer, cptr, clen);
		} else {
			/* Remove the '-' and the '!' we read above */
			lexer->bytesReadForToken -= 2;
			t->data.len -= 2;
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}
	}

	return emitToken(lexer, CSS_TOKEN_CDO, token);
}

css_error Comment(css_lexer *lexer, const css_token **token)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	enum { Initial = 0, InComment = 1 };

	/* COMMENT = '/' '*' [^*]* '*'+ ([^/] [^*]* '*'+)* '/'
	 *
	 * The '/' has been consumed.
	 */
	switch (lexer->substate) {
	case Initial:
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		c = *((uint8_t *) cptr);

		if (c != '*')
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		APPEND(lexer, cptr, clen);
	
		/* Fall through */
	case InComment:
		lexer->substate = InComment;

		while (1) {
			cptr = parserutils_inputstream_peek(lexer->input,
					lexer->bytesReadForToken, &clen);
			if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
				return CSS_NEEDDATA;

			if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
				/* As per unterminated strings, 
				 * we ignore unterminated comments. */
				return emitToken(lexer, CSS_TOKEN_EOF, token);
			}

			c = *((uint8_t *) cptr);

			APPEND(lexer, cptr, clen);

			if (lexer->context.lastWasStar && c == '/')
				break;

			lexer->context.lastWasStar = (c == '*');

			if (c == '\n' || c == '\f') {
				lexer->currentCol = 1;
				lexer->currentLine++;
			}

			if (lexer->context.lastWasCR && c != '\n') {
				lexer->currentCol = 1;
				lexer->currentLine++;
			}
			lexer->context.lastWasCR = (c == '\r');
		}
	}

	return emitToken(lexer, CSS_TOKEN_COMMENT, token);
}

css_error EscapedIdentOrFunction(css_lexer *lexer, const css_token **token)
{
	css_error error;

	/* IDENT = ident = [-]? nmstart nmchar*
	 * FUNCTION = ident '(' = [-]? nmstart nmchar* '('
	 *
	 * In this case, nmstart is an escape sequence and no '-' is present.
	 *
	 * The '\\' has been consumed.
	 */

	error = consumeEscape(lexer, false);
	if (error != CSS_OK) {
		if (error == CSS_EOF || error == CSS_INVALID) {
			/* The '\\' is a token of its own */
			return emitToken(lexer, CSS_TOKEN_CHAR, token);
		}

		return error;
	}

	lexer->state = sIDENT;
	lexer->substate = 0;
	return IdentOrFunction(lexer, token);
}

css_error Hash(css_lexer *lexer, const css_token **token)
{	
	css_error error;
	
	/* HASH = '#' name  = '#' nmchar+ 
	 *
	 * The '#' has been consumed.
	 */

	error = consumeNMChars(lexer);
	if (error != CSS_OK)
		return error;

	/* Require at least one NMChar otherwise, we're just a raw '#' */
	if (lexer->bytesReadForToken - lexer->context.origBytes > 0)
		return emitToken(lexer, CSS_TOKEN_HASH, token);

	return emitToken(lexer, CSS_TOKEN_CHAR, token);
}

css_error IdentOrFunction(css_lexer *lexer, const css_token **token)
{
	css_token *t = &lexer->token;
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	enum { Initial = 0, Bracket = 1 };

	/* IDENT = ident = [-]? nmstart nmchar*
	 * FUNCTION = ident '(' = [-]? nmstart nmchar* '('
	 *
	 * The optional dash and nmstart are already consumed
	 */

	switch (lexer->substate) {
	case Initial:
		/* Consume all subsequent nmchars (if any exist) */
		error = consumeNMChars(lexer);
		if (error != CSS_OK)
			return error;

		/* Fall through */
	case Bracket:
		lexer->substate = Bracket;

		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/* IDENT, rather than CHAR */
			return emitToken(lexer, CSS_TOKEN_IDENT, token);
		}

		c = *((uint8_t *) cptr);

		if (c == '(') {
			APPEND(lexer, cptr, clen);

			t->type = CSS_TOKEN_FUNCTION;
		} else {
			t->type = CSS_TOKEN_IDENT;
		}
	}

	return emitToken(lexer, t->type, token);
}

css_error Match(css_lexer *lexer, const css_token **token)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_token_type type = CSS_TOKEN_EOF; /* GCC's braindead */

	/* INCLUDES       = "~="
	 * DASHMATCH      = "|="
	 * PREFIXMATCH    = "^="
	 * SUFFIXMATCH    = "$="
	 * SUBSTRINGMATCH = "*="
	 *
	 * The first character has been consumed.
	 */

	cptr = parserutils_inputstream_peek(lexer->input,
			lexer->bytesReadForToken, &clen);
	if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
		return CSS_NEEDDATA;

	if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
		return emitToken(lexer, CSS_TOKEN_CHAR, token);

	c = *((uint8_t *) cptr);

	if (c != '=')
		return emitToken(lexer, CSS_TOKEN_CHAR, token);

	APPEND(lexer, cptr, clen);

	switch (lexer->context.first) {
	case '~':
		type = CSS_TOKEN_INCLUDES;
		break;
	case '|':
		type = CSS_TOKEN_DASHMATCH;
		break;
	case '^':
		type = CSS_TOKEN_PREFIXMATCH;
		break;
	case '$':
		type = CSS_TOKEN_SUFFIXMATCH;
		break;
	case '*':
		type = CSS_TOKEN_SUBSTRINGMATCH;
		break;
	default:
		assert(0);
	}

	return emitToken(lexer, type, token);
}

css_error NumberOrPercentageOrDimension(css_lexer *lexer, 
		const css_token **token)
{
	css_token *t = &lexer->token;
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	enum { Initial = 0, Dot = 1, MoreDigits = 2, 
		Suffix = 3, NMChars = 4, Escape = 5 };

	/* NUMBER = num = [0-9]+ | [0-9]* '.' [0-9]+
	 * PERCENTAGE = num '%'
	 * DIMENSION = num ident
	 *
	 * The first digit, or '.' has been consumed.
	 */

	switch (lexer->substate) {
	case Initial:
		error = consumeDigits(lexer);
		if (error != CSS_OK)
			return error;

		/* Fall through */
	case Dot:
		lexer->substate = Dot;

		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			if (t->data.len == 1 && lexer->context.first == '.')
				return emitToken(lexer, CSS_TOKEN_CHAR, token);
			else
				return emitToken(lexer, CSS_TOKEN_NUMBER, 
						token);
		}

		c = *((uint8_t *) cptr);

		/* Bail if we've not got a '.' or we've seen one already */
		if (c != '.' || lexer->context.first == '.')
			goto suffix;

		/* Save the token length up to the end of the digits */
		lexer->context.origBytes = lexer->bytesReadForToken;

		/* Append the '.' to the token */
		APPEND(lexer, cptr, clen);

		/* Fall through */
	case MoreDigits:
		lexer->substate = MoreDigits;

		error = consumeDigits(lexer);
		if (error != CSS_OK)
			return error;

		if (lexer->bytesReadForToken - lexer->context.origBytes == 1) {
			/* No digits after dot => dot isn't part of number */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
		}

		/* Fall through */
	case Suffix:
	suffix:
		lexer->substate = Suffix;

		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			if (t->data.len == 1 && lexer->context.first == '.')
				return emitToken(lexer, CSS_TOKEN_CHAR, token);
			else
				return emitToken(lexer, CSS_TOKEN_NUMBER, 
						token);
		}

		c = *((uint8_t *) cptr);

		/* A solitary '.' is a CHAR, not numeric */
		if (t->data.len == 1 && lexer->context.first == '.')
			return emitToken(lexer, CSS_TOKEN_CHAR, token);

		if (c == '%') {
			APPEND(lexer, cptr, clen);

			return emitToken(lexer, CSS_TOKEN_PERCENTAGE, token);
		} else if (!startNMStart(c)) {
			return emitToken(lexer, CSS_TOKEN_NUMBER, token);
		}

		if (c != '\\') {
			APPEND(lexer, cptr, clen);
		} else {
			lexer->bytesReadForToken += clen;
			goto escape;
		}

		/* Fall through */
	case NMChars:
	nmchars:
		lexer->substate = NMChars;

		error = consumeNMChars(lexer);
		if (error != CSS_OK)
			return error;

		break;
	case Escape:
	escape:
		lexer->substate = Escape;

		error = consumeEscape(lexer, false);
		if (error != CSS_OK) {
			if (error == CSS_EOF || error == CSS_INVALID) {
				/* Rewind the '\\' */
				lexer->bytesReadForToken -= 1;

				/* This can only be a number */
				return emitToken(lexer, 
						CSS_TOKEN_NUMBER, token);
			}

			return error;
		}

		goto nmchars;
	}

	return emitToken(lexer, CSS_TOKEN_DIMENSION, token);
}

css_error S(css_lexer *lexer, const css_token **token)
{
	css_error error;

	/* S = wc*
	 * 
	 * The first whitespace character has been consumed.
	 */

	error = consumeWChars(lexer);
	if (error != CSS_OK)
		return error;

	return emitToken(lexer, CSS_TOKEN_S, token);
}

css_error Start(css_lexer *lexer, const css_token **token)
{
	css_token *t = &lexer->token;
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_error error;

start:

	/* Advance past the input read for the previous token */
	if (lexer->bytesReadForToken > 0) {
		parserutils_inputstream_advance(
				lexer->input, lexer->bytesReadForToken);
		lexer->bytesReadForToken = 0;
	}

	/* Reset in preparation for the next token */
	t->type = CSS_TOKEN_EOF;
	t->data.ptr = NULL;
	t->data.len = 0;
	t->col = lexer->currentCol;
	t->line = lexer->currentLine;
	lexer->escapeSeen = false;
	if (lexer->unescapedTokenData != NULL)
		lexer->unescapedTokenData->length = 0;

	cptr = parserutils_inputstream_peek(lexer->input, 0, &clen);
	if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
		return CSS_NEEDDATA;

	if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
		return emitToken(lexer, CSS_TOKEN_EOF, token);

	APPEND(lexer, cptr, clen);

	c = *((uint8_t *) cptr);

	if (clen > 1 || c >= 0x80) {
		lexer->state = sIDENT;
		lexer->substate = 0;
		return IdentOrFunction(lexer, token);
	}

	switch (c) {
	case '@':
		lexer->state = sATKEYWORD;
		lexer->substate = 0;
		return AtKeyword(lexer, token);
	case '"': case '\'':
		lexer->state = sSTRING;
		lexer->substate = 0;
		lexer->context.first = c;
		return String(lexer, token);
	case '#':
		lexer->state = sHASH;
		lexer->substate = 0;
		lexer->context.origBytes = lexer->bytesReadForToken;
		return Hash(lexer, token);
	case '0': case '1': case '2': case '3': case '4': 
	case '5': case '6': case '7': case '8': case '9':
	case '.':
		lexer->state = sNUMBER;
		lexer->substate = 0;
		lexer->context.first = c;
		return NumberOrPercentageOrDimension(lexer, token);
	case '<':
		lexer->state = sCDO;
		lexer->substate = 0;
		return CDO(lexer, token);
	case '-':
		lexer->state = sCDC;
		lexer->substate = 0;
		return CDCOrIdentOrFunction(lexer, token);
	case ' ': case '\t': case '\r': case '\n': case '\f':
		lexer->state = sS;
		lexer->substate = 0;
		if (c == '\n' || c == '\f') {
			lexer->currentCol = 1;
			lexer->currentLine++;
		}
		lexer->context.lastWasCR = (c == '\r');
		return S(lexer, token);
	case '/':
		lexer->state = sCOMMENT;
		lexer->substate = 0;
		lexer->context.lastWasStar = false;
		lexer->context.lastWasCR = false;
		error = Comment(lexer, token);
		if (!lexer->emit_comments && error == CSS_OK)
			goto start;
		return error;
	case '~': case '|': case '^': case '$': case '*':
		lexer->state = sMATCH;
		lexer->substate = 0;
		lexer->context.first = c;
		return Match(lexer, token);
	case 'u': case 'U':
		lexer->state = sURI;
		lexer->substate = 0;
		return URIOrUnicodeRangeOrIdentOrFunction(lexer, token);
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': 
	case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': 
	case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
	case 's': case 't': /*  'u'*/ case 'v': case 'w': case 'x': 
	case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
	case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
	case 'S': case 'T': /*  'U'*/ case 'V': case 'W': case 'X':
	case 'Y': case 'Z':
	case '_': 
		lexer->state = sIDENT;
		lexer->substate = 0;
		return IdentOrFunction(lexer, token);
	case '\\':
		lexer->state = sESCAPEDIDENT;
		lexer->substate = 0;
		return EscapedIdentOrFunction(lexer, token);
	default:
		return emitToken(lexer, CSS_TOKEN_CHAR, token);
	}
}

css_error String(css_lexer *lexer, const css_token **token)
{
	css_error error;

	/* STRING = string
	 *
	 * The open quote has been consumed.
	 */

	error = consumeString(lexer);
	if (error != CSS_OK && error != CSS_EOF)
		return error;

	return emitToken(lexer, 
			error == CSS_EOF ? CSS_TOKEN_EOF : CSS_TOKEN_STRING, 
			token);
}

css_error URIOrUnicodeRangeOrIdentOrFunction(css_lexer *lexer, 
		const css_token **token)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;

	/* URI = "url(" w (string | urlchar*) w ')' 
	 * UNICODE-RANGE = [Uu] '+' [0-9a-fA-F?]{1,6}(-[0-9a-fA-F]{1,6})?
	 * IDENT = ident = [-]? nmstart nmchar*
	 * FUNCTION = ident '(' = [-]? nmstart nmchar* '('
	 *
	 * The 'u' (or 'U') has been consumed.
	 */

	cptr = parserutils_inputstream_peek(lexer->input, 
			lexer->bytesReadForToken, &clen);
	if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
		return CSS_NEEDDATA;

	if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
		/* IDENT, rather than CHAR */
		return emitToken(lexer, CSS_TOKEN_IDENT, token);
	}

	c = *((uint8_t *) cptr);

	if (c == 'r' || c == 'R') {
		APPEND(lexer, cptr, clen);

		lexer->state = sURL;
		lexer->substate = 0;
		return URI(lexer, token);
	} else if (c == '+') {
		APPEND(lexer, cptr, clen);

		lexer->state = sUCR;
		lexer->substate = 0;
		lexer->context.hexCount = 0;
		return UnicodeRange(lexer, token);
	}

	/* Can only be IDENT or FUNCTION. Reprocess current character */
	lexer->state = sIDENT;
	lexer->substate = 0;
	return IdentOrFunction(lexer, token);
}

css_error URI(css_lexer *lexer, const css_token **token)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_error error;
	enum { Initial = 0, LParen = 1, W1 = 2, Quote = 3, 
		URL = 4, W2 = 5, RParen = 6, String = 7 };

	/* URI = "url(" w (string | urlchar*) w ')' 
	 *
	 * 'u' and 'r' have been consumed.
	 */

	switch (lexer->substate) {
	case Initial:
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/* IDENT */
			return emitToken(lexer, CSS_TOKEN_IDENT, token);
		}

		c = *((uint8_t *) cptr);

		if (c == 'l' || c == 'L') {
			APPEND(lexer, cptr, clen);
		} else {
			lexer->state = sIDENT;
			lexer->substate = 0;
			return IdentOrFunction(lexer, token);
		}

		/* Fall through */
	case LParen:
		lexer->substate = LParen;

		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			return emitToken(lexer, CSS_TOKEN_IDENT, token);

		c = *((uint8_t *) cptr);

		if (c == '(') {
			APPEND(lexer, cptr, clen);
		} else {
			lexer->state = sIDENT;
			lexer->substate = 0;
			return IdentOrFunction(lexer, token);
		}

		/* Save the number of input bytes read for "url(" */
		lexer->context.bytesForURL = lexer->bytesReadForToken;
		/* And the length of the token data at the same point */
		lexer->context.dataLenForURL = lexer->token.data.len;

		lexer->context.lastWasCR = false;

		/* Fall through */
	case W1:
		lexer->substate = W1;

		error = consumeWChars(lexer);
		if (error != CSS_OK)
			return error;

		/* Fall through */
	case Quote:
		lexer->substate = Quote;

		cptr = parserutils_inputstream_peek(lexer->input,
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/* Rewind to "url(" */
			lexer->bytesReadForToken = lexer->context.bytesForURL;
			lexer->token.data.len = lexer->context.dataLenForURL;
			return emitToken(lexer, CSS_TOKEN_FUNCTION, token);
		}

		c = *((uint8_t *) cptr);

		if (c == '"' || c == '\'') {
			APPEND(lexer, cptr, clen);

			lexer->context.first = c;

			goto string;
		}

		/* Potential minor optimisation: If string is more common, 
		 * then fall through to that state and branch for the URL 
		 * state. Need to investigate a reasonably large corpus of 
		 * real-world data to determine if this is worthwhile. */

		/* Fall through */
	case URL:
		lexer->substate = URL;

		error = consumeURLChars(lexer);
		if (error != CSS_OK)
			return error;

		lexer->context.lastWasCR = false;

		/* Fall through */
	case W2:
	w2:
		lexer->substate = W2;

		error = consumeWChars(lexer);
		if (error != CSS_OK)
			return error;

		/* Fall through */
	case RParen:
		lexer->substate = RParen;

		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			/* Rewind to "url(" */
			lexer->bytesReadForToken = lexer->context.bytesForURL;
			lexer->token.data.len = lexer->context.dataLenForURL;
			return emitToken(lexer, CSS_TOKEN_FUNCTION, token);
		}

		c = *((uint8_t *) cptr);

		if (c != ')') {
			/* Rewind to "url(" */
			lexer->bytesReadForToken = lexer->context.bytesForURL;
			lexer->token.data.len = lexer->context.dataLenForURL;
			return emitToken(lexer, CSS_TOKEN_FUNCTION, token);
		}

		APPEND(lexer, cptr, clen);
		break;
	case String:
	string:
		lexer->substate = String;

		error = consumeString(lexer);
		if (error != CSS_OK && error != CSS_EOF)
			return error;

		/* EOF gets handled in RParen */

		lexer->context.lastWasCR = false;

		goto w2;
	}

	return emitToken(lexer, CSS_TOKEN_URI, token);
}

css_error UnicodeRange(css_lexer *lexer, const css_token **token)
{
	css_token *t = &lexer->token;
	uintptr_t cptr = PARSERUTILS_INPUTSTREAM_OOD; /* GCC: shush */
	uint8_t c = 0; /* GCC: shush */
	size_t clen;
	enum { Initial = 0, MoreDigits = 1 };

	/* UNICODE-RANGE = [Uu] '+' [0-9a-fA-F?]{1,6}(-[0-9a-fA-F]{1,6})?
	 * 
	 * "U+" has been consumed.
	 */

	switch (lexer->substate) {
	case Initial:
		/* Attempt to consume 6 hex digits (or question marks) */
		for (; lexer->context.hexCount < 6; lexer->context.hexCount++) {
			cptr = parserutils_inputstream_peek(lexer->input,
					lexer->bytesReadForToken, &clen);
			if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
				return CSS_NEEDDATA;

			if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
				if (lexer->context.hexCount == 0) {
					/* Remove '+' */
					lexer->bytesReadForToken -= 1;
					t->data.len -= 1;
					/* u == IDENT */
					return emitToken(lexer, 
							CSS_TOKEN_IDENT, token);
				} else {
					return emitToken(lexer, 
						CSS_TOKEN_UNICODE_RANGE, token);
				}
			}

			c = *((uint8_t *) cptr);

			if (isHex(c) || c == '?') {
				APPEND(lexer, cptr, clen);
			} else {
				break;
			}
		}

		if (lexer->context.hexCount == 0) {
			/* We didn't consume any valid Unicode Range digits */
			/* Remove the '+' */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
			/* 'u' == IDENT */
			return emitToken(lexer, CSS_TOKEN_IDENT, token);
		} 

		if (lexer->context.hexCount == 6) {
			/* Consumed 6 valid characters. Look for '-' */
			cptr = parserutils_inputstream_peek(lexer->input, 
					lexer->bytesReadForToken, &clen);
			if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
				return CSS_NEEDDATA;

			if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
				return emitToken(lexer, 
						CSS_TOKEN_UNICODE_RANGE, token);

			c = *((uint8_t *) cptr);
		}

		/* If we've got a '-', then we may have a 
		 * second range component */
		if (c != '-') {
			/* Reached the end of the range */
			return emitToken(lexer, CSS_TOKEN_UNICODE_RANGE, token);
		}

		APPEND(lexer, cptr, clen);

		/* Reset count for next set of digits */
		lexer->context.hexCount = 0;

		/* Fall through */
	case MoreDigits:
		lexer->substate = MoreDigits;

		/* Consume up to 6 hex digits */
		for (; lexer->context.hexCount < 6; lexer->context.hexCount++) {
			cptr = parserutils_inputstream_peek(lexer->input, 
					lexer->bytesReadForToken, &clen);
			if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
				return CSS_NEEDDATA;

			if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
				if (lexer->context.hexCount == 0) {
					/* Remove '-' */
					lexer->bytesReadForToken -= 1;
					t->data.len -= 1;
				}

				return emitToken(lexer, 
						CSS_TOKEN_UNICODE_RANGE, token);
			}

			c = *((uint8_t *) cptr);

			if (isHex(c)) {
				APPEND(lexer, cptr, clen);
			} else {
				break;
			}
		}

		if (lexer->context.hexCount == 0) {
			/* No hex digits consumed. Remove '-' */
			lexer->bytesReadForToken -= 1;
			t->data.len -= 1;
		}
	}

	return emitToken(lexer, CSS_TOKEN_UNICODE_RANGE, token);
}

/******************************************************************************
 * Input consumers                                                            *
 ******************************************************************************/

css_error consumeDigits(css_lexer *lexer)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;

	/* digit = [0-9] */

	/* Consume all digits */
	do {
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			return CSS_OK;

		c = *((uint8_t *) cptr);

		if (isDigit(c)) {
			APPEND(lexer, cptr, clen);
		}
	} while (isDigit(c));

	return CSS_OK;
}

css_error consumeEscape(css_lexer *lexer, bool nl)
{
	uintptr_t cptr, sptr;
	uint8_t c;
	size_t clen, slen;
	css_error error;

	/* escape = unicode | '\' [^\n\r\f0-9a-fA-F] 
	 * 
	 * The '\' has been consumed.
	 */

	cptr = parserutils_inputstream_peek(lexer->input, 
			lexer->bytesReadForToken, &clen);
	if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
		return CSS_NEEDDATA;

	if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
		return CSS_EOF;

	c = *((uint8_t *) cptr);

	if (!nl && (c == '\n' || c == '\r' || c == '\f')) {
		/* These are not permitted */
		return CSS_INVALID;
	}

	/* Create unescaped buffer, if it doesn't already exist */
	if (lexer->unescapedTokenData == NULL) {
		lexer->unescapedTokenData = 
			parserutils_buffer_create(lexer->alloc, lexer->pw);
		if (lexer->unescapedTokenData == NULL)
			return CSS_NOMEM;
	}

	/* If this is the first escaped character we've seen for this token,
	 * we must copy the characters we've read to the unescaped buffer */
	if (!lexer->escapeSeen) {
		if (lexer->bytesReadForToken > 1) {
			sptr = parserutils_inputstream_peek(
					lexer->input, 0, &slen);

			assert(sptr != PARSERUTILS_INPUTSTREAM_EOF && 
					sptr != PARSERUTILS_INPUTSTREAM_OOD);

			/* -1 to skip '\\' */
			error = css_error_from_parserutils_error(
				parserutils_buffer_append(
					lexer->unescapedTokenData, 
					(const uint8_t *) sptr, 
					lexer->bytesReadForToken - 1));
			if (error != CSS_OK)
				return error;
		}

		lexer->token.data.len = lexer->bytesReadForToken - 1;
		lexer->escapeSeen = true;
	}

	if (isHex(c)) {
		lexer->bytesReadForToken += clen;

		error = consumeUnicode(lexer, charToHex(c));
		if (error != CSS_OK) {
			/* Rewind for next time */
			lexer->bytesReadForToken -= clen;
		}

		return error;
	}

	/* If we're handling escaped newlines, convert CR(LF)? to LF */
	if (nl && c == '\r') {
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken + clen, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF) {
			c = '\n';
			APPEND(lexer, &c, 1);

			lexer->currentCol = 1;
			lexer->currentLine++;

			return CSS_OK;
		}

		c = *((uint8_t *) cptr);

		if (c == '\n') {
			APPEND(lexer, &c, 1);
			/* And skip the '\r' in the input */
			lexer->bytesReadForToken += clen;

			lexer->currentCol = 1;
			lexer->currentLine++;

			return CSS_OK;
		}
	} else if (nl && (c == '\n' || c == '\f')) {
		/* APPEND will increment this appropriately */
		lexer->currentCol = 0;
		lexer->currentLine++;
	} else if (c != '\n' && c != '\r' && c != '\f') {
		lexer->currentCol++;
	}

	/* Append the unescaped character */
	APPEND(lexer, cptr, clen);

	return CSS_OK;
}

css_error consumeNMChars(css_lexer *lexer)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_error error;

	/* nmchar = [a-zA-Z] | '-' | '_' | nonascii | escape */

	do {
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			return CSS_OK;

		c = *((uint8_t *) cptr);

		if (startNMChar(c) && c != '\\') {
			APPEND(lexer, cptr, clen);
		}

		if (c == '\\') {
			lexer->bytesReadForToken += clen;

			error = consumeEscape(lexer, false);
			if (error != CSS_OK) {
				/* Rewind '\\', so we do the 
				 * right thing next time */
				lexer->bytesReadForToken -= clen;

				/* Convert either EOF or INVALID into OK.
				 * This will cause the caller to believe that
				 * all NMChars in the sequence have been 
				 * processed (and thus proceed to the next
				 * state). Eventually, the '\\' will be output
				 * as a CHAR. */
				if (error == CSS_EOF || error == CSS_INVALID)
					return CSS_OK;

				return error;
			}
		}
	} while (startNMChar(c));

	return CSS_OK;
}

css_error consumeString(css_lexer *lexer)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	uint8_t quote = lexer->context.first;
	uint8_t permittedquote = (quote == '"') ? '\'' : '"';
	css_error error;

	/* string = '"' (stringchar | "'")* '"' | "'" (stringchar | '"')* "'"
	 *
	 * The open quote has been consumed.
	 */

	do {
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			return CSS_EOF;

		c = *((uint8_t *) cptr);

		if (c == permittedquote) {
			APPEND(lexer, cptr, clen);
		} else if (startStringChar(c)) {
			error = consumeStringChars(lexer);
			if (error != CSS_OK)
				return error;
		} else if (c != quote) {
			/* Invalid character in string -- skip */
			lexer->bytesReadForToken += clen;
		}
	} while(c != quote);

	/* Append closing quote to token data */
	APPEND(lexer, cptr, clen);

	return CSS_OK;
}

css_error consumeStringChars(css_lexer *lexer)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_error error;

	/* stringchar = urlchar | ' ' | ')' | '\' nl */

	do {
		cptr = parserutils_inputstream_peek(lexer->input,
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			return CSS_OK;

		c = *((uint8_t *) cptr);

		if (startStringChar(c) && c != '\\') {
			APPEND(lexer, cptr, clen);
		}

		if (c == '\\') {
			lexer->bytesReadForToken += clen;

			error = consumeEscape(lexer, true);
			if (error != CSS_OK) {
				/* Rewind '\\', so we do the 
				 * right thing next time. */
				lexer->bytesReadForToken -= clen;

				/* Convert EOF to OK. This causes the caller
				 * to believe that all StringChars have been
				 * processed. Eventually, the '\\' will be
				 * output as a CHAR. */
				if (error == CSS_EOF)
					return CSS_OK;

				return error;
			}
		}
	} while (startStringChar(c));

	return CSS_OK;

}

css_error consumeUnicode(css_lexer *lexer, uint32_t ucs)
{
	uintptr_t cptr = PARSERUTILS_INPUTSTREAM_OOD; /* GCC: shush */
	uint8_t c = 0;
	size_t clen;
	uint8_t utf8[6];
	uint8_t *utf8ptr = utf8;
	size_t utf8len = sizeof(utf8);
	size_t bytesReadInit = lexer->bytesReadForToken;
	int count;
	parserutils_error error;

	/* unicode = '\' [0-9a-fA-F]{1,6} wc? 
	 *
	 * The '\' and the first digit have been consumed.
	 */

	/* Attempt to consume a further five hex digits */
	for (count = 0; count < 5; count++) {
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD) {
			/* Rewind what we've read */
			lexer->bytesReadForToken = bytesReadInit;
			return CSS_NEEDDATA;
		}

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			break;

		c = *((uint8_t *) cptr);

		if (isHex(c)) {
			lexer->bytesReadForToken += clen;

			ucs = (ucs << 4) | charToHex(c);
		} else {
			break;
		}
	}

	/* Convert our UCS4 character to UTF-8 */
	error = parserutils_charset_utf8_from_ucs4(ucs, &utf8ptr, &utf8len);
	assert(error == PARSERUTILS_OK);

	/* Append it to the token data (unescaped buffer already set up) */
	/* We can't use the APPEND() macro here as we want to rewind correctly
	 * on error. Additionally, lexer->bytesReadForToken has already been
	 * advanced */
	error = appendToTokenData(lexer, (const uint8_t *) utf8, 
			sizeof(utf8) - utf8len);
	if (error != CSS_OK) {
		/* Rewind what we've read */
		lexer->bytesReadForToken = bytesReadInit;
		return error;
	}

	/* Finally, attempt to skip a whitespace character */
	if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
		return CSS_OK;

	if (isSpace(c)) {
		lexer->bytesReadForToken += clen;
	}

	/* +2 for '\' and first digit */
	lexer->currentCol += lexer->bytesReadForToken - bytesReadInit + 2;

	return CSS_OK;
}

css_error consumeURLChars(css_lexer *lexer)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;
	css_error error;

	/* urlchar = [\t!#-&(*-~] | nonascii | escape */

	do {
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			return CSS_OK;

		c = *((uint8_t *) cptr);

		if (startURLChar(c) && c != '\\') {
			APPEND(lexer, cptr, clen);
		}

		if (c == '\\') {
			lexer->bytesReadForToken += clen;

			error = consumeEscape(lexer, false);
			if (error != CSS_OK) {
				/* Rewind '\\', so we do the
				 * right thing next time */
				lexer->bytesReadForToken -= clen;

				/* Convert either EOF or INVALID into OK.
				 * This will cause the caller to believe that
				 * all URLChars in the sequence have been 
				 * processed (and thus proceed to the next
				 * state). Eventually, the '\\' will be output
				 * as a CHAR. */
				if (error == CSS_EOF || error == CSS_INVALID)
					return CSS_OK;

				return error;
			}
		}
	} while (startURLChar(c));

	return CSS_OK;
}

css_error consumeWChars(css_lexer *lexer)
{
	uintptr_t cptr;
	uint8_t c;
	size_t clen;

	do {
		cptr = parserutils_inputstream_peek(lexer->input, 
				lexer->bytesReadForToken, &clen);
		if (cptr == PARSERUTILS_INPUTSTREAM_OOD)
			return CSS_NEEDDATA;

		if (cptr == PARSERUTILS_INPUTSTREAM_EOF)
			return CSS_OK;

		c = *((uint8_t *) cptr);

		if (isSpace(c)) {
			APPEND(lexer, cptr, clen);
		}

		if (c == '\n' || c == '\f') {
			lexer->currentCol = 1;
			lexer->currentLine++;
		}

		if (lexer->context.lastWasCR && c != '\n') {
			lexer->currentCol = 1;
			lexer->currentLine++;
		}
		lexer->context.lastWasCR = (c == '\r');
	} while (isSpace(c));

	if (lexer->context.lastWasCR) {
		lexer->currentCol = 1;
		lexer->currentLine++;
	}

	return CSS_OK;
}

/******************************************************************************
 * More utility routines                                                      *
 ******************************************************************************/

uint32_t charToHex(uint8_t c)
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

bool startNMChar(uint8_t c)
{
	return c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || 
		('0' <= c && c <= '9') || c == '-' || c >= 0x80 || c == '\\';
}

bool startNMStart(uint8_t c)
{
	return c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
		c >= 0x80 || c == '\\';
}

bool startStringChar(uint8_t c)
{
	return startURLChar(c) || c == ' ' || c == ')';
}

bool startURLChar(uint8_t c)
{
	return c == '\t' || c == '!' || ('#' <= c && c <= '&') || c == '(' ||
		('*' <= c && c <= '~') || c >= 0x80 || c == '\\';
}

bool isDigit(uint8_t c)
{
	return '0' <= c && c <= '9';
}

bool isHex(uint8_t c)
{
	return isDigit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

bool isSpace(uint8_t c)
{
	return c == ' ' || c == '\r' || c == '\n' || c == '\f' || c == '\t';
}

