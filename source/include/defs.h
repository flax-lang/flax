// defs.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "precompile.h"


#ifndef __has_include
#error "Please switch to a compiler that supports '__has_include'"
#endif

/*
	STRING_VIEW_TYPE documentation

	0: normal, std::string_view
	1: experimental, std::experimental::string_view
	2: external, stx::string_view
*/
#if __has_include(<string_view>) && _HAS_CXX17
	#include <string_view>
	#define STRING_VIEW_TYPE 0
#elif __has_include(<experimental/string_view>) && _HAS_CXX17
	#include <experimental/string_view>
	#define STRING_VIEW_TYPE 1
#else
	// #error "Please switch to a compiler that supports 'string_view', or change your c++ standard version"
	#include "../external/stx/string_view.hpp"
	#define STRING_VIEW_TYPE 2
#endif


#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
	#pragma warning(push, 0)
#endif

#include "../external/mpreal/mpreal.h"

#if defined(__clang__)
	#pragma clang diagnostic pop
#elif defined(__GNUC__)
	#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
	#pragma warning(pop)
#endif


struct Identifier;
enum class VisibilityLevel;

namespace fir { struct Type; }
namespace tinyformat
{
	void formatValue(std::ostream& out, const char* /*fmtBegin*/, const char* fmtEnd, int ntrunc, fir::Type* ty);
	void formatValue(std::ostream& out, const char* /*fmtBegin*/, const char* fmtEnd, int ntrunc, const Identifier& id);
	void formatValue(std::ostream& out, const char* /*fmtBegin*/, const char* fmtEnd, int ntrunc, const VisibilityLevel& vl);
}

#define TINYFORMAT_ERROR(x)
#include "../external/tinyformat/tinyformat.h"




[[noreturn]] void doTheExit(bool trace = true);

template <typename... Ts>
[[noreturn]] inline void _error_and_exit(const char* s, Ts... ts)
{
	tinyformat::format(std::cerr, s, ts...);
	doTheExit();
}

template <typename T>
std::vector<T> operator + (const std::vector<T>& vec, const T& elm)
{
	auto copy = vec;

	copy.push_back(elm);
	return copy;
}

template <typename T>
std::vector<T> operator + (const T& elm, const std::vector<T>& vec)
{
	auto copy = vec;

	copy.insert(copy.begin(), elm);
	return copy;
}




template <typename... Ts>
std::string strprintf(const char* fmt, Ts... ts)
{
	return tinyformat::format(fmt, ts...);
}


#define __nothing

#ifdef NDEBUG
#define iceAssert(x)        ((void) (x))
#else
#define iceAssert(x)		((x) ? ((void) (0)) : _error_and_exit("Compiler assertion at %s:%d, cause:\n'%s' evaluated to false\n", __FILE__, __LINE__, #x))
#endif

#define TAB_WIDTH	4


#define dcast(t, v)		dynamic_cast<t*>(v)

namespace util
{
	#ifndef STRING_VIEW_TYPE
		#error "what?"
	#endif

	#if STRING_VIEW_TYPE == 0
		using string_view = std::string_view;
	#elif STRING_VIEW_TYPE == 1
		using string_view = std::experimental::string_view;
	#elif STRING_VIEW_TYPE == 2
		using string_view = stx::string_view;
	#else
		#error "No string_view, or unknown type"
	#endif

	inline std::string to_string(const string_view& sv)
	{
		return std::string(sv.data(), sv.length());
	}

	template <typename T>
	bool match(const T& first)
	{
		return true;
	}

	template <typename T, typename U>
	bool match(const T& first, const U& second)
	{
		return (first == second);
	}

	template <typename T, typename U, typename... Args>
	bool match(const T& first, const U& second, const Args&... comps)
	{
		return (first == second) || match(first, comps...);
	}

}

namespace fir
{
	struct Type;
	struct Value;
}

namespace sst
{
	struct Expr;
	struct Stmt;
	struct Defn;
}

enum class IdKind
{
	Invalid,
	Name,
	Function,

	Type,
};


