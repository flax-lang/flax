/*
	Copyright (C) 2014-2015 Quinten Lansu

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

#include "utf8rewind.h"

#include "internal/base.h"
#include "internal/casemapping.h"
#include "internal/codepoint.h"
#include "internal/composition.h"
#include "internal/decomposition.h"
#include "internal/database.h"
#include "internal/seeking.h"
#include "internal/streaming.h"

size_t utf8len(const char* text)
{
	const uint8_t* src;
	size_t src_length;
	size_t length;

	/* Validate input */

	if (text == 0 ||
		text[0] == 0)
	{
		return 0;
	}

	length = 0;

	/* Determine length in codepoints */

	src = (const uint8_t*)text;
	src_length = strlen(text);

	while (src_length > 0)
	{
		uint8_t src_offset = 1;

		/* Check if the current byte is part of a multi-byte sequence */

		uint8_t codepoint_length = codepoint_decoded_length[*src];
		if (codepoint_length > 1)
		{
			/* Check every byte of the sequence */

			do
			{
				if (src[src_offset] < 0x80 ||  /* Not a continuation byte */
					src[src_offset] > 0xBF)    /* Start of a new sequence */
				{
					break;
				}
			}
			while (++src_offset < codepoint_length);
		}

		/* Found a codepoint */

		length++;

		/* Move cursor */

		if (src_offset >= src_length)
		{
			break;
		}

		src += src_offset;
		src_length -= src_offset;
	}

	return length;
}

size_t utf16toutf8(const utf16_t* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors)
{
	const utf16_t* src;
	size_t src_size;
	char* dst;
	size_t dst_size;
	size_t bytes_written = 0;

	/* Validate parameters */

	UTF8_VALIDATE_PARAMETERS_CHAR(utf16_t, bytes_written);
	UTF8_SET_ERROR(NONE);

	/* Setup cursors */

	src = input;
	src_size = inputSize;
	dst = target;
	dst_size = targetSize;

	/* Loop over input */

	while (src_size > 0)
	{
		unicode_t codepoint;
		uint8_t encoded_size;

		if (src_size < sizeof(utf16_t))
		{
			/* Not enough data */

			goto invaliddata;
		}

		codepoint = (unicode_t)*src;

		if (codepoint >= SURROGATE_HIGH_START &&
			codepoint <= SURROGATE_LOW_END)
		{
			/* Decode surrogate pair */

			if (codepoint > SURROGATE_HIGH_END)
			{
				/* Missing high surrogate codepoint */

				codepoint = REPLACEMENT_CHARACTER;

				UTF8_SET_ERROR(INVALID_DATA);
			}
			else if (
				src_size < 2 * sizeof(utf16_t))
			{
				/* Not enough data */

				goto invaliddata;
			}
			else
			{
				/* Read low surrogate codepoint */

				if (src[1] < SURROGATE_LOW_START ||
					src[1] > SURROGATE_LOW_END)
				{
					/* Missing low surrogate codepoint */

					codepoint = REPLACEMENT_CHARACTER;

					UTF8_SET_ERROR(INVALID_DATA);
				}
				else
				{
					/* Decode codepoint from surrogate pair */

					codepoint =
						(MAX_BASIC_MULTILINGUAL_PLANE + 1) +
						(src[1] - SURROGATE_LOW_START) +
						((src[0] - SURROGATE_HIGH_START) << 10);

					src++;
					src_size -= sizeof(utf16_t);
				}
			}
		}

		encoded_size = codepoint_write(codepoint, &dst, &dst_size);
		if (encoded_size == 0)
		{
			UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

			return bytes_written;
		}

		bytes_written += encoded_size;

		src++;
		src_size -= sizeof(utf16_t);
	}

	return bytes_written;

invaliddata:
	if (dst != 0)
	{
		if (dst_size < REPLACEMENT_CHARACTER_STRING_LENGTH)
		{
			UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

			return bytes_written;
		}

		/* Write replacement codepoint to output */

		memcpy(dst, REPLACEMENT_CHARACTER_STRING, REPLACEMENT_CHARACTER_STRING_LENGTH);
	}

	UTF8_SET_ERROR(INVALID_DATA);

	return bytes_written + REPLACEMENT_CHARACTER_STRING_LENGTH;
}

