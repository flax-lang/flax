/*
	Copyright (C) 2014 Quinten Lansu

	Permission is hereby granted, free of charge, to any person
	obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without
	restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or
	sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following
	conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.
*/

#include "../include/utf8rewind.h"

#define MAX_BASIC_MULTILINGUAL_PLANE  0xFFFF
#define MAX_LEGAL_UNICODE             0x10FFFF
#define REPLACEMENT_CHARACTER         0xFFFD
#define SURROGATE_HIGH_START          0xD800
#define SURROGATE_HIGH_END            0xDBFF
#define SURROGATE_LOW_START           0xDC00
#define SURROGATE_LOW_END             0xDFFF



static const size_t Utf8ByteMinimum[6] = {
	0x00000000,
	0x00000080,
	0x00000800,
	0x00010000,
	0x0010FFFF,
	0x0010FFFF
};

static const size_t Utf8ByteMaximum[6] = {
	0x0000007F,
	0x000007FF,
	0x0000FFFF,
	0x0010FFFF,
	0x0010FFFF,
	0x0010FFFF
};

static size_t lengthcodepoint(uint8_t input)
{
	if((input & 0x80) == 0) /* ASCII */
	{
		return 1;
	}
	else if(
		(input & 0xC0) != 0xC0 || /* Malformed continuation byte */
		(input & 0xFE) == 0xFE    /* Illegal byte */
	)
	{
		return 0;
	}
	else if((input & 0xE0) == 0xC0)
	{
		return 2;
	}
	else if((input & 0xF0) == 0xE0)
	{
		return 3;
	}
	else if((input & 0xF8) == 0xF0)
	{
		return 4;
	}
	else if((input & 0xFC) == 0xF8)
	{
		return 5;
	}
	else // (input & 0xFE) == 0xFC
	{
		return 6;
	}
}

static size_t writecodepoint(unicode_t codepoint, char** dst, size_t* dstSize, int32_t* errors)
{
	char* target;
	size_t encoded_length;

	if(codepoint < 0x80)
	{
		encoded_length = 1;
	}
	else if(codepoint < 0x800)
	{
		encoded_length = 2;
	}
	else if(codepoint < 0x10000)
	{
		encoded_length = 3;
	}
	else if(codepoint <= MAX_LEGAL_UNICODE)
	{
		encoded_length = 4;
	}
	else
	{
		codepoint = REPLACEMENT_CHARACTER;
		encoded_length = 3;
	}

	if(*dst != 0)
	{
		if(*dstSize < encoded_length)
		{
			if(errors != 0)
			{
				*errors = UTF8_ERR_NOT_ENOUGH_SPACE;
			}
			return 0;
		}

		target = *dst;

		switch(encoded_length)
		{

		case 1:
			*target++ = (char)codepoint;
			break;

		case 2:
			*target++ = (char)(codepoint >>   6)         | 0xC0;
			*target++ = (char)(codepoint         & 0x3F) | 0x80;
			break;

		case 3:
			*target++ = (char)(codepoint  >> 12)         | 0xE0;
			*target++ = (char)((codepoint >>  6) & 0x3F) | 0x80;
			*target++ = (char)(codepoint         & 0x3F) | 0x80;
			break;

		case 4:
			*target++ = (char)(codepoint  >> 18)         | 0xF0;
			*target++ = (char)((codepoint >> 12) & 0x3F) | 0x80;
			*target++ = (char)((codepoint >>  6) & 0x3F) | 0x80;
			*target++ = (char)(codepoint         & 0x3F) | 0x80;
			break;

		default:
			break;

		}

		*dst += encoded_length;
		*dstSize -= encoded_length;
	}

	return encoded_length;
}

