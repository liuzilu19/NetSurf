#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* These two are for htonl / ntohl */
#include <arpa/inet.h>
#include <netinet/in.h>

#include "charset/charset.h"
#include <parserutils/charset/codec.h>

#include "utils/utils.h"

#include "testutils.h"

typedef struct line_ctx {
	parserutils_charset_codec *codec;

	size_t buflen;
	size_t bufused;
	uint8_t *buf;
	size_t explen;
	size_t expused;
	uint8_t *exp;

	bool indata;
	bool inexp;

	parserutils_error exp_ret;

	enum { ENCODE, DECODE, BOTH } dir;
} line_ctx;

static bool handle_line(const char *data, size_t datalen, void *pw);
static void run_test(line_ctx *ctx);

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	return realloc(ptr, len);
}

int main(int argc, char **argv)
{
	line_ctx ctx;

	if (argc != 3) {
		printf("Usage: %s <aliases_file> <filename>\n", argv[0]);
		return 1;
	}

	assert(parserutils_charset_initialise(argv[1], myrealloc, NULL) == 
			PARSERUTILS_OK);

	assert(parserutils_charset_codec_create("NATS-SEFI-ADD",
			myrealloc, NULL) == NULL);

	ctx.codec = parserutils_charset_codec_create("UTF-16", myrealloc, NULL);
	assert(ctx.codec != NULL);

	ctx.buflen = parse_filesize(argv[2]);
	if (ctx.buflen == 0)
		return 1;

	ctx.buf = malloc(2 * ctx.buflen);
	if (ctx.buf == NULL) {
		printf("Failed allocating %u bytes\n",
				(unsigned int) ctx.buflen);
		return 1;
	}

	ctx.exp = ctx.buf + ctx.buflen;
	ctx.explen = ctx.buflen;

	ctx.buf[0] = '\0';
	ctx.exp[0] = '\0';
	ctx.bufused = 0;
	ctx.expused = 0;
	ctx.indata = false;
	ctx.inexp = false;
	ctx.exp_ret = PARSERUTILS_OK;

	assert(parse_testfile(argv[2], handle_line, &ctx) == true);

	/* and run final test */
	if (ctx.bufused > 0 && ctx.buf[ctx.bufused - 1] == '\n')
		ctx.bufused -= 1;

	if (ctx.expused > 0 && ctx.exp[ctx.expused - 1] == '\n')
		ctx.expused -= 1;

	run_test(&ctx);

	free(ctx.buf);

	parserutils_charset_codec_destroy(ctx.codec);

	assert(parserutils_charset_finalise(myrealloc, NULL) == 
			PARSERUTILS_OK);

	printf("PASS\n");

	return 0;
}

/**
 * Converts hex character ('0' ... '9' or 'a' ... 'f' or 'A' ... 'F') to
 * digit value.
 * \param hex Valid hex character
 * \return Corresponding digit value.
 */
static inline int hex2digit(char hex)
{
	return (hex <= '9') ? hex - '0' : (hex | 0x20) - 'a' + 10;
}