size_t utf32toutf8(const unicode_t* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors)
{
	const unicode_t* src;
	size_t src_size;
	char* dst;
	size_t dst_size;
	size_t bytes_written = 0;

	/* Validate parameters */

	UTF8_VALIDATE_PARAMETERS_CHAR(unicode_t, bytes_written);
	UTF8_SET_ERROR(NONE);

	/* Setup cursors */

	src = input;
	src_size = inputSize;
	dst = target;
	dst_size = targetSize;

	/* Loop over input */

	while (src_size > 0)
	{
		unicode_t codepoint;
		uint8_t encoded_size;

		if (src_size < sizeof(unicode_t))
		{
			/* Not enough data */

			goto invaliddata;
		}

		codepoint = *src;

		if (codepoint >= SURROGATE_HIGH_START &&
			codepoint <= SURROGATE_LOW_END)
		{
			/* Decode surrogate pair */

			if (codepoint > SURROGATE_HIGH_END) 
			{
				/* Missing high surrogate codepoint */

				codepoint = REPLACEMENT_CHARACTER;

				UTF8_SET_ERROR(INVALID_DATA);
			}
			else if (
				src_size < 2 * sizeof(unicode_t))
			{
				/* Not enough data */

				goto invaliddata;
			}
			else
			{
				/* Read low surrogate codepoint */

				if (src[1] < SURROGATE_LOW_START ||
					src[1] > SURROGATE_LOW_END)
				{
					/* Missing low surrogate codepoint */

					codepoint = REPLACEMENT_CHARACTER;

					UTF8_SET_ERROR(INVALID_DATA);
				}
				else
				{
					/* Decode codepoint from surrogate pair */

					codepoint =
						(MAX_BASIC_MULTILINGUAL_PLANE + 1) +
						(src[1] - SURROGATE_LOW_START) +
						((src[0] - SURROGATE_HIGH_START) << 10);

					src++;
					src_size -= sizeof(unicode_t);
				}
			}
		}

		encoded_size = codepoint_write(codepoint, &dst, &dst_size);
		if (encoded_size == 0)
		{
			UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

			return bytes_written;
		}

		bytes_written += encoded_size;

		src++;
		src_size -= sizeof(unicode_t);
	}

	return bytes_written;

invaliddata:
	if (dst != 0)
	{
		if (dst_size < REPLACEMENT_CHARACTER_STRING_LENGTH)
		{
			UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

			return bytes_written;
		}

		/* Write replacement codepoint to output */

		memcpy(dst, REPLACEMENT_CHARACTER_STRING, REPLACEMENT_CHARACTER_STRING_LENGTH);
	}

	UTF8_SET_ERROR(INVALID_DATA);

	return bytes_written + REPLACEMENT_CHARACTER_STRING_LENGTH;
}

size_t widetoutf8(const wchar_t* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors)
{
#if UTF8_WCHAR_UTF16
	return utf16toutf8((const utf16_t*)input, inputSize, target, targetSize, errors);
#elif UTF8_WCHAR_UTF32
	return utf32toutf8((const unicode_t*)input, inputSize, target, targetSize, errors);
#else
	return SIZE_MAX;
#endif
}

size_t utf8toutf16(const char* input, size_t inputSize, utf16_t* target, size_t targetSize, int32_t* errors)
{
	const char* src;
	size_t src_size;
	utf16_t* dst;
	size_t dst_size;
	size_t bytes_written = 0;

	/* Validate parameters */

	UTF8_VALIDATE_PARAMETERS(char, utf16_t, bytes_written);

	/* Setup cursors */

	src = input;
	src_size = inputSize;
	dst = target;
	dst_size = targetSize;

	/* Loop over input */

	while (src_size > 0)
	{
		unicode_t decoded;
		uint8_t decoded_size = codepoint_read(src, src_size, &decoded);

		if (decoded <= MAX_BASIC_MULTILINGUAL_PLANE)
		{
			/* Codepoint fits in a single UTF-16 codepoint */

			if (dst != 0)
			{
				/* Write to output */

				if (dst_size < sizeof(utf16_t))
				{
					UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

					return bytes_written;
				}

				*dst++ = (utf16_t)decoded;
				dst_size -= sizeof(utf16_t);
			}

			bytes_written += sizeof(utf16_t);
		}
		else
		{
			/* Codepoint must be encoded using a surrogate pair */

			if (dst != 0)
			{
				/* Write to output */

				if (dst_size < 2 * sizeof(utf16_t))
				{
					UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

					return bytes_written;
				}

				/* Encoded value is always beyond BMP */

				decoded -= (MAX_BASIC_MULTILINGUAL_PLANE + 1);
				*dst++ = SURROGATE_HIGH_START + (decoded >> 10);
				*dst++ = SURROGATE_LOW_START + (decoded & 0x03FF);

				dst_size -= 2 * sizeof(utf16_t);
			}

			bytes_written += 2 * sizeof(utf16_t);
		}

		src += decoded_size;
		src_size -= decoded_size;
	}

	UTF8_SET_ERROR(NONE);

	return bytes_written;
}

