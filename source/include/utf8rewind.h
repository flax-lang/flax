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

/*!
	@file utf8rewind.h
	@brief Functions for working with UTF-8 encoded text.
*/

#ifndef _UTF8REWIND_H_
#define _UTF8REWIND_H_

/// @cond IGNORE
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
/// @endcond

#define UTF8_ERR_INVALID_DATA (-1)
#define UTF8_ERR_NOT_ENOUGH_SPACE (-2)
#define UTF8_ERR_UNMATCHED_HIGH_SURROGATE_PAIR (-3)
#define UTF8_ERR_UNMATCHED_LOW_SURROGATE_PAIR (-4)

//! @defgroup configuration Global configuration
//! @{

#ifndef UTF8_WCHAR_SIZE
	#if (__SIZEOF_WCHAR_T__ == 4) || (WCHAR_MAX > UINT16_MAX) || (__WCHAR_MAX__ > UINT16_MAX)
		#define UTF8_WCHAR_SIZE (4)
	#else
		#define UTF8_WCHAR_SIZE (2)
	#endif
#endif

#if (UTF8_WCHAR_SIZE == 4)
	#define UTF8_WCHAR_UTF32 (1)
	#define UTF8_WCHAR_UTF16 (0)
#elif (UTF8_WCHAR_SIZE == 2)
	#define UTF8_WCHAR_UTF32 (0)
	#define UTF8_WCHAR_UTF16 (1)
#else
	#error Invalid size for wchar_t type.
#endif

//! @}

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint16_t utf16_t; /*!< UTF-16 encoded codepoint. */
typedef uint32_t unicode_t; /*!< Unicode codepoint. */

//! Get the length in codepoints of a UTF-8 encoded string.
/*!
	Example:

	@code{.c}
		int8_t CheckPassword(const char* password)
		{
			size_t length = utf8len(password);
			return (length == utf8len("hunter2"));
		}
	@endcode

	@param[in]  text  UTF-8 encoded string.

	@return Length in codepoints.
*/
size_t utf8len(const char* text);

//! Convert a UTF-16 encoded string to a UTF-8 encoded string.
/*!
	@note This function should only be called directly if you are positive
	that you're working with UTF-16 encoded text. If you're working
	with wide strings, take a look at widetoutf8() instead.

	Example:

	@code{.c}
		int8_t Player_SetName(const utf16_t* name, size_t nameSize)
		{
			int32_t errors = 0;
			char converted_name[256] = { 0 };
			utf16toutf8(name, nameSize, converted_name, 256, &errors);
			if (errors != 0)
			{
				return 0;
			}

			return Player_SetName(converted_name);
		}
	@endcode

	@param[in]   input       UTF-16 encoded string.
	@param[in]   inputSize   Size of the input in bytes.
	@param[out]  target      Output buffer for the result.
	@param[in]   targetSize  Size of the output buffer in bytes.
	@param[out]  errors      Output for errors.

	@return Bytes written or amount of bytes needed for output
	if target buffer is specified as NULL.

	@retval #UTF8_ERR_INVALID_DATA                   Input does not contain enough bytes for encoding.
	@retval #UTF8_ERR_UNMATCHED_HIGH_SURROGATE_PAIR  High surrogate pair was not matched.
	@retval #UTF8_ERR_UNMATCHED_LOW_SURROGATE_PAIR   Low surrogate pair was not matched.
	@retval #UTF8_ERR_NOT_ENOUGH_SPACE               Target buffer could not contain result.

	@sa utf32toutf8
	@sa widetoutf8
*/
size_t utf16toutf8(const utf16_t* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors);

//! Convert a UTF-32 encoded string to a UTF-8 encoded string.
/*!
	@note This function should only be called directly if you are positive
	that you're working with UTF-32 encoded text. If you're working
	with wide strings, take a look at widetoutf8() instead.

	Example:

	@code{.c}
		int8_t Database_ExecuteQuery(const unicode_t* query, size_t querySize)
		{
			int32_t errors = 0;
			char* converted = 0;
			int8_t result = 0;
			size_t converted_size = utf32toutf8(query, querySize, 0, 0, &errors);
			if (errors != 0)
			{
				goto cleanup;
			}

			converted = (char*)malloc(converted_size + 1);
			memset(converted, 0, converted_size + 1);

			utf32toutf8(query, querySize, converted, converted_size, &errors);
			if (errors != 0)
			{
				goto cleanup;
			}

			result = Database_ExecuteQuery(converted);

		cleanup:
			if (converted != 0)
			{
				free(converted);
				converted = 0;
			}
			return result;
		}
	@endcode

	@param[in]   input       UTF-32 encoded string.
	@param[in]   inputSize   Size of the input in bytes.
	@param[out]  target      Output buffer for the result.
	@param[in]   targetSize  Size of the output buffer in bytes.
	@param[out]  errors      Output for errors.

	@return Bytes written or amount of bytes needed for output
	if target buffer is specified as NULL.

	@retval #UTF8_ERR_INVALID_DATA                   Input does not contain enough bytes for encoding.
	@retval #UTF8_ERR_UNMATCHED_HIGH_SURROGATE_PAIR  High surrogate pair was not matched.
	@retval #UTF8_ERR_UNMATCHED_LOW_SURROGATE_PAIR   Low surrogate pair was not matched.
	@retval #UTF8_ERR_NOT_ENOUGH_SPACE               Target buffer could not contain result.

	@sa utf16toutf8
	@sa widetoutf8
*/
size_t utf32toutf8(const unicode_t* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors);

