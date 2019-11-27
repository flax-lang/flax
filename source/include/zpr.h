// zpr.h
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include <stdint.h>
#include <stddef.h>

#include <map>
#include <string>
#include <algorithm>
#include <type_traits>
#include <string_view>

#ifndef ENABLE_FIELD_SIZES
	#define ENABLE_FIELD_SIZES          1
#endif

#ifndef HEX_0X_RESPECTS_UPPERCASE
	#define HEX_0X_RESPECTS_UPPERCASE   1
#endif

namespace zpr
{
	struct format_args
	{
		bool zero_pad = false;
		bool alternate = false;
		bool prepend_plus_if_positive = false;
		bool prepend_blank_if_positive = false;

		char specifier      = 0;

		int64_t width       = 0;
		int64_t length      = 0;
		int64_t precision   = -1;

		constexpr static int LENGTH_DEFAULT     = 0;
		constexpr static int LENGTH_SHORT_SHORT = 1;
		constexpr static int LENGTH_SHORT       = 2;
		constexpr static int LENGTH_LONG        = 3;
		constexpr static int LENGTH_LONG_LONG   = 4;
		constexpr static int LENGTH_LONG_DOUBLE = 5;
		constexpr static int LENGTH_INTMAX_T    = 6;
		constexpr static int LENGTH_SIZE_T      = 7;
		constexpr static int LENGTH_PTRDIFF_T   = 8;
	};

	template <typename T, typename = void>
	struct print_formatter
	{
		template <typename K>
		struct has_formatter { static constexpr bool value = false; };

		// when printing, we use print_formatter<T>().print(...). if there is no specialisation
		// for print_formatter<T>, then we will instantiate this base class -- which causes the nice
		// static_assert message. note that we must use some predicate that depends on T, so that the
		// compiler can only know the value when it tries to instantiate. using static_assert(false)
		// will always fail to compile.

		// we make a inner type has_formatter which is more descriptive, and since we only make this
		// error when we try to instantiate the base, any specialisations don't even need to care!
		static_assert(true || has_formatter<T>::value, "no formatter defined for type!");
	};




	namespace _internal
	{
		inline format_args parseFormatArgs(const char* fmt, const char** end, bool* need_width, bool* need_prec)
		{
			auto ret = format_args();
			if(fmt[0] != '%')
				return ret;

			fmt++;

			bool negative_width = false;
			while(true)
			{
				switch(*fmt++)
				{
					case '0':   ret.zero_pad = true; continue;
					case '#':   ret.alternate = true; continue;
					case '-':   negative_width = true; continue;
					case '+':   ret.prepend_plus_if_positive = true; continue;
					case ' ':   ret.prepend_blank_if_positive = true; continue;
					default:    fmt--; break;
				}
				break;
			}

			if(*fmt == '*' && (fmt++, true))
			{
				// note: if you use *, then the negative width is ignored!
				*need_width = true;
			}
			else
			{
				while((*fmt >= '0') && (*fmt <= '9'))
					ret.width = 10 * ret.width + (*fmt++ - '0');

				if(negative_width)
					ret.width *= -1;
			}

			if(*fmt == '.' && (fmt++, true))
			{
				if(*fmt == '*' && (fmt++, true))
				{
					// int int_precision = va_arg(parameters, int);
					// ret.precision = 0 <= int_precision ? (size_t) int_precision : 0;
					*need_prec = true;
				}
				else if(*fmt == '-' && (fmt++, true))
				{
					while(('0' <= *fmt) && (*fmt <= '9'))
						fmt++;
				}
				else
				{
					ret.precision = 0;
					while((*fmt >= '0') && (*fmt <= '9'))
						ret.precision = 10 * ret.precision + (*fmt++ - '0');
				}
			}

		#if ENABLE_FIELD_SIZES

			if(fmt[0] == 'h')
			{
				if(fmt[1] == 'h')   fmt += 2, ret.length = format_args::LENGTH_SHORT_SHORT;
				else                fmt += 1, ret.length = format_args::LENGTH_SHORT;
			}
			else if(fmt[0] == 'l')
			{
				if(fmt[1] == 'l')   fmt += 2, ret.length = format_args::LENGTH_LONG_LONG;
				else                fmt += 1, ret.length = format_args::LENGTH_LONG;
			}
			else if(fmt[0] == 'L')  fmt += 1, ret.length = format_args::LENGTH_LONG_DOUBLE;
			else if(fmt[0] == 't')  fmt += 1, ret.length = format_args::LENGTH_PTRDIFF_T;
			else if(fmt[0] == 'j')  fmt += 1, ret.length = format_args::LENGTH_INTMAX_T;
			else if(fmt[0] == 'z')  fmt += 1, ret.length = format_args::LENGTH_SIZE_T;

		#endif

			ret.specifier = fmt[0];
			fmt++;

			*end = fmt;
			return ret;
		}