size_t utf8toutf32(const char* input, size_t inputSize, unicode_t* target, size_t targetSize, int32_t* errors)
{
	const char* src;
	size_t src_size;
	unicode_t* dst;
	size_t dst_size;
	size_t bytes_written = 0;

	/* Validate parameters */

	UTF8_VALIDATE_PARAMETERS(char, unicode_t, bytes_written);

	/* Setup cursors */

	src = input;
	src_size = inputSize;
	dst = target;
	dst_size = targetSize;

	/* Loop over input */

	while (src_size > 0)
	{
		unicode_t decoded;
		uint8_t decoded_length = codepoint_read(src, src_size, &decoded);

		if (dst != 0)
		{
			/* Write to output */

			if (dst_size < sizeof(unicode_t))
			{
				UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

				return bytes_written;
			}

			*dst++ = decoded;
			dst_size -= sizeof(unicode_t);
		}

		bytes_written += sizeof(unicode_t);

		src += decoded_length;
		src_size -= decoded_length;
	}

	UTF8_SET_ERROR(NONE);

	return bytes_written;
}

size_t utf8towide(const char* input, size_t inputSize, wchar_t* target, size_t targetSize, int32_t* errors)
{
#if UTF8_WCHAR_UTF16
	return utf8toutf16(input, inputSize, (utf16_t*)target, targetSize, errors);
#elif UTF8_WCHAR_UTF32
	return utf8toutf32(input, inputSize, (unicode_t*)target, targetSize, errors);
#else
	return SIZE_MAX;
#endif
}

const char* utf8seek(const char* text, size_t textSize, const char* textStart, off_t offset, int direction)
{
	const char* text_end;

	if (text == 0 ||
		textStart == 0)
	{
		return text;
	}

	text_end = textStart + textSize;

	switch (direction)
	{

	case SEEK_CUR:
		{
			if (offset == 0)
			{
				return text;
			}
			else if (offset > 0)
			{
				return seeking_forward(text, text_end, textSize, offset);
			}
			else
			{
				return seeking_rewind(textStart, text, textSize, offset);
			}

		} break;

	case SEEK_SET:
		{
			if (text < textStart)
			{
				return text;
			}

			return seeking_forward(textStart, text_end, textSize, offset);

		} break;

	case SEEK_END:
		return seeking_rewind(textStart, text_end, textSize, -offset);

	default:
		return text;

	}
}

size_t utf8toupper(const char* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors)
{
	CaseMappingState state;
	size_t bytes_written = 0;

	/* Validate parameters */

	UTF8_VALIDATE_PARAMETERS_CHAR(char, bytes_written);

	/* Initialize case mapping */

	if (!casemapping_initialize(
		&state,
		input, inputSize,
		target, targetSize,
		UppercaseIndex1Ptr, UppercaseIndex2Ptr, UppercaseDataPtr))
	{
		UTF8_SET_ERROR(NONE);

		return bytes_written;
	}

	/* Execute case mapping as long as input remains */

	while (state.src_size > 0)
	{
		size_t result = casemapping_execute(&state);
		if (!result)
		{
			UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

			return bytes_written;
		}

		bytes_written += result;
	}

	UTF8_SET_ERROR(NONE);

	return bytes_written;
}

size_t utf8tolower(const char* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors)
{
	CaseMappingState state;
	size_t bytes_written = 0;

	/* Validate parameters */

	UTF8_VALIDATE_PARAMETERS_CHAR(char, bytes_written);

	/* Initialize case mapping */

	if (!casemapping_initialize(
		&state,
		input, inputSize,
		target, targetSize,
		LowercaseIndex1Ptr, LowercaseIndex2Ptr, LowercaseDataPtr))
	{
		UTF8_SET_ERROR(NONE);

		return bytes_written;
	}

	/* Execute case mapping as long as input remains */

	while (state.src_size > 0)
	{
		size_t result = casemapping_execute(&state);
		if (!result)
		{
			UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

			return bytes_written;
		}

		bytes_written += result;
	}

	UTF8_SET_ERROR(NONE);

	return bytes_written;
}

