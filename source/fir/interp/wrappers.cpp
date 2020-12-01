// wrappers.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "platform.h"
#include "ir/module.h"

#include "ir/interp.h"


PLATFORM_EXPORT_FUNCTION
void __interp_intrinsic_memset(void* dst, int8_t val, size_t cnt, bool)
{
	memset(dst, val, cnt);
}

PLATFORM_EXPORT_FUNCTION
void __interp_intrinsic_memcpy(void* dst, void* src, size_t cnt, bool)
{
	memcpy(dst, src, cnt);
}

PLATFORM_EXPORT_FUNCTION
void __interp_intrinsic_memmove(void* dst, void* src, size_t cnt, bool)
{
	memmove(dst, src, cnt);
}

PLATFORM_EXPORT_FUNCTION
int __interp_intrinsic_memcmp(void* a, void*b, size_t cnt, bool)
{
	return memcmp(a, b, cnt);
}

PLATFORM_EXPORT_FUNCTION
int64_t __interp_intrinsic_roundup_pow2(int64_t x)
{
	auto num = x;
	auto ret = 1;

	while(num > 0)
	{
		num >>= 1;
		ret <<= 1;
	}

	return ret;
}







PLATFORM_EXPORT_FUNCTION int __interp_wrapper_printf(char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	int ret = vprintf(fmt, ap);

	va_end(ap);
	return ret;
}

PLATFORM_EXPORT_FUNCTION int __interp_wrapper_sprintf(char* buf, char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	int ret = vsprintf(buf, fmt, ap);

	va_end(ap);
	return ret;
}

PLATFORM_EXPORT_FUNCTION int __interp_wrapper_snprintf(char* buf, size_t n, char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	int ret = vsnprintf(buf, n, fmt, ap);

	va_end(ap);
	return ret;
}

PLATFORM_EXPORT_FUNCTION int __interp_wrapper_fprintf(void* stream, char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	int ret = vfprintf(static_cast<FILE*>(stream), fmt, ap);

	va_end(ap);
	return ret;
}