static size_t readcodepoint(unicode_t* codepoint, const char* src, size_t srcSize)
{
	uint8_t current = (uint8_t)*src;
	uint8_t mask;
	size_t decoded_length;
	size_t src_index;
	size_t src_size = srcSize;

	if(current == 0)
	{
		*codepoint = 0;
		return 1;
	}

	decoded_length = lengthcodepoint(current);
	if(decoded_length == 0)
	{
		*codepoint = REPLACEMENT_CHARACTER;
		return 1;
	}

	mask = (decoded_length == 1) ? 0xFF : (1 << (7 - decoded_length)) - 1;

	*codepoint = (unicode_t)(current & mask);
	src++;

	for(src_index = 1; src_index < decoded_length && src_size > 0; ++src_index)
	{
		if((*src & 0x80) == 0)
		{
			/* Not a continuation byte for a multi-byte sequence */

			*codepoint = REPLACEMENT_CHARACTER;
			return src_index;
		}

		*codepoint = (*codepoint << 6) | (*src & 0x3F);
		src++;
		src_size--;
	}

	if(decoded_length > 1)
	{
		/* Check for overlong sequences */

		if(
			(*codepoint < Utf8ByteMinimum[decoded_length - 1] ||
			*codepoint > Utf8ByteMaximum[decoded_length - 1])
		)
		{
			*codepoint = REPLACEMENT_CHARACTER;
		}

		/* Check for surrogate pairs */

		else if(
			(*codepoint >= SURROGATE_HIGH_START && *codepoint <= SURROGATE_HIGH_END) ||
			(*codepoint >= SURROGATE_LOW_START && *codepoint <= SURROGATE_LOW_END)
		)
		{
			*codepoint = REPLACEMENT_CHARACTER;
		}
	}

	return decoded_length;
}

size_t utf8len(const char* text)
{
	const char* src;
	size_t src_index;
	size_t length = 0;
	size_t codepoint_length = 0;
	size_t text_length = 0;
	uint8_t codepoint = 0;

	if(text == 0)
	{
		return length;
	}

	text_length = strlen(text);
	if(text_length == 0)
	{
		return length;
	}

	while(*text != 0)
	{
		codepoint = (uint8_t)*text;

		if(codepoint == 0)
		{
			break;
		}
		else if(
			(codepoint & 0x80) == 0 ||    /* ASCII */
			(codepoint & 0xC0) != 0xC0 || /* Malformed continuation byte */
			(codepoint & 0xFE) == 0xFE    /* Illegal byte */
		)
		{
			codepoint_length = 1;
		}
		else if((codepoint & 0xE0) == 0xC0)
		{
			codepoint_length = 2;
		}
		else if((codepoint & 0xF0) == 0xE0)
		{
			codepoint_length = 3;
		}
		else if((codepoint & 0xF8) == 0xF0)
		{
			codepoint_length = 4;
		}
		else if((codepoint & 0xFC) == 0xF8)
		{
			codepoint_length = 5;
		}
		else // (codepoint & 0xFE) == 0xFC
		{
			codepoint_length = 6;
		}

		length++;

		/* Check if encoding is valid */

		src = text + 1;

		for(src_index = 1; src_index < codepoint_length; ++src_index)
		{
			if((*src & 0x80) == 0)
			{
				/* Not a continuation byte for a multi-byte sequence */

				break;
			}

			src++;
		}

		if(src_index > text_length)
		{
			break;
		}

		text += src_index;
		text_length -= src_index;
	}

	return length;
}

size_t utf16toutf8(const utf16_t* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors)
{
	size_t encoded_length = 0;
	utf16_t surrogate_low;
	utf16_t current;
	unicode_t codepoint;
	const utf16_t* src = (const utf16_t*)input;
	size_t src_size = inputSize;
	char* dst = target;
	size_t dst_size = targetSize;
	size_t bytes_written = 0;

	if(input == 0 || inputSize < sizeof(utf16_t))
	{
		if(errors != 0)
		{
			*errors = UTF8_ERR_INVALID_DATA;
		}
		return bytes_written;
	}

	while(src_size > 0)
	{
		current = *src;

		if(current == 0)
		{
			break;
		}
		else if(current >= SURROGATE_HIGH_START && current <= SURROGATE_LOW_END)
		{
			if(current > SURROGATE_HIGH_END)
			{
				if(errors != 0)
				{
					*errors = UTF8_ERR_UNMATCHED_HIGH_SURROGATE_PAIR;
				}
				return bytes_written;
			}

			if(src_size < sizeof(utf16_t))
			{
				if(errors != 0)
				{
					*errors = UTF8_ERR_INVALID_DATA;
				}
				return bytes_written;
			}

			src++;
			src_size -= sizeof(utf16_t);

			surrogate_low = *src;

			if(surrogate_low < SURROGATE_LOW_START || surrogate_low > SURROGATE_LOW_END)
			{
				if(errors != 0)
				{
					*errors = UTF8_ERR_UNMATCHED_LOW_SURROGATE_PAIR;
				}
				return bytes_written;
			}

			codepoint =
				0x10000 +
				(surrogate_low - SURROGATE_LOW_START) +
				((current - SURROGATE_HIGH_START) << 10);
		}
		else
		{
			codepoint = (unicode_t)current;
		}

		encoded_length = writecodepoint(codepoint, &dst, &dst_size, errors);
		if(encoded_length == 0)
		{
			return bytes_written;
		}

		src++;
		src_size -= sizeof(utf16_t);

		bytes_written += encoded_length;
	}

	return bytes_written;
}

