// defs.h
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "zpr.h"
#include "utils.h"


struct Identifier;
enum class VisibilityLevel;

namespace fir { struct Type; }
namespace pts { struct Type; }


namespace zpr
{
	template <typename T>
	struct print_formatter<T, typename std::enable_if<
		(std::is_same_v<fir::Type*, T>) ||
		(std::is_pointer_v<T> && std::is_base_of_v<fir::Type, std::remove_pointer_t<T>>)
	>::type>
	{
		std::string print(const T& x, const format_args&)
		{
			return x->str();
		}
	};

	template <>
	struct print_formatter<Identifier>
	{
		std::string print(const Identifier& x, const format_args& args);
	};

	template <>
	struct print_formatter<VisibilityLevel>
	{
		std::string print(const VisibilityLevel& x, const format_args& args);
	};
}




[[noreturn]] void doTheExit(bool trace = true);

template <typename... Ts>
[[noreturn]] inline void _error_and_exit(const char* fmt, Ts&&... ts)
{
	// tinyformat::format(std::cerr, fmt, ts...);
	fprintf(stderr, "%s\n", zpr::sprint(fmt, ts...).c_str());
	doTheExit();
}
namespace platform { void printStackTrace(); }

template <typename... Ts>
[[noreturn]] inline void compiler_crash(const char* fmt, Ts&&... ts)
{
	fprintf(stderr, "%s\n", zpr::sprint(fmt, ts...).c_str());

	platform::printStackTrace();
	abort();
}


template <typename... Ts>
std::string strprintf(const char* fmt, Ts&&... ts)
{
	// return tinyformat::format(fmt, ts...);
	return zpr::sprint(fmt, ts...);
}


#define __nothing

#ifdef NDEBUG
#define iceAssert(x)    ((void) (x))
#else
#define iceAssert(x)    ((x) ? ((void) (0)) : _error_and_exit("compiler assertion at %s:%d, cause:\n'%s' evaluated to false\n", __FILE__, __LINE__, #x))
#endif

#define TAB_WIDTH       4
#define dcast(t, v)     (dynamic_cast<t*>(v))

namespace util
{
	template<typename K, typename V>
	using hash_map = std::unordered_map<K, V>;
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

template <typename... Ts> std::string strbold(const char* fmt, Ts&&... ts)
{
	return std::string(COLOUR_RESET) + std::string(COLOUR_BLACK_BOLD) + strprintf(fmt, ts...) + std::string(COLOUR_RESET);
}

struct Identifier
{
	Identifier() : name(""), kind(IdKind::Invalid) { }
	Identifier(const std::string& n, IdKind k) : name(n), kind(k) { }

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