		inline std::string skip(const char* fmt, const char** end)
		{
			std::string ret;

			top:
			while(*fmt && *fmt != '%')
				ret += *fmt++;

			if(*fmt && fmt[1] == '%')
			{
				ret += "%";
				fmt += 2;
				goto top;
			}

			*end = fmt;
			return ret;
		}

		inline std::string sprint(const char* &fmt)
		{
			return std::string(fmt);
		}

		// we need to forward declare this.
		template <typename... Args>
		std::string sprint(const char* fmt, Args&&... xs);



		// we need bogus ones that don't take the arguments. if we get error handling, these will throw errors.
		template <typename... Args>
		std::string _consume_both_sprint(const format_args&, const char* fmt, Args&&... xs)
		{
			return std::string("<missing width and prec>")
				.append(sprint(fmt, xs...));
		}

		template <typename... Args>
		std::string _consume_prec_sprint(const format_args&, const char* fmt, Args&&... xs)
		{
			return std::string("<missing prec>")
				.append(sprint(fmt, xs...));
		}

		template <typename... Args>
		std::string _consume_width_sprint(const format_args&, const char* fmt, Args&&... xs)
		{
			return std::string("<missing width>")
				.append(sprint(fmt, xs...));
		}

		// we need separate versions of these functions that only enable if the argument is
		// a pointer, THEN check if the specifier is 'p', THEN do the void* thing.

		template <typename T>
		constexpr bool _stupid_type_tester(decltype(!std::is_pointer_v<std::decay<T>>
			|| &print_formatter<std::decay_t<T>>::print)*) { return true; }

		template <typename>
		constexpr bool _stupid_type_tester(...) { return false; }


		template <typename T, typename... Args, typename = std::enable_if_t<_stupid_type_tester<T>(0)>>
		std::string _consume_neither_sprint(const format_args& args, const char* fmt, T&& x, Args&&... xs)
		{
			return print_formatter<std::decay_t<T>>().print(x, args).append(sprint(fmt, xs...));
		}

		template <typename T, typename... Args, typename = std::enable_if_t<!_stupid_type_tester<T>(0)>, typename F = void*>
		std::string _consume_neither_sprint(const format_args& args, const char* fmt, T&& x, Args&&... xs)
		{
			return print_formatter<F>().print(x, args).append(sprint(fmt, xs...));
		}



		template <typename W, typename T, typename... Args,
			typename = std::enable_if_t<std::is_integral_v<std::remove_reference_t<W>> && _stupid_type_tester<T>(0)>
		>
		std::string _consume_width_sprint(format_args args, const char* fmt, W&& width, T&& x, Args&&... xs)
		{
			args.width = width;
			return print_formatter<std::decay_t<T>>().print(x, args).append(sprint(fmt, xs...));
		}

		template <typename W, typename T, typename... Args,
			typename = std::enable_if_t<std::is_integral_v<std::remove_reference_t<W>> && !_stupid_type_tester<T>(0)>,
			typename F = void*
		>
		std::string _consume_width_sprint(format_args args, const char* fmt, W&& width, T&& x, Args&&... xs)
		{
			args.width = width;
			return print_formatter<F>().print(x, args).append(sprint(fmt, xs...));
		}



		template <typename P, typename T, typename... Args,
			typename = std::enable_if_t<std::is_integral_v<std::remove_reference_t<P>> && _stupid_type_tester<T>(0)>
		>
		std::string _consume_prec_sprint(format_args args, const char* fmt, P&& prec, T&& x, Args&&... xs)
		{
			args.precision = prec;
			return print_formatter<std::decay_t<T>>().print(x, args).append(sprint(fmt, xs...));
		}