size_t utf32toutf8(const unicode_t* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors)
{
	unicode_t codepoint;
	size_t encoded_length;
	utf16_t surrogate_low;
	const unicode_t* src = (const unicode_t*)input;
	size_t src_size = inputSize;
	char* dst = target;
	size_t dst_size = targetSize;
	size_t bytes_written = 0;

	if(input == 0 || inputSize < sizeof(unicode_t))
	{
		if(errors != 0)
		{
			*errors = UTF8_ERR_INVALID_DATA;
		}
		return bytes_written;
	}

	while(src_size > 0)
	{
		codepoint = *src;

		if(codepoint == 0)
		{
			break;
		}
		else if(codepoint >= SURROGATE_HIGH_START && codepoint <= SURROGATE_LOW_END)
		{
			if(codepoint > SURROGATE_HIGH_END)
			{
				if(errors != 0)
				{
					*errors = UTF8_ERR_UNMATCHED_HIGH_SURROGATE_PAIR;
				}
				return bytes_written;
			}

			if(src_size < sizeof(unicode_t))
			{
				if(errors != 0)
				{
					*errors = UTF8_ERR_INVALID_DATA;
				}
				return bytes_written;
			}

			src++;
			src_size -= sizeof(unicode_t);

			surrogate_low = *src;

			if(surrogate_low < SURROGATE_LOW_START || surrogate_low > SURROGATE_LOW_END)
			{
				if(errors != 0)
				{
					*errors = UTF8_ERR_UNMATCHED_LOW_SURROGATE_PAIR;
				}
				return bytes_written;
			}

			codepoint =	0x10000 + (surrogate_low - SURROGATE_LOW_START)
								+ ((codepoint - SURROGATE_HIGH_START) << 10);
		}

		encoded_length = writecodepoint(codepoint, &dst, &dst_size, errors);
		if(encoded_length == 0)
		{
			return bytes_written;
		}

		src++;
		src_size -= sizeof(unicode_t);

		bytes_written += encoded_length;
	}

	return bytes_written;
}

size_t widetoutf8(const wchar_t* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors)
{
	#if UTF8_WCHAR_UTF16
	{
		return utf16toutf8((const utf16_t*)input, inputSize, target, targetSize, errors);
	}
	#elif UTF8_WCHAR_UTF32
	{
		return utf32toutf8((const unicode_t*)input, inputSize, target, targetSize, errors);
	}
	#else
	{
		return SIZE_MAX;
	}
	#endif
}

size_t utf8toutf16(const char* input, size_t inputSize, utf16_t* target, size_t targetSize, int32_t* errors)
{
	size_t bytes_written = 0;
	size_t decoded_length;
	unicode_t codepoint;
	const char* src = input;
	size_t src_length = inputSize;
	utf16_t* dst = target;
	size_t dst_size = targetSize;

	if(target != 0 && targetSize < sizeof(utf16_t))
	{
		if(errors != 0)
		{
			*errors = UTF8_ERR_NOT_ENOUGH_SPACE;
		}
		return bytes_written;
	}

	if(input == 0 || inputSize == 0)
	{
		if(errors != 0)
		{
			*errors = UTF8_ERR_INVALID_DATA;
		}
		return bytes_written;
	}

	while(src_length > 0)
	{
		decoded_length = readcodepoint(&codepoint, src, src_length);

		if(codepoint <= MAX_BASIC_MULTILINGUAL_PLANE)
		{
			if(dst != 0)
			{
				if(dst_size < sizeof(utf16_t))
				{
					if(errors != 0)
					{
						*errors = UTF8_ERR_NOT_ENOUGH_SPACE;
					}
					return bytes_written;
				}

				if(codepoint >= SURROGATE_HIGH_START && codepoint <= SURROGATE_LOW_END)
				{
					*dst++ = REPLACEMENT_CHARACTER;
				}
				else
				{
					*dst++ = (utf16_t)codepoint;
				}

				dst_size -= sizeof(utf16_t);
			}

			bytes_written += sizeof(utf16_t);
		}
		else
		{
			if(dst != 0)
			{
				/* Codepoint must be converted to a surrogate pair. */

				if(dst_size < sizeof(unicode_t))
				{
					if(errors != 0)
					{
						*errors = UTF8_ERR_NOT_ENOUGH_SPACE;
					}
					return bytes_written;
				}

				codepoint -= 0x10000;
				*dst++ = (codepoint >> 10) + SURROGATE_HIGH_START;
				*dst++ = (codepoint & 0x3FF) + SURROGATE_LOW_START;

				dst_size -= sizeof(unicode_t);
			}

			bytes_written += sizeof(unicode_t);
		}

		src += decoded_length;
		src_length -= decoded_length;
	}

	return bytes_written;
}