	static Location unionOf(const Location& a, const Location& b)
	{
		if(a.fileID != b.fileID || a.line != b.line)
			return a;

		Location ret;
		ret.fileID = a.fileID;
		ret.line = a.line;

		auto end = std::max(a.col + a.len, b.col + b.len);
		if(a.col <= b.col)
		{
			ret.col = a.col;
		}
		else if(b.col < a.col)
		{
			ret.col = b.col;
		}

		ret.len = (end - ret.col);

		return ret;
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


struct BareError;
struct SpanError;
struct SimpleError;
struct OverloadError;
struct ExampleMsg;

//? in order of complexity, i guess?
enum class ErrKind
{
	Bare,           // error without context
	Simple,         // error with context

	Span,           // error with context, but with squiggles within those (general case of
	Overload,       // most complex one; built specifically to handle multiple candidates and such

	Example,        // same as a SimpleError, but we give the "context" as a string.
};

enum class MsgType
{
	Note,
	Warning,
	Error
};

namespace util
{
	template <typename T> struct MemoryPool;
	template <typename T> struct FastInsertVector;

	struct ESpan
	{
		ESpan() { }
		ESpan(const Location& l, const std::string& m) : loc(l), msg(m) { }

		Location loc;
		std::string msg;
		std::string colour;
	};

	BareError* make_BareError(const std::string& m, MsgType t = MsgType::Error);
	SpanError* make_SpanError(SimpleError* se, const std::vector<ESpan>& s = { }, MsgType t = MsgType::Error);
	SimpleError* make_SimpleError(const Location& l, const std::string& m, MsgType t = MsgType::Error);
	OverloadError* make_OverloadError(SimpleError* se, MsgType t = MsgType::Error);
	ExampleMsg* make_ExampleMsg(const std::string& eg, MsgType t = MsgType::Note);


}




struct ErrorMsg
{
	virtual ~ErrorMsg() { }

	virtual void post() = 0;
	virtual ErrorMsg* append(ErrorMsg* e) { this->subs.push_back(e); return this; }
	virtual ErrorMsg* prepend(ErrorMsg* e) { this->subs.insert(this->subs.begin(), e); return this; }

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

	friend struct util::MemoryPool<ErrorMsg>;
	friend struct util::FastInsertVector<ErrorMsg>;
};

struct BareError : ErrorMsg
{
	template <typename... Ts>
	static BareError* make(const char* fmt, Ts&&... ts) { return util::make_BareError(strprintf(fmt, ts...)); }

	template <typename... Ts>
	static BareError* make(MsgType t, const char* fmt, Ts&&... ts) { return util::make_BareError(strprintf(fmt, ts...), t); }

	virtual void post() override;
	virtual BareError* append(ErrorMsg* e) override { this->subs.push_back(e); return this; }
	virtual BareError* prepend(ErrorMsg* e) override { this->subs.insert(this->subs.begin(), e); return this; }

	std::string msg;

	protected:
	BareError() : ErrorMsg(ErrKind::Bare, MsgType::Error) { }
	BareError(const std::string& m, MsgType t) : ErrorMsg(ErrKind::Bare, t), msg(m) { }

	friend struct util::MemoryPool<BareError>;
	friend struct util::FastInsertVector<BareError>;
};

struct SimpleError : ErrorMsg
{
	template <typename... Ts>
	static SimpleError* make(const Location& l, const char* fmt, Ts&&... ts) { return util::make_SimpleError(l, strprintf(fmt, ts...)); }

	template <typename... Ts>
	static SimpleError* make(MsgType t, const Location& l, const char* fmt, Ts&&... ts) { return util::make_SimpleError(l, strprintf(fmt, ts...), t); }




	virtual void post() override;
	virtual SimpleError* append(ErrorMsg* e) override { this->subs.push_back(e); return this; }
	virtual SimpleError* prepend(ErrorMsg* e) override { this->subs.insert(this->subs.begin(), e); return this; }

	Location loc;
	std::string msg;

	// just a hacky thing to print some words (eg. '(call site)') before the context.
	bool printContext = true;
	std::string wordsBeforeContext;

	protected:
	SimpleError() : ErrorMsg(ErrKind::Simple, MsgType::Error) { }
	SimpleError(const Location& l, const std::string& m, MsgType t) : ErrorMsg(ErrKind::Bare, t), loc(l), msg(m) { }

	friend struct util::MemoryPool<SimpleError>;
	friend struct util::FastInsertVector<SimpleError>;
};

struct ExampleMsg : ErrorMsg
{
	static ExampleMsg* make(const std::string& eg) { return util::make_ExampleMsg(eg); }

	virtual void post() override;
	virtual ExampleMsg* append(ErrorMsg* e) override { this->subs.push_back(e); return this; }
	virtual ExampleMsg* prepend(ErrorMsg* e) override { this->subs.insert(this->subs.begin(), e); return this; }

	std::string example;

	protected:
	ExampleMsg() : ErrorMsg(ErrKind::Example, MsgType::Note) { }
	ExampleMsg(const std::string& eg, MsgType t) : ErrorMsg(ErrKind::Example, t), example(eg) { }

	friend struct util::MemoryPool<ExampleMsg>;
	friend struct util::FastInsertVector<ExampleMsg>;
};




struct SpanError : ErrorMsg
{
	SpanError* add(const util::ESpan& s);

	static SpanError* make(SimpleError* se = 0, const std::vector<util::ESpan>& s = { }) { return util::make_SpanError(se, s, MsgType::Error); }
	static SpanError* make(MsgType t, SimpleError* se = 0, const std::vector<util::ESpan>& s = { }) { return util::make_SpanError(se, s, t); }


	virtual void post() override;
	virtual SpanError* append(ErrorMsg* e) override { this->subs.push_back(e); return this; }
	virtual SpanError* prepend(ErrorMsg* e) override { this->subs.insert(this->subs.begin(), e); return this; }

	SimpleError* top = 0;
	std::vector<util::ESpan> spans;

	// again, another internal flag; this one controls whether or not to underline the original location.
	bool highlightActual = true;

	protected:
	SpanError() : SpanError(0, { }, MsgType::Error) { }
	SpanError(SimpleError* se, const std::vector<util::ESpan>& s, MsgType t) : ErrorMsg(ErrKind::Span, t), top(se), spans(s) { }


	friend struct util::MemoryPool<SpanError>;
	friend struct util::FastInsertVector<SpanError>;
};



struct OverloadError : ErrorMsg
{
	void clear();

	static OverloadError* make(SimpleError* se = 0) { return util::make_OverloadError(se, MsgType::Error); }
	static OverloadError* make(MsgType t, SimpleError* se = 0) { return util::make_OverloadError(se, t); }

	virtual void post() override;
	virtual OverloadError* append(ErrorMsg* e) override { this->subs.push_back(e); return this; }
	virtual OverloadError* prepend(ErrorMsg* e) override { this->subs.insert(this->subs.begin(), e); return this; }

	OverloadError& addCand(Locatable* d, ErrorMsg* e);


	SimpleError* top = 0;
	util::hash_map<Locatable*, ErrorMsg*> cands;

	protected:
	OverloadError() : ErrorMsg(ErrKind::Overload, MsgType::Error) { }
	OverloadError(SimpleError* se, MsgType t) : ErrorMsg(ErrKind::Overload, t), top(se) { }

	friend struct util::MemoryPool<OverloadError>;
	friend struct util::FastInsertVector<OverloadError>;
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

	~TCResult() { }

	TCResult(RK k) :  _kind(k)                                  { _st = 0; }
	explicit TCResult(sst::Stmt* s) : _kind(RK::Statement)      { _st = s; }
	explicit TCResult(sst::Expr* e) : _kind(RK::Expression)     { _ex = e; }
	explicit TCResult(sst::Defn* d) : _kind(RK::Definition)     { _df = d; }

	explicit TCResult(ErrorMsg* pe) : _kind(RK::Error) { _pe = pe; }

	TCResult(const TCResult& r)
	{
		this->_kind = r._kind;

		if(this->isError())     this->_pe = r._pe;
		else if(this->isStmt()) this->_st = r._st;
		else if(this->isExpr()) this->_ex = r._ex;
		else if(this->isDefn()) this->_df = r._df;
	}

	TCResult(TCResult&& r) noexcept
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

	TCResult& operator = (TCResult&& r) noexcept
	{
		if(&r != this)
		{
			if(this->isError())     { this->_pe = r._pe; r._pe = 0; }
			else if(this->isStmt()) { this->_st = r._st; r._st = 0; }
			else if(this->isExpr()) { this->_ex = r._ex; r._ex = 0; }
			else if(this->isDefn()) { this->_df = r._df; r._df = 0; }
		}

		return *this;
	}



	ErrorMsg* error() const    { if(this->_kind != RK::Error || !this->_pe) { _error_and_exit("not error\n"); } return this->_pe; }

	sst::Expr* expr() const;
	sst::Defn* defn() const;

	//* stmt() is the most general case -- definitions and expressions are both statements.
	// note: we need the definition of sst::Stmt and sst::Expr to do safe dynamic casting, so it's in identifier.cpp.
	sst::Stmt* stmt() const;

	bool isError() const        { return this->_kind == RK::Error; }
	bool isStmt() const         { return this->_kind == RK::Statement; }
	bool isExpr() const         { return this->_kind == RK::Expression; }
	bool isDefn() const         { return this->_kind == RK::Definition; }
	bool isParametric() const   { return this->_kind == RK::Parametric; }
	bool isDummy() const        { return this->_kind == RK::Dummy; }

	static TCResult getParametric() { return TCResult(RK::Parametric); }
	static TCResult getDummy()      { return TCResult(RK::Dummy); }
};








struct CGResult
{
	enum class VK
	{
		Invalid,

		Normal,
		EarlyOut,
	};

	CGResult() : CGResult(0) { }
	explicit CGResult(fir::Value* v) noexcept : value(v), kind(VK::Normal) { }
	explicit CGResult(fir::Value* v, VK k) noexcept : value(v), kind(k) { }

	fir::Value* operator -> () { return this->value; }

	fir::Value* value = 0;
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

using TypeParamMap_t = util::hash_map<std::string, fir::Type*>;

struct PolyArgMapping_t
{
	struct SingleArg
	{
		size_t index;
		std::string name;

		pts::Type* type;
	};

	std::vector<SingleArg> maps;

	bool empty() const { return maps.empty(); }
	void add(const std::string& name, pts::Type* t);
	void add(size_t idx, pts::Type* t);

	std::string print() const;

	static inline PolyArgMapping_t none() { return PolyArgMapping_t(); }
};


namespace util
{
	std::string typeParamMapToString(const std::string& name, const TypeParamMap_t& map);

	std::string obfuscateName(const std::string& name);
	std::string obfuscateName(const std::string& name, size_t id);
	std::string obfuscateName(const std::string& name, const std::string& extra);
	Identifier obfuscateIdentifier(const std::string& name, IdKind kind = IdKind::Name);
	Identifier obfuscateIdentifier(const std::string& name, size_t id, IdKind kind = IdKind::Name);
	Identifier obfuscateIdentifier(const std::string& name, const std::string& extra, IdKind kind = IdKind::Name);

	template <typename T>
	std::string listToEnglish(const std::vector<T>& list, bool quote = true)
	{
		auto printitem = [quote](const T& i) -> std::string {
			return strprintf("%s%s%s", quote ? "'" : "", i, quote ? "'" : "");
		};

		std::string mstr;
		if(list.size() == 1)
		{
			mstr = printitem(list[0]);
		}
		else if(list.size() == 2)
		{
			mstr = strprintf("%s and %s", printitem(list[0]), printitem(list[1]));
		}
		else
		{
			for(size_t i = 0; i < list.size() - 1; i++)
				mstr += strprintf("%s, ", printitem(list[i]));

			// oxford comma is important.
			mstr += strprintf("and %s", printitem(list.back()));
		}

		return mstr;
	}

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

	extern const std::string TypeCast;
	extern const std::string TypeIs;


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