#define COLOUR_RESET			"\033[0m"
#define COLOUR_BLACK			"\033[30m"			// Black
#define COLOUR_RED				"\033[31m"			// Red
#define COLOUR_GREEN			"\033[32m"			// Green
#define COLOUR_YELLOW			"\033[33m"			// Yellow
#define COLOUR_BLUE				"\033[34m"			// Blue
#define COLOUR_MAGENTA			"\033[35m"			// Magenta
#define COLOUR_CYAN				"\033[36m"			// Cyan
#define COLOUR_WHITE			"\033[37m"			// White
#define COLOUR_BLACK_BOLD		"\033[1m"			// Bold Black
#define COLOUR_RED_BOLD			"\033[1m\033[31m"	// Bold Red
#define COLOUR_GREEN_BOLD		"\033[1m\033[32m"	// Bold Green
#define COLOUR_YELLOW_BOLD		"\033[1m\033[33m"	// Bold Yellow
#define COLOUR_BLUE_BOLD		"\033[1m\033[34m"	// Bold Blue
#define COLOUR_MAGENTA_BOLD		"\033[1m\033[35m"	// Bold Magenta
#define COLOUR_CYAN_BOLD		"\033[1m\033[36m"	// Bold Cyan
#define COLOUR_WHITE_BOLD		"\033[1m\033[37m"	// Bold White
#define COLOUR_GREY_BOLD		"\033[30;1m"		// Bold Grey

template <typename... Ts> std::string strbold(const char* fmt, Ts... ts)
{
	return std::string(COLOUR_RESET) + std::string(COLOUR_BLACK_BOLD) + tinyformat::format(fmt, ts...) + std::string(COLOUR_RESET);
}

struct Identifier
{
	Identifier() : name(""), kind(IdKind::Invalid) { }
	Identifier(std::string n, IdKind k) : name(n), kind(k) { }

	std::string name;
	std::vector<std::string> scope;
	std::vector<fir::Type*> params;

	IdKind kind;

	std::string str() const;
	std::string mangled() const;
	std::string mangledName() const;

	bool operator == (const Identifier& other) const;
	bool operator != (const Identifier& other) const;
};


struct Location
{
	size_t fileID = 0;
	size_t line = 0;
	size_t col = 0;
	size_t len = 0;

	bool operator == (const Location& other) const
	{
		return this->col == other.col && this->line == other.line && this->len == other.len && this->fileID == other.fileID;
	}

	bool operator != (const Location& other) const
	{
		return !(*this == other);
	}

	std::string toString() const;
	std::string shortString() const;
};

struct Locatable
{
	Locatable(const Location& l, const std::string& readable) : loc(l), readableName(readable) { }
	virtual ~Locatable() { }

	Location loc;
	std::string readableName;
};

//? in order of complexity, i guess?
enum class ErrKind
{
	Bare,           // error without context
	Simple,         // error with context

	Span,           // error with context, but with squiggles within those (general case of
	Overload,       // most complex one; built specifically to handle multiple candidates and such
};

enum class MsgType
{
	Note,
	Warning,
	Error
};

struct ErrorMsg
{

	virtual ~ErrorMsg() { }

	virtual void post() = 0;
	virtual bool hasErrors() const = 0;
	virtual ErrorMsg* clone() const = 0;

	virtual ErrorMsg& append(const ErrorMsg& e)
	{
		this->subs.push_back(e.clone());
		return *this;
	}

	[[noreturn]] void postAndQuit()
	{
		this->post();
		doTheExit();
	}

	ErrKind kind;
	MsgType type;
	std::vector<ErrorMsg*> subs;

	protected:
	ErrorMsg(ErrKind k, MsgType t) : kind(k), type(t) { }
};

struct BareError : ErrorMsg
{
	BareError(MsgType t = MsgType::Error) : ErrorMsg(ErrKind::Bare, t) { }
	BareError(const std::string& m, MsgType t = MsgType::Error) : ErrorMsg(ErrKind::Bare, t), msg(m) { }

	template <typename... Ts>
	BareError& set(const char* fmt, Ts... ts)
	{
		this->msg = strprintf(fmt, ts...);
		return *this;
	}

	BareError& set(const std::string& m)
	{
		this->msg = m;
		return *this;
	}

	template <typename... Ts> static BareError make(const char* fmt, Ts... ts) { return BareError().set(fmt, ts...); }
	template <typename... Ts> static BareError make(MsgType t, const char* fmt, Ts... ts) { return BareError(t).set(fmt, ts...); }

	virtual void post() override;
	virtual bool hasErrors() const override;
	virtual BareError* clone() const override;
	virtual BareError& append(const ErrorMsg& e) override { this->subs.push_back(e.clone()); return *this; }

	std::string msg;
};

struct SimpleError : ErrorMsg
{
	SimpleError(MsgType t = MsgType::Error) : ErrorMsg(ErrKind::Simple, t) { }
	SimpleError(const Location& l, const std::string& m, MsgType t = MsgType::Error) : ErrorMsg(ErrKind::Bare, t), loc(l), msg(m) { }

