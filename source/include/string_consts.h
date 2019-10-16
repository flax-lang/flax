// string_consts.h
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

// is this too many levels of nesting?
namespace strs
{
	namespace attrs
	{
		inline constexpr auto COMPILER_SUPPORT      = "@compiler_support";
	}

	namespace names
	{
		namespace saa
		{
			inline constexpr auto FN_APPEND         = "append";
			inline constexpr auto FN_CLONE          = "clone";

			inline constexpr auto FIELD_LENGTH      = "length";
			inline constexpr auto FIELD_POINTER     = "ptr";
			inline constexpr auto FIELD_REFCOUNT    = "refcount";
			inline constexpr auto FIELD_CAPACITY    = "capacity";
		}

		namespace range
		{
			inline constexpr auto FIELD_BEGIN       = "begin";
			inline constexpr auto FIELD_END         = "end";
			inline constexpr auto FIELD_STEP        = "step";
		}

		// obviously cos enum is a keyword
		namespace enumeration
		{
			inline constexpr auto FIELD_VALUE       = "value";
			inline constexpr auto FIELD_INDEX       = "index";
			inline constexpr auto FIELD_NAME        = "name";
		}

		namespace string
		{
			inline constexpr auto FIELD_COUNT       = "count";
		}

		namespace array
		{
			inline constexpr auto FN_POP            = "pop";
		}

		namespace any
		{
			inline constexpr auto FIELD_TYPEID      = "id";
			inline constexpr auto FIELD_REFCOUNT    = "refcount";
		}

		namespace support
		{
			inline constexpr auto RAII_TRAIT_DROP       = "raii_trait::drop";
			inline constexpr auto RAII_TRAIT_COPY       = "raii_trait::copy";
			inline constexpr auto RAII_TRAIT_MOVE       = "raii_trait::move";
		}

		inline constexpr auto GLOBAL_INIT_FUNCTION      = "global_init_function__";
	}
}