size_t utf8totitle(const char* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors)
{
	CaseMappingState state;
	size_t bytes_written = 0;

	/* Validate parameters */

	UTF8_VALIDATE_PARAMETERS_CHAR(char, bytes_written);

	/* Initialize case mapping */

	if (!casemapping_initialize(
		&state,
		input, inputSize,
		target, targetSize,
		TitlecaseIndex1Ptr, TitlecaseIndex2Ptr, TitlecaseDataPtr))
	{
		UTF8_SET_ERROR(NONE);

		return bytes_written;
	}

	/* Execute case mapping as long as input remains */

	while (state.src_size > 0)
	{
		size_t result = casemapping_execute(&state);
		if (!result)
		{
			UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

			return bytes_written;
		}

		/* The first letter of every word should be titlecase, the rest lowercase */

		if (state.property_data == TitlecaseDataPtr)
		{
			if ((state.last_general_category & GeneralCategory_Letter) != 0)
			{
				state.property_index1 = LowercaseIndex1Ptr;
				state.property_index2 = LowercaseIndex2Ptr;
				state.property_data = LowercaseDataPtr;
			}
		}
		else if (
			(state.last_general_category & GeneralCategory_Letter) == 0)
		{
			state.property_index1 = TitlecaseIndex1Ptr;
			state.property_index2 = TitlecaseIndex2Ptr;
			state.property_data = TitlecaseDataPtr;
		}

		bytes_written += result;
	}

	UTF8_SET_ERROR(NONE);

	return bytes_written;
}

uint8_t utf8isnormalized(const char* input, size_t inputSize, size_t flags, size_t* offset)
{
	const char* src = input;
	size_t src_size = inputSize;
	uint8_t last_canonical_class = 0;
	size_t found_offset = 0;
	uint8_t result = UTF8_NORMALIZATION_RESULT_YES;
	unicode_t decoded;
	uint8_t canonical_class;
	uint8_t quick_check;
	const size_t* property_index;
	const uint8_t* property_data;

	/* Validate input and flags */

	if (input == NULL ||
		inputSize == 0 ||
		(flags & (UTF8_NORMALIZE_DECOMPOSE | UTF8_NORMALIZE_COMPOSE)) == 0)
	{
		goto end;
	}

	/* Get properties */

	if ((flags & UTF8_NORMALIZE_COMPOSE) != 0)
	{
		if ((flags & UTF8_NORMALIZE_COMPATIBILITY) != 0)
		{
			property_index = QuickCheckNFKCIndexPtr;
			property_data = QuickCheckNFKCDataPtr;
		}
		else
		{
			property_index = QuickCheckNFCIndexPtr;
			property_data = QuickCheckNFCDataPtr;
		}
	}
	else
	{
		if ((flags & UTF8_NORMALIZE_COMPATIBILITY) != 0)
		{
			property_index = QuickCheckNFKDIndexPtr;
			property_data = QuickCheckNFKDDataPtr;
		}
		else
		{
			property_index = QuickCheckNFDIndexPtr;
			property_data = QuickCheckNFDDataPtr;
		}
	}

	/* Process input */

	while (src_size > 0)
	{
		/* Read codepoint at cursor */

		uint8_t read = codepoint_read(src, src_size, &decoded);
		if (read == 0)
		{
			break;
		}

		/* Get canonical combining class and quick check value */

		canonical_class = PROPERTY_GET_CCC(decoded);
		quick_check = PROPERTY_GET(property_index, property_data, decoded);

		/* Compare CCC to previous CCC */

		if (last_canonical_class > canonical_class &&
			canonical_class > 0)
		{
			result = UTF8_NORMALIZATION_RESULT_NO;

			break;
		}

		/* Compare quick check value */

		if (quick_check == QuickCheckResult_No)
		{
			result = UTF8_NORMALIZATION_RESULT_NO;

			break;
		}
		else if (
			quick_check == QuickCheckResult_Maybe)
		{
			result = UTF8_NORMALIZATION_RESULT_MAYBE;
		}

		/* Append to offset */

		if (result != UTF8_NORMALIZATION_RESULT_MAYBE)
		{
			found_offset += read;
		}

		last_canonical_class = canonical_class;

		src += read;
		src_size -= read;
	}

end:
	if (offset != 0)
	{
		*offset = found_offset;
	}
	return result;
}