	template <typename... Ts>
	SimpleError& set(Locatable* l, const char* fmt, Ts... ts)
	{
		return this->set(l ? l->loc : Location(), fmt, ts...);
	}

	template <typename... Ts>
	SimpleError& set(const Location& l, const char* fmt, Ts... ts)
	{
		this->loc = l;
		this->msg = strprintf(fmt, ts...);
		return *this;
	}

	template <typename... Ts>
	static SimpleError make(const Location& l, const char* fmt, Ts... ts) { return SimpleError().set(l, fmt, ts...); }
	template <typename... Ts>
	static SimpleError make(Locatable* l, const char* fmt, Ts... ts)      { return SimpleError().set(l, fmt, ts...); }
	template <typename... Ts>
	static SimpleError make(MsgType t, const Location& l, const char* fmt, Ts... ts) { return SimpleError(t).set(l, fmt, ts...); }
	template <typename... Ts>
	static SimpleError make(MsgType t, Locatable* l, const char* fmt, Ts... ts)      { return SimpleError(t).set(l, fmt, ts...); }

	SimpleError& set(const Location& l, const std::string& m)
	{
		this->loc = l;
		this->msg = m;
		return *this;
	}

	virtual void post() override;
	virtual bool hasErrors() const override;
	virtual SimpleError* clone() const override;
	virtual SimpleError& append(const ErrorMsg& e) override { this->subs.push_back(e.clone()); return *this; }

	Location loc;
	std::string msg;

	// just a hacky thing to print some words (eg. '(call site)') before the context.
	bool printContext = true;
	std::string wordsBeforeContext;
};

struct SpanError : ErrorMsg
{
	struct Span
	{
		Span() { }
		Span(const Location& l, const std::string& m) : loc(l), msg(m) { }

		Location loc;
		std::string msg;
		std::string colour;
	};

	SpanError(MsgType t = MsgType::Error) : ErrorMsg(ErrKind::Span, t) { }
	SpanError(const SimpleError& se, MsgType t = MsgType::Error) : ErrorMsg(ErrKind::Span, t), top(se) { }
	SpanError(const SimpleError& se, const std::vector<Span>& s, MsgType t = MsgType::Error) : ErrorMsg(ErrKind::Span, t), top(se), spans(s) { }

	SpanError& add(const Span& s);
	SpanError& set(const SimpleError& se) { this->top = se; return *this; }

	virtual void post() override;
	virtual bool hasErrors() const override;
	virtual SpanError* clone() const override;
	virtual SpanError& append(const ErrorMsg& e) override { this->subs.push_back(e.clone()); return *this; }

	SimpleError top;
	std::vector<Span> spans;

	// again, another internal flag; this one controls whether or not to underline the original location.
	bool highlightActual = true;
};



struct OverloadError : ErrorMsg
{
	OverloadError() : ErrorMsg(ErrKind::Overload, MsgType::Error) { }
	OverloadError(const SimpleError& se) : ErrorMsg(ErrKind::Overload, MsgType::Error) { this->set(se); }

	void clear();
	virtual void post() override;
	virtual bool hasErrors() const override;
	virtual OverloadError* clone() const override;
	virtual OverloadError& append(const ErrorMsg& e) override { this->subs.push_back(e.clone()); return *this; }

	OverloadError& set(const SimpleError& se) { this->top = se; return *this; }
	OverloadError& addCand(Locatable* d, const ErrorMsg& e);

	// void incorporate(const OverloadError& e);
	// void incorporate(const ErrorMsg& e);

	SimpleError top;
	std::unordered_map<Locatable*, ErrorMsg*> cands;
};





struct TCResult
{
	enum class RK
	{
		Invalid,

		Statement,
		Expression,
		Definition,

		Parametric,
		Dummy,

		Error
	};

	union {
		sst::Stmt* _st;
		sst::Expr* _ex;
		sst::Defn* _df;
		ErrorMsg* _pe;
	};

	RK _kind = RK::Invalid;

	~TCResult() { if(this->isError() && this->_pe) delete this->_pe; }

	TCResult(RK k) :  _kind(k)                                  { _st = 0; }
	explicit TCResult(sst::Stmt* s) : _kind(RK::Statement)      { _st = s; }
	explicit TCResult(sst::Expr* e) : _kind(RK::Expression)     { _ex = e; }
	explicit TCResult(sst::Defn* d) : _kind(RK::Definition)     { _df = d; }

