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

#include "casemapping.h"

#include "base.h"
#include "codepoint.h"
#include "database.h"

uint8_t casemapping_initialize(
	CaseMappingState* state,
	const char* input, size_t inputSize,
	char* target, size_t targetSize,
	const uint32_t* propertyIndex1, const uint32_t* propertyIndex2, const uint32_t* propertyData)
{
	memset(state, 0, sizeof(CaseMappingState));

	state->src = input;
	state->src_size = inputSize;
	state->dst = target;
	state->dst_size = targetSize;
	state->property_index1 = propertyIndex1;
	state->property_index2 = propertyIndex2;
	state->property_data = propertyData;

	return 1;
}

size_t casemapping_execute(CaseMappingState* state)
{
	unicode_t decoded;
	uint8_t decoded_size;
	size_t written = 0;

	if (state->src_size == 0)
	{
		return 0;
	}

	if ((*state->src & 0x80) == 0)
	{
		/* Basic Latin does not have to be converted to UTF-32 */

		decoded = (unicode_t)*state->src;
		decoded_size = 1;

		/* Store codepoint's general category */

		state->last_general_category = PROPERTY_GET_GC(decoded);

		/* Write decomposition */

		if (state->dst != 0)
		{
			if (state->dst_size < 1)
			{
				goto outofspace;
			}

			/* Lowercase letters are U+0061 ('a') to U+007A ('z') */
			/* Uppercase letters are U+0041 ('A') to U+005A ('Z') */
			/* All other codepoints in Basic Latin are unaffected by case mapping */

			if (state->property_data == LowercaseDataPtr)
			{
				*state->dst =
					(*state->src >= 0x41 && *state->src <= 0x5A)
					? *state->src + 0x20
					: *state->src;
			}
			else
			{
				*state->dst =
					(*state->src >= 0x61 && *state->src <= 0x7A)
					? *state->src - 0x20
					: *state->src;
			}

			state->dst++;
			state->dst_size--;
		}

		written++;
	}
	else
	{
		uint8_t resolved_size = 0;

		/* Decode current codepoint */

		decoded_size = codepoint_read(state->src, state->src_size, &decoded);

		/* Check if the codepoint's general category property indicates case mapping */

		state->last_general_category = PROPERTY_GET_GC(decoded);
		if ((state->last_general_category & GeneralCategory_CaseMapped) != 0)
		{
			/* Resolve the codepoint's decomposition */

			const char* resolved = database_querydecomposition(
				decoded,
				state->property_index1, state->property_index2, state->property_data,
				&resolved_size);

			if (resolved != 0)
			{
				/* Copy the decomposition to the output buffer */

				if (state->dst != 0 &&
					resolved_size > 0)
				{
					if (state->dst_size < resolved_size)
					{
						goto outofspace;
					}

					memcpy(state->dst, resolved, resolved_size);

					state->dst += resolved_size;
					state->dst_size -= resolved_size;
				}

				written += resolved_size;
			}
		}

		/* Check if codepoint was unaffected */

		if (resolved_size == 0)
		{
			/* Write the codepoint to the output buffer */
			/* This ensures that invalid codepoints in the input are always converted to U+FFFD in the output */

			uint8_t decoded_written = codepoint_write(decoded, &state->dst, &state->dst_size);
			if (decoded_written == 0)
			{
				goto outofspace;
			}

			written += decoded_written;
		}
	}

	/* Invalid codepoints can be longer than the source length indicates */

	if (state->src_size >= decoded_size)
	{
		state->src += decoded_size;
		state->src_size -= decoded_size;
	}
	else
	{
		state->src_size = 0;
	}

	return written;

outofspace:
	state->src_size = 0;

	return 0;
}