size_t utf8normalize(const char* input, size_t inputSize, char* target, size_t targetSize, size_t flags, int32_t* errors)
{
	char* dst = target;
	size_t dst_size = targetSize;
	StreamState stream[4];
	DecomposeState decompose_state;
	ComposeState compose_state;
	uint8_t compatibility = (flags & UTF8_NORMALIZE_COMPATIBILITY) != 0;
	StreamState* stream_output;
	uint8_t finished = 0;
	size_t bytes_written = 0;

	/*
		Decomposition uses the following process:

		input         -->  stream[0]  -->
		(decompose)   -->  stream[1]  -->
		(accumulate)  -->  stream[2]  -->
		output

		The accumulation step is necessary in order to prevent buffer overflow
		attacks.

		Composition adds another stream buffer:

		input         --> stream[0]  -->
		(decompose)   --> stream[1]  -->
		(accumulate)  --> stream[2]  -->
		(compose)     --> stream[3]  -->
		output

		Although four streaming buffers may seem excessive, they are necessary
		for preventing allocations on the heap.
	*/

	/* Check for valid flags */

	if ((flags & (UTF8_NORMALIZE_DECOMPOSE | UTF8_NORMALIZE_COMPOSE)) == 0)
	{
		UTF8_SET_ERROR(INVALID_FLAG);

		return bytes_written;
	}

	/* Validate parameters */

	UTF8_VALIDATE_PARAMETERS_CHAR(char, bytes_written);

	/* Initialize decomposition */

	memset(stream, 0, sizeof(stream));

	if (!stream_initialize(&stream[0], input, inputSize) ||
		!decompose_initialize(&decompose_state, &stream[0], &stream[1], compatibility))
	{
		UTF8_SET_ERROR(INVALID_DATA);

		return bytes_written;
	}

	stream_output = &stream[2];

	if ((flags & UTF8_NORMALIZE_COMPOSE) != 0)
	{
		/* Initialize composition */

		if (!compose_initialize(&compose_state, &stream[2], &stream[3], compatibility))
		{
			UTF8_SET_ERROR(INVALID_DATA);

			return bytes_written;
		}

		stream_output = &stream[3];
	}

	do
	{
		uint8_t write = 0;

		/* Accumulate decomposed input in next stream */

		if (stream[1].current > 0)
		{
			unicode_t* src_codepoint = stream[1].codepoint;
			unicode_t* dst_codepoint = stream[2].codepoint + stream[2].filled;
			uint8_t* src_qc = stream[1].quick_check;
			uint8_t* dst_qc = stream[2].quick_check + stream[2].filled;
			uint8_t* src_ccc = stream[1].canonical_combining_class;
			uint8_t* dst_ccc = stream[2].canonical_combining_class + stream[2].filled;

			if ((flags & UTF8_NORMALIZE_COMPOSE) != 0)
			{
				uint8_t i;

				/* Update stream properties to use composition values */

				for (i = 0; i < stream[1].current; ++i)
				{
					*dst_qc++ = PROPERTY_GET(compose_state.qc_index, compose_state.qc_data, *src_codepoint);
					*dst_ccc++ = *src_ccc++;
					*dst_codepoint++ = *src_codepoint++;
				}
			}
			else
			{
				/* Copy directly */

				memcpy(dst_codepoint, src_codepoint, stream[1].current * sizeof(unicode_t));
				memcpy(dst_qc, src_qc, stream[1].current * sizeof(uint8_t));
				memcpy(dst_ccc, src_ccc, stream[1].current * sizeof(uint8_t));
			}

			stream[2].current += stream[1].current;
			stream[2].filled += stream[1].current;
		}

		/* Decompose input sequence into next stream */

		finished = !decompose_execute(&decompose_state);
		if (!finished)
		{
			/* Output current stream it it could overflow accumulation buffer */

			write = (stream[1].current + stream[2].filled) >= STREAM_SAFE_MAX;
		}

		/* Reorder potentially unordered decomposed stream */

		if (!stream[1].stable)
		{
			stream_reorder(&stream[1]);
		}

		/* Write stream to output when overflowing or when accumulation buffer is empty*/

		if (write ||
			finished)
		{
			uint8_t i;

			/* Compose accumulation buffer */

			if ((flags & UTF8_NORMALIZE_COMPOSE) != 0 &&
				!compose_execute(&compose_state))
			{
				break;
			}

			/* Write to output buffer */

			for (i = 0; i < stream_output->current; ++i)
			{
				uint8_t encoded_size = codepoint_write(stream_output->codepoint[i], &dst, &dst_size);
				if (encoded_size == 0)
				{
					UTF8_SET_ERROR(NOT_ENOUGH_SPACE);

					return bytes_written;
				}

				bytes_written += encoded_size;
			}

			/* Reset accumulation buffer */

			stream[2].current = 0;
			stream[2].filled = 0;
		}
	}
	while (!finished);

	UTF8_SET_ERROR(NONE);

	return bytes_written;
}