	explicit TCResult(const ErrorMsg& pe) : _kind(RK::Error) { _pe = pe.clone(); }

	TCResult(const TCResult& r)
	{
		this->_kind = r._kind;

		if(this->isError())     this->_pe = r._pe->clone();
		else if(this->isStmt()) this->_st = r._st;
		else if(this->isExpr()) this->_ex = r._ex;
		else if(this->isDefn()) this->_df = r._df;
	}

	TCResult(TCResult&& r)
	{
		this->_kind = r._kind;

		if(this->isError())     { this->_pe = r._pe; r._pe = 0; }
		else if(this->isStmt()) { this->_st = r._st; r._st = 0; }
		else if(this->isExpr()) { this->_ex = r._ex; r._ex = 0; }
		else if(this->isDefn()) { this->_df = r._df; r._df = 0; }
	}

	TCResult& operator = (const TCResult& r)
	{
		TCResult tmp(r);
		*this = std::move(tmp);
		return *this;
	}

	TCResult& operator = (TCResult&& r)
	{
		if(&r != this)
		{
			if(this->isError())     { delete this->_pe; this->_pe = r._pe; r._pe = 0; }
			else if(this->isStmt()) { this->_st = r._st; r._st = 0; }
			else if(this->isExpr()) { this->_ex = r._ex; r._ex = 0; }
			else if(this->isDefn()) { this->_df = r._df; r._df = 0; }
		}

		return *this;
	}



	ErrorMsg& error()    { if(this->_kind != RK::Error)      { _error_and_exit("not error\n"); } return *this->_pe; }


	sst::Expr* expr();
	sst::Defn* defn();

	//* stmt() is the most general case -- definitions and expressions are both statements.
	// note: we need the definition of sst::Stmt and sst::Expr to do safe dynamic casting, so it's in identifier.cpp.
	sst::Stmt* stmt();

	bool isError()      { return this->_kind == RK::Error; }
	bool isStmt()       { return this->_kind == RK::Statement; }
	bool isExpr()       { return this->_kind == RK::Expression; }
	bool isDefn()       { return this->_kind == RK::Definition; }
	bool isParametric() { return this->_kind == RK::Parametric; }
	bool isDummy()      { return this->_kind == RK::Dummy; }

	static TCResult getParametric() { return TCResult(RK::Parametric); }
	static TCResult getDummy()      { return TCResult(RK::Dummy); }
};

struct CGResult
{
	enum class VK
	{
		Invalid,
		LValue,		// lvalue, as in c/c++
		RValue,		// rvalue, as in c/c++
		LitRValue,	// literal rvalue, simplifies refcounting a little I guess

		Break,
		Continue,
	};

	CGResult() : CGResult(0) { }
	explicit CGResult(fir::Value* v) noexcept : value(v), pointer(0), kind(VK::RValue) { }
	explicit CGResult(fir::Value* v, fir::Value* p) noexcept : value(v), pointer(p), kind(VK::RValue) { }
	explicit CGResult(fir::Value* v, fir::Value* p, VK k) noexcept : value(v), pointer(p), kind(k) { }

	fir::Value* value = 0;
	fir::Value* pointer = 0;

	VK kind = VK::Invalid;
};

namespace std
{
	template<>
	struct hash<Identifier>
	{
		std::size_t operator()(const Identifier& k) const
		{
			using std::size_t;
			using std::hash;
			using std::string;

			// Compute individual hash values for first,
			// second and third and combine them using XOR
			// and bit shifting:

			// return ((hash<string>()(k.name) ^ (hash<std::vector<std::string>>()(k.scope) << 1)) >> 1) ^ (hash<int>()(k.third) << 1);
			return hash<string>()(k.str());
		}
	};
}


enum class VisibilityLevel
{
	Invalid,

	Public,
	Private,
	Internal,
};

struct TypeConstraints_t
{
	std::vector<std::string> protocols;
	int pointerDegree = 0;

	bool operator == (const TypeConstraints_t& other) const
	{
		return this->protocols == other.protocols && this->pointerDegree == other.pointerDegree;
	}
};

using TypeParamMap_t = std::unordered_map<std::string, fir::Type*>;



namespace util
{
	template <typename T, class UnaryOp, typename K = typename std::result_of<UnaryOp(T)>::type>
	std::vector<K> map(const std::vector<T>& input, UnaryOp fn)
	{
		std::vector<K> ret;
		for(auto i : input)
			ret.push_back(fn(i));

		return ret;
	}