//! Convert a wide string to a UTF-8 encoded string.
/*!
	Depending on the platform, wide strings are either UTF-16
	or UTF-32 encoded. This function takes a wide string as
	input and automatically calls the correct conversion
	function.

	This allows for a cross-platform treatment of wide text and
	is preferable to using the UTF-16 or UTF-32 versions
	directly.

	Example:

	@code{.c}
		const wchar_t* input = L"textures/\xD803\xDC11.png";
		size_t input_size = wcslen(input) * sizeof(wchar_t);
		size_t output_size = 0;
		char* output = 0;
		size_t result = 0;
		int32_t errors = 0;

		result = widetoutf8(input, input_size, 0, 0, &errors);
		if (errors == 0)
		{
			output_size = result + 1;

			output = (char*)malloc(output_size);
			memset(output, 0, output_size);

			widetoutf8(input, wcslen(input) * sizeof(wchar_t), output, output_size, &errors);
			if (errors == 0)
			{
				Texture_Load(output);
			}

			free(output);
		}
	@endcode

	@param[in]   input       Wide-encoded string.
	@param[in]   inputSize   Size of the input in bytes.
	@param[out]  target      Output buffer for the result.
	@param[in]   targetSize  Size of the output buffer in bytes.
	@param[out]  errors      Output for errors.

	@return Bytes written or amount of bytes needed for output
	if target buffer is specified as NULL.

	@retval #UTF8_ERR_INVALID_DATA                   Input does not contain enough bytes for encoding.
	@retval #UTF8_ERR_UNMATCHED_HIGH_SURROGATE_PAIR  High surrogate pair was not matched.
	@retval #UTF8_ERR_UNMATCHED_LOW_SURROGATE_PAIR   Low surrogate pair was not matched.
	@retval #UTF8_ERR_NOT_ENOUGH_SPACE               Target buffer could not contain result.

	@sa utf8towide
	@sa utf16toutf8
	@sa utf32toutf8
*/
size_t widetoutf8(const wchar_t* input, size_t inputSize, char* target, size_t targetSize, int32_t* errors);

//! Convert a UTF-8 encoded string to a UTF-16 encoded string.
/*!
	@note This function should only be called directly if you are positive
	that you *must* convert to UTF-16, independent of platform.
	If you're working with wide strings, take a look at utf8towide()
	instead.

	Erroneous byte sequences such as missing bytes, illegal bytes or
	overlong encodings of codepoints are converted to the
	replacement character U+FFFD.

	Example:

	@code{.c}
		void Font_DrawText(int x, int y, const char* text)
		{
			int32_t errors = 0;
			utf16_t converted[256] = { 0 };
			size_t converted_size = utf8toutf16(title, strlen(title), converted, 256 * sizeof(utf16_t), &errors);
			if (errors == 0)
			{
				Legacy_DrawText(g_FontCurrent, x, y, (unsigned short*)converted, converted_size);
			}
		}
	@endcode

	@param[in]   input       UTF-8 encoded string.
	@param[in]   inputSize   Size of the input in bytes.
	@param[out]  target      Output buffer for the result.
	@param[in]   targetSize  Size of the output buffer in bytes.
	@param[out]  errors      Output for errors.

	@return Bytes written or amount of bytes needed for output
	if target buffer is specified as NULL.

	@retval #UTF8_ERR_INVALID_DATA      Input does not contain enough bytes for decoding.
	@retval #UTF8_ERR_NOT_ENOUGH_SPACE  Target buffer could not contain result.

	@sa utf8towide
	@sa utf8toutf32
*/
size_t utf8toutf16(const char* input, size_t inputSize, utf16_t* target, size_t targetSize, int32_t* errors);