		template <typename P, typename T, typename... Args,
			typename = std::enable_if_t<std::is_integral_v<std::remove_reference_t<P>> && !_stupid_type_tester<T>(0)>,
			typename F = void*
		>
		std::string _consume_prec_sprint(format_args args, const char* fmt, P&& prec, T&& x, Args&&... xs)
		{
			args.precision = prec;
			return print_formatter<F>().print(x, args).append(sprint(fmt, xs...));
		}





		template <typename W, typename P, typename T, typename... Args,
			typename = std::enable_if_t<std::is_integral_v<std::remove_reference_t<W>>
				&& std::is_integral_v<std::remove_reference_t<P>> && _stupid_type_tester<T>(0)>
		>
		std::string _consume_both_sprint(format_args args, const char* fmt, W&& width, P&& prec, T&& x, Args&&... xs)
		{
			args.width = width;
			args.precision = prec;
			return print_formatter<std::decay_t<T>>().print(x, args).append(sprint(fmt, xs...));
		}

		template <typename W, typename P, typename T, typename... Args,
			typename = std::enable_if_t<std::is_integral_v<std::remove_reference_t<W>>
				&& std::is_integral_v<std::remove_reference_t<P>> && !_stupid_type_tester<T>(0)>,
			typename F = void*
		>
		std::string _consume_both_sprint(format_args args, const char* fmt, W&& width, P&& prec, T&& x, Args&&... xs)
		{
			args.width = width;
			args.precision = prec;
			return print_formatter<F>().print(x, args).append(sprint(fmt, xs...));
		}



		template <typename... Args>
		std::string sprint(const char* fmt, Args&&... xs)
		{
			bool need_prec = false;
			bool need_width = false;

			std::string ret = skip(fmt, &fmt);

			auto args = parseFormatArgs(fmt, &fmt, &need_width, &need_prec);

			// because the if happens at runtime, all these functions need to be instantiable. that's
			// why we make bogus ones that just return error strings when there aren't enough arguments.
			if(need_width && need_prec) return ret.append(_consume_both_sprint(args, fmt, xs...));
			else if(need_prec)          return ret.append(_consume_prec_sprint(args, fmt, xs...));
			else if(need_width)         return ret.append(_consume_width_sprint(args, fmt, xs...));
			else                        return ret.append(_consume_neither_sprint(args, fmt, xs...));
		}
	}


	inline std::string sprint(const std::string& fmt)
	{
		return fmt;
	}

	inline int print(const std::string& fmt)
	{
		return printf("%s", fmt.c_str());
	}

	inline int println(const std::string& fmt)
	{
		return printf("%s\n", fmt.c_str());
	}



	template <typename... Args>
	std::string sprint(const std::string& fmt, Args&&... xs)
	{
		return _internal::sprint(fmt.c_str(), xs...);
	}

	template <typename... Args>
	int print(const std::string& fmt, Args&&... xs)
	{
		auto x = _internal::sprint(fmt.c_str(), xs...);
		return printf("%s", x.c_str());
	}

	template <typename... Args>
	int println(const std::string& fmt, Args&&... xs)
	{
		auto x = _internal::sprint(fmt.c_str(), xs...);
		return printf("%s\n", x.c_str());
	}












	// formatters lie here