	template <typename T, class UnaryOp, class Predicate, typename K = typename std::result_of<UnaryOp(T)>::type>
	std::vector<K> filterMap(const std::vector<T>& input, Predicate cond, UnaryOp fn)
	{
		std::vector<K> ret;
		for(auto i : input)
		{
			if(cond(i))
				ret.push_back(fn(i));
		}

		return ret;
	}

	template <typename T, class UnaryOp, class Predicate, typename K = typename std::result_of<UnaryOp(T)>::type>
	std::vector<K> mapFilter(const std::vector<T>& input, UnaryOp fn, Predicate cond)
	{
		std::vector<K> ret;
		for(auto i : input)
		{
			auto k = fn(i);
			if(cond(k)) ret.push_back(k);
		}

		return ret;
	}

	template <typename T, class Predicate>
	std::vector<T> filter(const std::vector<T>& input, Predicate cond)
	{
		std::vector<T> ret;
		for(const auto& i : input)
			if(cond(i))
				ret.push_back(i);

		return ret;
	}

	template <typename T, class Predicate>
	std::vector<T> filterUntil(const std::vector<T>& input, Predicate cond)
	{
		std::vector<T> ret;
		for(const auto& i : input)
		{
			if(cond(i)) ret.push_back(i);
			else        break;
		}

		return ret;
	}

	template <typename T, class Predicate>
	size_t indexOf(const std::vector<T>& input, Predicate cond)
	{
		for(size_t i = 0; i < input.size(); i++)
			if(cond(input[i])) return i;

		return -1;
	}


	inline std::string serialiseScope(const std::vector<std::string>& scope)
	{
		std::string ret;
		for(const std::string& s : scope)
			ret += s + ".";

		if(!ret.empty() && ret.back() == '.')
			ret.pop_back();

		return ret;
	}

	inline std::string plural(const std::string& thing, size_t count)
	{
		return thing + (count == 1 ? "" : "s");
	}

	template <typename T, class UnaryOp>
	std::string listToString(const std::vector<T>& list, UnaryOp fn)
	{
		std::string ret;
		for(size_t i = 0; i < list.size(); i++)
		{
			ret += fn(list[i]);
			if(i != list.size() - 1)
				ret += ", ";
		}

		return "[ " + ret + " ]";
	}

	template <typename K, typename V>
	std::vector<std::pair<K, V>> pairs(const std::unordered_map<K, V>& map)
	{
		auto ret = std::vector<std::pair<K, V>>(map.begin(), map.end());
		return ret;
	}

	template <typename T>
	class FastVector
	{
		public:
		FastVector()
		{
			this->array = 0;
			this->length = 0;
			this->capacity = 0;
		}
		FastVector(size_t initSize)
		{
			this->array = (T*) malloc(initSize * sizeof(T));
			this->capacity = initSize;
			this->length = 0;
		}

		FastVector(const FastVector& other)
		{
			this->array = (T*) malloc(other.capacity * sizeof(T));
			memmove(this->array, other.array, other.length * sizeof(T));

			this->capacity = other.capacity;
			this->length = other.length;
		}

		FastVector& operator = (const FastVector& other)
		{
			this->array = (T*) malloc(other.capacity * sizeof(T));
			memmove(this->array, other.array, other.length * sizeof(T));

			this->capacity = other.capacity;
			this->length = other.length;

			return *this;
		}

		FastVector(FastVector&& other)
		{
			// move.
			this->array = other.array;
			this->length = other.length;
			this->capacity = other.capacity;

			other.array = 0;
			other.length = 0;
			other.capacity = 0;
		}

		FastVector& operator = (FastVector&& other)
		{
			if(this != &other)
			{
				if(this->array)
					free(this->array);

				// move.
				this->array = other.array;
				this->length = other.length;
				this->capacity = other.capacity;

				other.array = 0;
				other.length = 0;
				other.capacity = 0;
			}

			return *this;
		}

		~FastVector()
		{
			if(this->array != 0)
				free(this->array);
		}

		size_t size() const
		{
			return this->length;
		}

		T& operator[] (size_t index) const
		{
			return this->array[index];
		}

		T* getEmptySlotPtrAndAppend()
		{
			this->autoResize();

			this->length++;
			return &this->array[this->length - 1];
		}

		void autoResize()
		{
			if(this->length == this->capacity)
			{
				if(this->capacity == 0)
					this->capacity = 64;

				this->array = (T*) realloc(this->array, this->capacity * 2 * sizeof(T));

				iceAssert(this->array);
				this->capacity *= 2;
			}
		}