bool handle_line(const char *data, size_t datalen, void *pw)
{
	line_ctx *ctx = (line_ctx *) pw;

	if (data[0] == '#') {
		if (ctx->inexp) {
			/* This marks end of testcase, so run it */

			if (ctx->buf[ctx->bufused - 1] == '\n')
				ctx->bufused -= 1;

			if (ctx->exp[ctx->expused - 1] == '\n')
				ctx->expused -= 1;

			run_test(ctx);

			ctx->buf[0] = '\0';
			ctx->exp[0] = '\0';
			ctx->bufused = 0;
			ctx->expused = 0;
			ctx->exp_ret = PARSERUTILS_OK;
		}

		if (strncasecmp(data+1, "data", 4) == 0) {
			parserutils_charset_codec_optparams params;
			const char *ptr = data + 6;

			ctx->indata = true;
			ctx->inexp = false;

			if (strncasecmp(ptr, "decode", 6) == 0)
				ctx->dir = DECODE;
			else if (strncasecmp(ptr, "encode", 6) == 0)
				ctx->dir = ENCODE;
			else
				ctx->dir = BOTH;

			ptr += 7;

			if (strncasecmp(ptr, "LOOSE", 5) == 0) {
				params.error_mode.mode =
					PARSERUTILS_CHARSET_CODEC_ERROR_LOOSE;
				ptr += 6;
			} else if (strncasecmp(ptr, "STRICT", 6) == 0) {
				params.error_mode.mode =
					PARSERUTILS_CHARSET_CODEC_ERROR_STRICT;
				ptr += 7;
			} else {
				params.error_mode.mode =
					PARSERUTILS_CHARSET_CODEC_ERROR_TRANSLIT;
				ptr += 9;
			}

			assert(parserutils_charset_codec_setopt(ctx->codec,
				PARSERUTILS_CHARSET_CODEC_ERROR_MODE,
				(parserutils_charset_codec_optparams *) &params)
				== PARSERUTILS_OK);
		} else if (strncasecmp(data+1, "expected", 8) == 0) {
			ctx->indata = false;
			ctx->inexp = true;

			ctx->exp_ret = parserutils_error_from_string(data + 10,
					datalen - 10 - 1 /* \n */);
		} else if (strncasecmp(data+1, "reset", 5) == 0) {
			ctx->indata = false;
			ctx->inexp = false;

			parserutils_charset_codec_reset(ctx->codec);
		}
	} else {
		if (ctx->indata) {
			/* Process "&#xNNNN" as 16-bit code units.  */
			while (datalen) {
				if (data[0] == '\n') {
					ctx->buf[ctx->bufused++] = *data++;
					--datalen;
					continue;
				}
				assert(datalen >= sizeof ("&#xNNNN")-1 \
					&& data[0] == '&' && data[1] == '#' \
					&& data[2] == 'x' && isxdigit(data[3]) \
					&& isxdigit(data[4]) && isxdigit(data[5]) \
					&& isxdigit(data[6]));
				/* UTF-16 code is always host endian (different
				   than UCS-32 !).  */
				ctx->buf[ctx->bufused++]
					= (hex2digit(data[5]) << 4) | hex2digit(data[6]);
				ctx->buf[ctx->bufused++]
					= (hex2digit(data[3]) << 4) | hex2digit(data[4]);
				data += sizeof ("&#xNNNN")-1;
				datalen -= sizeof ("&#xNNNN")-1;
			}
		}
		if (ctx->inexp) {
			/* Process "&#xXXXXYYYY as 32-bit code units.  */
			while (datalen) {
				if (data[0] == '\n') {
					ctx->exp[ctx->expused++] = *data++;
					--datalen;
					continue;
				}
				assert(datalen >= sizeof ("&#xXXXXYYYY")-1 \
					&& data[0] == '&' && data[1] == '#' \
					&& data[2] == 'x' && isxdigit(data[3]) \
					&& isxdigit(data[4]) && isxdigit(data[5]) \
					&& isxdigit(data[6]) && isxdigit(data[7]) \
					&& isxdigit(data[8]) && isxdigit(data[9]) \
					&& isxdigit(data[10]));
				/* UCS-4 code is always big endian, so convert
				   host endian to big endian.  */
				const uint32_t nCodePoint =
					htonl((hex2digit(data[3]) << 28)
					| (hex2digit(data[4]) << 24)
					| (hex2digit(data[5]) << 20)
					| (hex2digit(data[6]) << 16)
					| (hex2digit(data[7]) << 12)
					| (hex2digit(data[8]) << 8)
					| (hex2digit(data[9]) << 4)
					| hex2digit(data[10]));
				ctx->exp[ctx->expused++] = (nCodePoint >>  0) & 0xFF;
				ctx->exp[ctx->expused++] = (nCodePoint >>  8) & 0xFF;
				ctx->exp[ctx->expused++] = (nCodePoint >> 16) & 0xFF;
				ctx->exp[ctx->expused++] = (nCodePoint >> 24) & 0xFF;
				data += sizeof ("&#xXXXXYYYY")-1;
				datalen -= sizeof ("&#xXXXXYYYY")-1;
			}
		}
	}

	return true;
}

void run_test(line_ctx *ctx)
{
	static int testnum;
	size_t destlen = ctx->bufused * 4;
	uint8_t dest[destlen];
	uint8_t *pdest = dest;
	const uint8_t *psrc = ctx->buf;
	size_t srclen = ctx->bufused;
	size_t i;

	if (ctx->dir == DECODE) {
		assert(parserutils_charset_codec_decode(ctx->codec,
				&psrc, &srclen,
				&pdest, &destlen) == ctx->exp_ret);
	} else if (ctx->dir == ENCODE) {
		assert(parserutils_charset_codec_encode(ctx->codec,
				&psrc, &srclen,
				&pdest, &destlen) == ctx->exp_ret);
	} else {
		size_t templen = ctx->bufused * 4;
		uint8_t temp[templen];
		uint8_t *ptemp = temp;
		const uint8_t *ptemp2;
		size_t templen2;

		assert(parserutils_charset_codec_decode(ctx->codec,
				&psrc, &srclen,
				&ptemp, &templen) == ctx->exp_ret);
		/* \todo currently there is no way to specify the number of
		   consumed & produced data in case of a deliberate bad input
		   data set.  */
		if (ctx->exp_ret == PARSERUTILS_OK) {
			assert(temp + (ctx->bufused * 4 - templen) == ptemp);
		}

		ptemp2 = temp;
		templen2 = ctx->bufused * 4 - templen;
		assert(parserutils_charset_codec_encode(ctx->codec,
				&ptemp2, &templen2,
				&pdest, &destlen) == ctx->exp_ret);
		if (ctx->exp_ret == PARSERUTILS_OK) {
			assert(templen2 == 0);
			assert(temp + (ctx->bufused * 4 - templen) == ptemp2);
		}
	}
	if (ctx->exp_ret == PARSERUTILS_OK) {
		assert(srclen == 0);
		assert(ctx->buf + ctx->bufused == psrc);
		assert(dest + (ctx->bufused * 4 - destlen) == pdest);
		assert(ctx->bufused * 4 - destlen == ctx->expused);
	}

	printf("%d: Read '", ++testnum);
	for (i = 0; i < ctx->expused; i++) {
		printf("%c%c ", "0123456789abcdef"[(dest[i] >> 4) & 0xf],
				"0123456789abcdef"[dest[i] & 0xf]);
	}
	printf("' Expected '");
	for (i = 0; i < ctx->expused; i++) {
		printf("%c%c ", "0123456789abcdef"[(ctx->exp[i] >> 4) & 0xf],
				"0123456789abcdef"[ctx->exp[i] & 0xf]);
	}
	printf("'\n");

	assert(pdest == dest + ctx->expused);
	assert(memcmp(dest, ctx->exp, ctx->expused) == 0);
}