	template <typename T>
	struct print_formatter<T, typename std::enable_if<
		(
			std::is_integral_v<std::remove_cv_t<std::decay_t<T>>> &&
			!std::is_same_v<char, std::remove_cv_t<std::decay_t<T>>> &&
			!std::is_same_v<bool, std::remove_cv_t<std::decay_t<T>>> &&
			sizeof(T) <= sizeof(uint64_t)
		) ||
		(std::is_enum_v<std::remove_cv_t<std::decay_t<T>>>)
	>::type>
	{
		std::string print(T x, format_args args)
		{
			int base = 10;
			if(args.specifier == 'x' || args.specifier == 'X')      base = 16;
			// else if(args.specifier == 'o')                          base = 8;
			// else if(args.specifier == 'b')                          base = 2;

			// handle negative values ourselves btw, due to padding
			bool is_neg = false;

			if constexpr (!std::is_enum_v<T> && std::is_signed_v<T>)
			{
				is_neg = (x < 0);

				if(is_neg)
					x = -x;
			}

			std::string digits;
			{
				// if we print base 2 we need 64 digits!
				char buf[65] = {0};

				size_t digits_len = 0;
				auto spec = args.specifier;

				static std::map<int64_t, std::string> len_specs = {
					{ format_args::LENGTH_SHORT_SHORT,  "hh"   },
					{ format_args::LENGTH_SHORT,        "h"    },
					{ format_args::LENGTH_LONG,         "l"    },
					{ format_args::LENGTH_LONG_LONG,    "ll"   },
					{ format_args::LENGTH_INTMAX_T,     "j"    },
					{ format_args::LENGTH_SIZE_T,       "z"    },
					{ format_args::LENGTH_PTRDIFF_T,    "t"    }
				};

				auto fmt_str = ("%" + len_specs[args.length] + spec);

				digits_len = snprintf(&buf[0], 64, fmt_str.c_str(), x);

				// sadly, we must cheat here as well, because osx doesn't bloody have charconv (STILL)?

				// std::to_chars_result ret;
				// if constexpr (std::is_enum_v<T>)
				// 	ret = std::to_chars(&buf[0], &buf[65], static_cast<std::underlying_type_t<T>>(x), /* base: */ base);

				// else
				// 	ret = std::to_chars(&buf[0], &buf[65], x, /* base: */ base);


				// if(ret.ec == std::errc())   digits_len = (ret.ptr - &buf[0]), *ret.ptr = 0;
				// else                        return "<to_chars(int) error>";

				if(isupper(args.specifier))
					for(size_t i = 0; i < digits_len; i++)
						buf[i] = static_cast<char>(toupper(buf[i]));

				digits = std::string(buf, digits_len);
			}

			std::string prefix;
			if(is_neg)                              prefix += "-";
			else if(args.prepend_plus_if_positive)  prefix += "+";
			else if(args.prepend_blank_if_positive) prefix += " ";

			// prepend 0x or 0b or 0o for alternate.
			int64_t prefix_digits_length = 0;
			if((base == 2 || base == 8 || base == 16) && args.alternate)
			{
				prefix += "0";
				#if HEX_0X_RESPECTS_UPPERCASE
					prefix += args.specifier;
				#else
					prefix += tolower(args.specifier);
				#endif
				prefix_digits_length += 2;
			}

			int64_t output_length_with_precision = (args.precision == -1
				? digits.size()
				: std::max(args.precision, static_cast<int64_t>(digits.size()))
			);

			int64_t digits_length = prefix_digits_length + digits.size();
			int64_t normal_length = prefix.size() + digits.size();
			int64_t length_with_precision = prefix.size() + output_length_with_precision;

			bool use_precision = (args.precision != -1);
			bool use_zero_pad = args.zero_pad && 0 <= args.width && !use_precision;
			bool use_left_pad = !use_zero_pad && 0 <= args.width;
			bool use_right_pad = !use_zero_pad && args.width < 0;

			int64_t abs_field_width = std::abs(args.width);

			std::string pre_prefix;
			if(use_left_pad)
				pre_prefix = std::string(std::max(int64_t(0), abs_field_width - length_with_precision), ' ');

			std::string post_prefix;
			if(use_zero_pad)
				post_prefix = std::string(std::max(int64_t(0), abs_field_width - normal_length), '0');

			std::string prec_string;
			if(use_precision)
				prec_string = std::string(std::max(int64_t(0), args.precision - digits_length), '0');

			std::string postfix;
			if(use_right_pad)
				postfix = std::string(std::max(int64_t(0), abs_field_width - length_with_precision), ' ');

			return pre_prefix + prefix + post_prefix + prec_string + digits + postfix;
		}
	};