		private:
		T* array = 0;
		size_t capacity;
		size_t length;
	};
}









namespace Operator
{
	extern const std::string Plus;
	extern const std::string Minus;
	extern const std::string Multiply;
	extern const std::string Divide;
	extern const std::string Modulo;

	extern const std::string UnaryPlus;
	extern const std::string UnaryMinus;

	extern const std::string PointerDeref;
	extern const std::string AddressOf;

	extern const std::string BitwiseNot;
	extern const std::string BitwiseAnd;
	extern const std::string BitwiseOr;
	extern const std::string BitwiseXor;
	extern const std::string BitwiseShiftLeft;
	extern const std::string BitwiseShiftRight;

	extern const std::string LogicalNot;
	extern const std::string LogicalAnd;
	extern const std::string LogicalOr;

	extern const std::string CompareEQ;
	extern const std::string CompareNEQ;
	extern const std::string CompareLT;
	extern const std::string CompareLEQ;
	extern const std::string CompareGT;
	extern const std::string CompareGEQ;

	extern const std::string Assign;
	extern const std::string PlusEquals;
	extern const std::string MinusEquals;
	extern const std::string MultiplyEquals;
	extern const std::string DivideEquals;
	extern const std::string ModuloEquals;
	extern const std::string BitwiseShiftLeftEquals;
	extern const std::string BitwiseShiftRightEquals;
	extern const std::string BitwiseXorEquals;
	extern const std::string BitwiseAndEquals;
	extern const std::string BitwiseOrEquals;


	std::string getNonAssignmentVersion(const std::string& op);
	bool isArithmetic(const std::string& op);
	bool isComparison(const std::string& op);
	bool isAssignment(const std::string& op);
	bool isBitwise(const std::string& op);
}



// https://stackoverflow.com/questions/28367913/how-to-stdhash-an-unordered-stdpair

template<typename T>
void _hash_combine(std::size_t& seed, const T& key)
{
	std::hash<T> hasher;
	seed ^= hasher(key) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
	template<typename T1, typename T2>
	struct hash<std::pair<T1, T2>>
	{
		size_t operator () (const std::pair<T1, T2>& p) const
		{
			size_t seed = 0;
			_hash_combine(seed, p.first);
			_hash_combine(seed, p.second);
			return seed;
		}
	};
}








// defer implementation
// credit: gingerBill
// shamelessly stolen from https://github.com/gingerBill/gb

#if 0
namespace __dontlook
{
	template <typename F>
	struct privDefer
	{
		F f;
		privDefer(F f) : f(f) { }
		~privDefer() { f(); }
	};

	template <typename F>
	privDefer<F> defer_func(F f)
	{
		return privDefer<F>(f);
	}
}

#define DEFER_1(x, y)	x##y
#define DEFER_2(x, y)	DEFER_1(x, y)
#define DEFER_3(x)		DEFER_2(x, __COUNTER__)
#define defer(code)		auto DEFER_3(_defer_) = __dontlook::defer_func([&](){code;})
#endif

namespace __dontlook
{
	// NOTE(bill): Stupid fucking templates
	template <typename T> struct gbRemoveReference       { typedef T Type; };
	template <typename T> struct gbRemoveReference<T &>  { typedef T Type; };
	template <typename T> struct gbRemoveReference<T &&> { typedef T Type; };

	/// NOTE(bill): "Move" semantics - invented because the C++ committee are idiots (as a collective not as indiviuals (well a least some aren't))
	template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &t)  { return static_cast<T &&>(t); }
	template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &&t) { return static_cast<T &&>(t); }
	template <typename T> inline T &&gb_move   (T &&t)                                   { return static_cast<typename gbRemoveReference<T>::Type &&>(t); }
	template <typename F>
	struct gbprivDefer {
		F f;
		gbprivDefer(F &&f) : f(gb_forward<F>(f)) {}
		~gbprivDefer() { f(); }
	};
	template <typename F> gbprivDefer<F> gb__defer_func(F &&f) { return gbprivDefer<F>(gb_forward<F>(f)); }
}

#define GB_DEFER_1(x, y) x##y
#define GB_DEFER_2(x, y) GB_DEFER_1(x, y)
#define GB_DEFER_3(x)    GB_DEFER_2(x, __COUNTER__)
#define defer(code) auto GB_DEFER_3(_defer_) = __dontlook::gb__defer_func([&]()->void{code;})