size_t utf8toutf32(const char* input, size_t inputSize, unicode_t* target, size_t targetSize, int32_t* errors)
{
	size_t bytes_written = 0;
	size_t decoded_length;
	unicode_t codepoint;
	const char* src = input;
	size_t src_length = inputSize;
	unicode_t* dst = target;
	size_t dst_size = targetSize;

	if(target != 0 && targetSize < sizeof(unicode_t))
	{
		if(errors != 0)
		{
			*errors = UTF8_ERR_NOT_ENOUGH_SPACE;
		}
		return bytes_written;
	}

	if(input == 0 || inputSize == 0)
	{
		if(errors != 0)
		{
			*errors = UTF8_ERR_INVALID_DATA;
		}
		return bytes_written;
	}

	while(src_length > 0)
	{
		decoded_length = readcodepoint(&codepoint, src, src_length);

		if(dst != 0)
		{
			if(dst_size < sizeof(unicode_t))
			{
				if(errors != 0)
				{
					*errors = UTF8_ERR_NOT_ENOUGH_SPACE;
				}
				return bytes_written;
			}

			*dst++ = codepoint;
			dst_size -= sizeof(unicode_t);
		}

		bytes_written += sizeof(unicode_t);

		src += decoded_length;
		src_length -= decoded_length;
	}

	return bytes_written;
}

size_t utf8towide(const char* input, size_t inputSize, wchar_t* target, size_t targetSize, int32_t* errors)
{
	#if UTF8_WCHAR_UTF16
	{
		return utf8toutf16(input, inputSize, (utf16_t*)target, targetSize, errors);
	}
	#elif UTF8_WCHAR_UTF32
	{
		return utf8toutf32(input, inputSize, (unicode_t*)target, targetSize, errors);
	}
	#else
	{
		return SIZE_MAX;
	}
	#endif
}

static const char* seekforward(const char* src, const char* srcEnd, size_t srcLength, off_t offset)
{
	if(srcEnd <= src || offset <= 0 || srcLength == 0)
	{
		return src;
	}
	if(offset >= (off_t)srcLength)
	{
		return srcEnd;
	}

	do
	{
		size_t codepoint_length = lengthcodepoint(*src);
		if(codepoint_length == 0)
			codepoint_length = 1;

		src++;
		srcLength--;

		for(size_t i = 1; i < codepoint_length && srcLength > 0; ++i)
		{
			if((*src & 0x80) == 0			/* Not a continuation byte */
				|| (*src & 0xC0) == 0xC0	/* Start of a new sequence */
			)
			{
				break;
			}

			src++;
			srcLength--;
		}
	}
	while(--offset > 0 && srcLength > 0);

	return src;
}

static const char* seekrewind(const char* srcStart, const char* src, size_t srcLength, off_t offset)
{
	if(srcStart >= src || offset >= 0 || srcLength == 0)
	{
		return src;
	}
	if(-offset >= (off_t) srcLength)
	{
		return srcStart;
	}

	/* Ignore NUL codepoint */
	src--;
	while(src != srcStart)
	{
		if((*src & 0x80) == 0				/* ASCII */
			|| (*src & 0xC0) != 0x80		/* Malformed continuation byte */
			|| (*src & 0xFE) == 0xFE)		/* Illegal byte */
		{
			offset++;
			if(offset == 0)
				break;
		}

		src--;
	}

	return src;
}

const char* utf8seek(const char* text, const char* textStart, off_t offset, int direction)
{
	size_t textLength = strlen(textStart);
	const char* textEnd = textStart + textLength;

	switch(direction)
	{
		case SEEK_CUR:
			if(offset == 0)
			{
				return text;
			}
			else if(offset > 0)
			{
				return seekforward(text, textEnd, textLength, offset);
			}
			else
			{
				return seekrewind(textStart, text, textLength, offset);
			}

		case SEEK_SET:
			if(text < textStart)
			{
				return text;
			}
			return seekforward(textStart, textEnd, textLength, offset);

		case SEEK_END:
			return seekrewind(textStart, textEnd, textLength, -offset);

		default:
			return text;

	}
}