//! Convert a UTF-8 encoded string to a UTF-32 encoded string.
/*!
	@note This function should only be called directly if you are positive
	that you *must* convert to UTF-32, independent of platform.
	If you're working with wide strings, take a look at utf8towide()
	instead.

	Erroneous byte sequences such as missing bytes, illegal bytes or
	overlong encodings of codepoints are converted to the
	replacement character U+FFFD.

	Example:

	@code{.c}
		void TextField_AddCharacter(const char* encoded)
		{
			int32_t errors = 0;
			unicode_t codepoint = 0;
			utf8toutf32(encoded, strlen(encoded), &codepoint, sizeof(unicode_t), &errors);
			if (errors == 0)
			{
				TextField_AddCodepoint(codepoint);
			}
		}
	@endcode

	@param[in]   input       UTF-8 encoded string.
	@param[in]   inputSize   Size of the input in bytes.
	@param[out]  target      Output buffer for the result.
	@param[in]   targetSize  Size of the output buffer in bytes.
	@param[out]  errors      Output for errors.

	@return Bytes written or amount of bytes needed for output
	if target buffer is specified as NULL.

	@retval #UTF8_ERR_INVALID_DATA Input does not contain enough bytes for decoding.
	@retval #UTF8_ERR_NOT_ENOUGH_SPACE Target buffer could not contain result.

	@sa utf8towide
	@sa utf8toutf16
*/
size_t utf8toutf32(const char* input, size_t inputSize, unicode_t* target, size_t targetSize, int32_t* errors);

//! Convert a UTF-8 encoded string to a wide string.
/*!
	Depending on the platform, wide strings are either UTF-16
	or UTF-32 encoded. This function takes a UTF-8 encoded
	string as input and automatically calls the correct
	conversion function.

	This allows for a cross-platform treatment of wide text and
	is preferable to using the UTF-16 or UTF-32 versions
	directly.

	Erroneous byte sequences such as missing bytes, illegal bytes or
	overlong encodings of codepoints are converted to the
	replacement character U+FFFD.

	@note Codepoints outside the Basic Multilingual Plane (BMP) are
	converted to surrogate pairs when using UTF-16. This means
	that strings containing characters outside the BMP
	converted on a platform with UTF-32 wide strings are *not*
	compatible with platforms with UTF-16 wide strings.

	@par Hence, it is preferable to keep all data as UTF-8 and only
	convert to wide strings when required by a third-party
	interface.

	Example:

	@code{.c}
		const char* input = "Bj\xC3\xB6rn Zonderland";
		size_t input_size = strlen(input);
		wchar_t* output = 0;
		size_t output_size = 0;
		size_t result = 0;
		int32_t errors = 0;

		output_size = utf8towide(input, input_size, 0, 0, &errors);
		if (errors == 0)
		{
			output = (wchar_t*)malloc(output_size);
			memset(output, 0, output_size);

			utf8towide(input, input_size, output, output_size, &errors);
			if (errors == 0)
			{
				Player_SetName(output);
			}

			free(output);
		}
	@endcode

	@param[in]   input       UTF-8 encoded string.
	@param[in]   inputSize   Size of the input in bytes.
	@param[out]  target      Output buffer for the result.
	@param[in]   targetSize  Size of the output buffer in bytes.
	@param[out]  errors      Output for errors.

	@return Bytes written or amount of bytes needed for output
	if target buffer is specified as NULL.

	@retval #UTF8_ERR_INVALID_DATA Input does not contain enough bytes for decoding.
	@retval #UTF8_ERR_NOT_ENOUGH_SPACE Target buffer could not contain result.

	@sa widetoutf8
	@sa utf8toutf16
	@sa utf8toutf32
*/
size_t utf8towide(const char* input, size_t inputSize, wchar_t* target, size_t targetSize, int32_t* errors);

//! Seek into a UTF-8 encoded string.
/*!
	Working with UTF-8 encoded strings can be tricky due to
	the nature of the variable-length encoding. Because one
	character no longer equals one byte, it can be difficult
	to skip around in a UTF-8 encoded string without
	decoding the codepoints.

	This function provides an interface similar to `fseek`
	in order to enable skipping to another part of the
	string.

	@note `textStart` must come before `text` in memory when
	seeking from the current or end position.

	Example:

	@code{.c}
		const char* text = "Press \xE0\x80\x13 to continue.";
		const char fixed[1024] = { 0 };
		const char* commandStart;
		const char* commandEnd;

		commandStart = strstr(text, "\xE0\x80\x13");
		if (commandStart == 0)
		{
			return 0;
		}

		strncpy(fixed, text, commandStart - text);
		strcat(fixed, "ENTER");

		commandEnd = utf8seek(commandStart, text, 1, SEEK_CUR);
		if (commandEnd != commandStart)
		{
			strcat(fixed, commandEnd);
		}
	@endcode

	@param[in]  text       Input string.
	@param[in]  textStart  Start of input string.
	@param[in]  offset     Requested offset in codepoints.
	@param[in]  direction  Direction to seek in.
	@arg `SEEK_SET` Offset is from the start of the string.
	@arg `SEEK_CUR` Offset is from the current position of the string.
	@arg `SEEK_END` Offset is from the end of the string.

	@return Changed string or no change on error.
*/
const char* utf8seek(const char* text, const char* textStart, off_t offset, int direction);

#if defined(__cplusplus)
}
#endif

#endif
