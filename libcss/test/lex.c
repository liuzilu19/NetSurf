#include <inttypes.h>
#include <stdio.h>

#include <parserutils/charset/utf8.h>
#include <parserutils/input/inputstream.h>

#include <libcss/libcss.h>

#include "charset/detect.h"
#include "utils/utils.h"

#include "lex/lex.h"

#include "testutils.h"

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

static void printToken(const css_token *token)
{
#if 0
	UNUSED(token);
#else
	printf("[%d, %d] : ", token->line, token->col);

	switch (token->type) {
	case CSS_TOKEN_IDENT:
		printf("IDENT(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_ATKEYWORD:
		printf("ATKEYWORD(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_STRING:
		printf("STRING(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_INVALID_STRING:
		printf("INVALID(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_HASH:
		printf("HASH(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_NUMBER:
		printf("NUMBER(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_PERCENTAGE:
		printf("PERCENTAGE(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_DIMENSION:
		printf("DIMENSION(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_URI:
		printf("URI(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_UNICODE_RANGE:
		printf("UNICODE-RANGE(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_CDO:
		printf("CDO");
		break;
	case CSS_TOKEN_CDC:
		printf("CDC");
		break;
	case CSS_TOKEN_S:
		printf("S");
		break;
	case CSS_TOKEN_COMMENT:
		printf("COMMENT(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_FUNCTION:
		printf("FUNCTION(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_INCLUDES:
		printf("INCLUDES");
		break;
	case CSS_TOKEN_DASHMATCH:
		printf("DASHMATCH");
		break;
	case CSS_TOKEN_PREFIXMATCH:
		printf("PREFIXMATCH");
		break;
	case CSS_TOKEN_SUFFIXMATCH:
		printf("SUFFIXMATCH");
		break;
	case CSS_TOKEN_SUBSTRINGMATCH:
		printf("SUBSTRINGMATCH");
		break;
	case CSS_TOKEN_CHAR:
		printf("CHAR(%.*s)", token->data.len, token->data.ptr);
		break;
	case CSS_TOKEN_EOF:
		printf("EOF");
		break;
	}

	printf("\n");
#endif
}

int main(int argc, char **argv)
{
	parserutils_inputstream *stream;
	css_lexer *lexer;
	FILE *fp;
	size_t len, origlen;
#define CHUNK_SIZE (4096)
	uint8_t buf[CHUNK_SIZE];
	const css_token *tok;
	css_error error;

	if (argc != 3) {
		printf("Usage: %s <aliases_file> <filename>\n", argv[0]);
		return 1;
	}

	/* Initialise library */
	assert(css_initialise(argv[1], myrealloc, NULL) == CSS_OK);

	stream = parserutils_inputstream_create("UTF-8", CSS_CHARSET_DICTATED,
			css_charset_extract, 
			(parserutils_alloc) myrealloc, NULL);
	assert(stream != NULL);

	lexer = css_lexer_create(stream, myrealloc, NULL);
	assert(lexer != NULL);

	fp = fopen(argv[2], "rb");
	if (fp == NULL) {
		printf("Failed opening %s\n", argv[2]);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	origlen = len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	while (len >= CHUNK_SIZE) {
		fread(buf, 1, CHUNK_SIZE, fp);

		assert(parserutils_inputstream_append(stream,
				buf, CHUNK_SIZE) == PARSERUTILS_OK);

		len -= CHUNK_SIZE;

		while ((error = css_lexer_get_token(lexer, &tok)) == CSS_OK) {
			printToken(tok);

			if (tok->type == CSS_TOKEN_EOF)
				break;
		}
	}

	if (len > 0) {
		fread(buf, 1, len, fp);

		assert(parserutils_inputstream_append(stream,
				buf, len) == PARSERUTILS_OK);

		len = 0;
	}

	fclose(fp);

	assert(parserutils_inputstream_append(stream, NULL, 0) == 
			PARSERUTILS_OK);

	while ((error = css_lexer_get_token(lexer, &tok)) == CSS_OK) {
		printToken(tok);

		if (tok->type == CSS_TOKEN_EOF)
			break;
	}

	css_lexer_destroy(lexer);

	parserutils_inputstream_destroy(stream);

	assert(css_finalise(myrealloc, NULL) == CSS_OK);

	printf("PASS\n");

	return 0;
}