	template <typename T>
	struct print_formatter<T, typename std::enable_if<
		std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<std::remove_cv_t<T>>>>
	>::type>
	{
		std::string print(const T& x, const format_args& args)
		{
			constexpr int default_prec = 6;

			char buf[81] = { 0 };
			int64_t num_length = 0;

			// lmao. nobody except msvc stl (and only the most recent version) implements std::to_chars
			// for floating point types, even though it's in the c++17 standard. so we just cheat.

			// let printf handle the precision, but we'll handle the width and the negativity.
			{
				const char* fmt_str = 0;
				bool longdouble = (args.length == format_args::LENGTH_LONG_DOUBLE);

				switch(args.specifier)
				{
					case 'E': fmt_str = (longdouble ? "%.*LE" : "%.*E"); break;
					case 'e': fmt_str = (longdouble ? "%.*Le" : "%.*e"); break;
					case 'F': fmt_str = (longdouble ? "%.*LF" : "%.*F"); break;
					case 'f': fmt_str = (longdouble ? "%.*Lf" : "%.*f"); break;
					case 'G': fmt_str = (longdouble ? "%.*LG" : "%.*G"); break;

					case 'g': [[fallthrough]];
					default:  fmt_str = (longdouble ? "%.*Lg" : "%.*g"); break;
				}

				num_length = snprintf(&buf[0], 80, fmt_str,
					(args.precision == -1 ? default_prec : args.precision), fabs(x));
			}

			auto abs_field_width = std::abs(args.width);

			bool use_zero_pad = args.zero_pad && args.width >= 0;
			bool use_left_pad = !use_zero_pad && args.width >= 0;
			bool use_right_pad = !use_zero_pad && args.width < 0;

			// account for the signs, if any.
			if(x < 0 || args.prepend_plus_if_positive || args.prepend_blank_if_positive)
				num_length += 1;

			std::string pre_prefix;
			if(use_left_pad)
				pre_prefix = std::string(std::max(int64_t(0), abs_field_width - num_length), ' ');

			std::string prefix;
			if(x < 0)                               prefix = "-";
			else if(args.prepend_plus_if_positive)  prefix = "+";
			else if(args.prepend_blank_if_positive) prefix = " ";

			std::string post_prefix;
			if(use_zero_pad)
				post_prefix = std::string(std::max(int64_t(0), abs_field_width - num_length), '0');

			std::string postfix;
			if(use_right_pad)
				postfix = std::string(std::max(int64_t(0), abs_field_width - num_length), ' ');

			return pre_prefix + prefix + post_prefix + std::string(buf) + postfix;
		}
	};


	template <typename T>
	struct print_formatter<T, typename std::enable_if<
		(std::is_same_v<std::string, T>) || (std::is_same_v<std::string_view, T>) ||
		(std::is_same_v<const char*, std::decay_t<T>>) || (std::is_same_v<char*, std::decay_t<T>>)
	>::type>
	{
		std::string print(const T& x, const format_args& args)
		{
			int64_t string_length = 0;
			int64_t abs_field_width = std::abs(args.width);

			if constexpr (std::is_pointer_v<std::decay_t<T>>)
			{
				for(int64_t i = 0; (args.precision != -1 ? (i < args.precision && x && x[i]) : (x && x[i])); i++)
					string_length++;
			}
			else
			{
				if(args.precision >= 0) string_length = std::min(args.precision, static_cast<int64_t>(x.size()));
				else                    string_length = static_cast<int64_t>(x.size());
			}

			std::string prefix;
			if(args.width >= 0 && string_length < abs_field_width)
				prefix = std::string(abs_field_width - string_length, ' ');

			std::string postfix;
			if(args.width < 0 && string_length < abs_field_width)
				postfix = std::string(abs_field_width - string_length, ' ');


			if constexpr (std::is_pointer_v<std::decay_t<T>>)
				return prefix + std::string(x, string_length) + postfix;

			else
				return prefix + std::string(x, 0, string_length) + postfix;
		}
	};

	template <typename T>
	struct print_formatter<T, typename std::enable_if<
		(std::is_same_v<char, std::remove_cv_t<std::remove_reference_t<std::remove_cv_t<T>>>>)
	>::type>
	{
		std::string print(const T& x, const format_args& args)
		{
			// just reuse the string printer, but with a one-char-long string.
			// probably not so efficient, but idgaf for now.
			return print_formatter<std::string>()
				.print(std::string(1, x), args);
		}
	};

	template <typename T>
	struct print_formatter<T, typename std::enable_if<
		(std::is_same_v<bool, std::remove_cv_t<std::remove_reference_t<std::remove_cv_t<T>>>>)
	>::type>
	{
		std::string print(const T& x, format_args args)
		{
			return print_formatter<std::string>()
				.print(x ? "true" : "false", args);
		}
	};

	template <typename T>
	struct print_formatter<T, typename std::enable_if<
		(std::is_same_v<void*, std::decay_t<T>>)
	>::type>
	{
		std::string print(const T& x, format_args args)
		{
			args.specifier = 'x';
			return print_formatter<uintptr_t>()
				.print(reinterpret_cast<uintptr_t>(x), args);
		}
	};
}












