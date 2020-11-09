// zfu.h
// Copyright (c) 2020, zhiayang
// Licensed under the Apache License Version 2.0.

// updated 10/06/2020
// origins:
// flax     -- post 16/06/2020
// ikurabot -- pre  16/06/2020

#pragma once
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <unordered_map>

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

template <typename T>
std::vector<T> operator + (const std::vector<T>& a, const std::vector<T>& b)
{
	auto ret = a;
	ret.insert(ret.end(), b.begin(), b.end());

	return ret;
}


template <typename T>
std::vector<T>& operator += (std::vector<T>& vec, const T& elm)
{
	vec.push_back(elm);
	return vec;
}

template <typename T>
std::vector<T>& operator += (std::vector<T>& vec, const std::vector<T>& xs)
{
	vec.insert(vec.end(), xs.begin(), xs.end());
	return vec;
}


namespace zfu
{
	template <typename T>
	bool match(const T&)
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

	template <typename T, typename... Args>
	std::vector<T> merge(const std::vector<T>& x, const Args&... xs)
	{
		return (x + ... + xs);
	}



	template <typename T>
	std::vector<T> vectorOf(const T& x)
	{
		return std::vector<T>({ x });
	}

	template <typename T, typename... Args>
	std::vector<T> vectorOf(const T& x, const Args&... xs)
	{
		return x + vectorOf<T>(xs...);
	}

	template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
	std::vector<T> rangeOpen(const T& begin, const T& end, const T& step = 1)
	{
		std::vector<T> ret;
		ret.reserve((end - begin + 1) / step);

		T x = begin;
		while(x != end)
		{
			ret.push_back(x);
			x += step;
		}

		return ret;
	}

	template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
	std::vector<T> rangeClosed(const T& begin, const T& end, const T& step = 1)
	{
		return rangeOpen<T>(begin, end + 1, step);
	}

	template <typename T, typename Predicate, typename UnaryOp>
	std::vector<T> iterateWhile(const T& seed, Predicate pred, UnaryOp fn)
	{
		T x = seed;
		std::vector<T> ret;

		while(pred(x))
		{
			ret.push_back(x);
			x = fn(x);
		}

		return ret;
	}



	template <typename Container, typename U, typename FoldOp>
	U foldl(const U& i, const Container& xs, FoldOp fn)
	{
		auto ret = i;
		for(const auto& x : xs)
			ret = fn(ret, x);

		return ret;
	}

	template <typename Container, typename T = typename Container::value_type>
	T sum(const Container& xs)
	{
		return foldl(T(), xs, [](const T& a, const T& b) -> T { return a + b; });
	}



	template <typename Container, typename UnaryOp, typename T = typename Container::value_type>
	auto map(const Container& input, UnaryOp fn) -> std::vector<decltype(fn(std::declval<T>()))>
	{
		std::vector<decltype(fn(std::declval<T>()))> ret;
		ret.reserve(input.size());

		for(const auto& i : input)
			ret.push_back(fn(i));

		return ret;
	}

	template <typename Container, typename UnaryOp, typename T = typename Container::value_type>
	auto map(Container&& input, UnaryOp fn) -> std::vector<decltype(fn(std::move(std::declval<T>())))>
	{
		std::vector<decltype(fn(std::move(std::declval<T>())))> ret;
		ret.reserve(input.size());

		for(auto& i : input)
			ret.push_back(fn(std::move(i)));

		return ret;
	}

	template <typename Container, typename UnaryOp, typename T = typename Container::value_type>
	auto flatmap(const Container& input, UnaryOp fn) -> std::vector<decltype(fn(std::declval<T>()))>
	{
		std::vector<decltype(fn(std::declval<T>()))> ret;
		ret.reserve(input.size());

		for(const auto& i : input)
		{
			auto x = fn(i);
			ret.insert(ret.end(), x.begin(), x.end());
		}

		return ret;
	}






	template <typename Container, typename UnaryOp>
	void foreach(const Container& input, UnaryOp fn)
	{
		for(const auto& i : input)
			fn(i);
	}


	template <typename Container, typename UnaryOp>
	void foreachWhile(const Container& input, UnaryOp fn)
	{
		for(const auto& i : input)
			if(!fn(i)) break;
	}

	template <typename Container, typename UnaryOp>
	void foreachIdx(const Container& input, UnaryOp fn)
	{
		for(size_t i = 0; i < input.size(); i++)
			fn(input[i], i);
	}


	template <typename Container, typename UnaryOp, typename T = typename Container::value_type>
	auto mapIdx(const Container& input, UnaryOp fn) -> std::vector<decltype(fn(std::declval<T>(), static_cast<size_t>(0)))>
	{
		std::vector<decltype(fn(std::declval<T>(), static_cast<size_t>(0)))> ret;
		ret.reserve(input.size());

		for(size_t i = 0; i < input.size(); i++)
			ret.push_back(fn(input[i], i));

		return ret;
	}



	template <typename Container, typename UnaryOp, typename Predicate, typename T = typename Container::value_type>
	auto filterMap(const Container& input, Predicate cond, UnaryOp fn) -> std::vector<decltype(fn(std::declval<T>()))>
	{
		std::vector<decltype(fn(std::declval<T>()))> ret;
		for(const auto& i : input)
			if(cond(i)) ret.push_back(fn(i));

		return ret;
	}

	template <typename Container, typename UnaryOp, typename Predicate, typename T = typename Container::value_type>
	auto mapFilter(const Container& input, UnaryOp fn, Predicate cond) -> std::vector<decltype(fn(std::declval<T>()))>
	{
		std::vector<decltype(fn(std::declval<T>()))> ret;
		for(const auto& i : input)
		{
			auto k = fn(i);
			if(cond(k)) ret.push_back(k);
		}

		return ret;
	}

	template <typename Container, typename Predicate>
	Container filter(const Container& input, Predicate cond)
	{
		Container ret;
		for(const auto& i : input)
			if(cond(i))
				ret.push_back(i);

		return ret;
	}

	template <typename Container, typename Predicate>
	bool matchAny(const Container& input, Predicate cond)
	{
		for(const auto& x : input)
			if(cond(x)) return true;

		return false;
	}

	template <typename Container, typename Predicate>
	bool matchNone(const Container& input, Predicate cond)
	{
		return !matchAny(input, cond);
	}

	template <typename Container, typename Predicate>
	bool matchAll(const Container& input, Predicate cond)
	{
		for(const auto& x : input)
			if(!cond(x)) return false;

		return true;
	}

	template <typename Container, typename Predicate>
	size_t indexOf(const Container& input, Predicate cond)
	{
		for(size_t i = 0; i < input.size(); i++)
			if(cond(input[i])) return i;

		return -1;
	}

	template <typename Container, typename U>
	bool contains(const Container& input, const U& x)
	{
		return std::find(input.begin(), input.end(), x) != input.end();
	}

	template <typename T>
	std::vector<T> take(const std::vector<T>& v, size_t num)
	{
		return std::vector<T>(v.begin(), v.begin() + std::min(num, v.size()));
	}

	template <typename T, typename Predicate>
	std::vector<T> takeWhile(const std::vector<T>& input, Predicate cond)
	{
		std::vector<T> ret;
		for(const auto& i : input)
		{
			if(cond(i)) ret.push_back(i);
			else        break;
		}

		return ret;
	}

	template <typename T>
	std::vector<T> drop(const std::vector<T>& v, size_t num)
	{
		return std::vector<T>(v.begin() + std::min(num, v.size()), v.end());
	}

	template <typename T, typename Predicate>
	std::vector<T> dropWhile(const std::vector<T>& input, Predicate cond)
	{
		bool flag = false;
		std::vector<T> ret;

		for(const auto& i : input)
		{
			if(!flag && cond(i))    continue;
			else                    flag = true;

			ret.push_back(i);
		}

		return ret;
	}



	template <typename T, typename GroupFn, typename R = decltype(std::declval<GroupFn(T)>())>
	std::map<R, std::vector<T>> groupBy(const std::vector<T>& xs, GroupFn gfn)
	{
		std::map<R, std::vector<T>> groups;
		for(const T& x : xs)
			groups[gfn(x)].push_back(x);

		return groups;
	}

	// special case for one to many. so cartesian(1, { 1, 2, 3, 4 }) gives (1, 1), (1, 2), (1, 3), ...
	template <typename T, typename U>
	std::vector<std::pair<T, U>> cartesian(const T& a, const std::vector<U>& xs)
	{
		return map(xs, [&a](const U& x) -> auto {
			return std::make_pair(a, x);
		});
	}

	// special case for two vectors, because tuples are a major pain in the ass. pairs >>> tuples.
	template <typename T, typename U>
	std::vector<std::pair<T, U>> cartesian(const std::vector<T>& a, const std::vector<U>& b)
	{
		std::vector<std::pair<T, U>> ret;

		for(size_t i = 0; i < a.size(); i++)
			for(size_t k = 0; k < b.size(); k++)
				ret.push_back({ a[i], b[k] });

		return ret;
	}

	template <typename F>
	inline void cross_imp(F f) { f(); }

	template <typename F, typename H, typename... Ts>
	inline void cross_imp(F f, const std::vector<H>& h, const std::vector<Ts>&... t)
	{
		for(const H& hs : h)
			cross_imp([&](const Ts&... ts) { f(hs, ts...); }, t...);
	}

	template <typename... Ts>
	std::vector<std::tuple<Ts...>> cartesian(const std::vector<Ts>&... in)
	{
		std::vector<std::tuple<Ts...>> res;

		cross_imp([&](Ts const&... ts) {
			res.emplace_back(ts...);
		}, in...);

		return res;
	}




	template <typename T, bool permute_inside>
	std::vector<std::vector<T>> _permutations(const std::vector<T>& xs, size_t r)
	{
		if(r == 0) return { };

		auto fact = [](size_t x) -> size_t {
			size_t ret = 1;
			while(x > 1)
				ret *= x, x -= 1;

			return ret;
		};

		std::vector<std::vector<T>> ret;
		if(permute_inside)  ret.reserve(fact(xs.size()) / fact(xs.size() - r));
		else                ret.reserve(fact(xs.size()) / (fact(r) * fact(xs.size() - r)));


		std::function<void (size_t, size_t, size_t, std::vector<T>)> recurse;

		recurse = [&recurse, &xs, &ret](size_t R, size_t r, size_t i, std::vector<T> cur) {
			if(r == R)
			{
				if(permute_inside)
				{
					do {
						ret.push_back(cur);
					} while(std::next_permutation(cur.begin(), cur.end()));
				}
				else
				{
					std::sort(cur.begin(), cur.end());
					ret.push_back(cur);
				}
			}
			else
			{
				for(size_t k = i; k < xs.size(); k++)
				{
					cur.push_back(xs[k]);
					recurse(R, r + 1, k + 1, cur);
					cur.pop_back();
				}
			}
		};

		recurse(r, 0, 0, { });
		return ret;
	}

	template <typename T>
	std::vector<std::vector<T>> powerset(const std::vector<T>& xs)
	{
		std::vector<std::vector<T>> ret;

		// well... if there's more than 64 elements then gg
		ret.reserve(2 << xs.size());

		for(size_t i = 0; i < xs.size(); i++)
		{
			auto x = _permutations<T, false>(xs, i);
			ret.insert(ret.end(), x.begin(), x.end());
		}

		return ret;
	}

	template <typename T>
	std::vector<std::vector<T>> combinations(const std::vector<T>& xs, size_t r)
	{
		return _permutations<T, false>(xs, r);
	}

	template <typename T>
	std::vector<std::vector<T>> permutations(const std::vector<T>& xs, size_t r)
	{
		return _permutations<T, true>(xs, r);
	}

	template <typename T>
	std::vector<std::vector<T>> permutations(const std::vector<T>& xs)
	{
		return _permutations<T, true>(xs, xs.size());
	}





	template <typename T, typename U>
	std::vector<std::pair<T, U>> zip(const std::vector<T>& a, const std::vector<U>& b)
	{
		std::vector<std::pair<T, U>> ret;
		for(size_t i = 0; i < std::min(a.size(), b.size()); i++)
			ret.push_back({ a[i], b[i] });

		return ret;
	}

	static inline std::string join(const std::vector<std::string>& list, const std::string& sep)
	{
		if(list.empty())            return "";
		else if(list.size() == 1)   return list[0];

		std::string ret;
		for(size_t i = 0; i < list.size() - 1; i++)
			ret += list[i] + sep;

		return ret + list.back();
	}





	static inline std::string serialiseScope(const std::vector<std::string>& scope)
	{
		if(scope.empty()) return "";

		std::string ret = scope[0];
		for(size_t i = 1; i < scope.size(); i++)
			ret += "::" + scope[i];

		return ret;
	}

	static inline std::string plural(const std::string& thing, size_t count)
	{
		return thing + (count == 1 ? "" : "s");
	}

	template <typename Container, typename UnaryOp>
	std::string listToString(const Container& list, UnaryOp fn, bool braces = true, const std::string& sep = ", ")
	{
		std::string ret;
		for(size_t i = 0; i < list.size(); i++)
		{
			ret += fn(list[i]);
			if(i != list.size() - 1)
				ret += sep;
		}

		return braces ? ("[ " + ret + " ]") : ret;
	}

	template <typename K, typename V>
	std::vector<std::pair<K, V>> pairs(const std::unordered_map<K, V>& map)
	{
		auto ret = std::vector<std::pair<K, V>>(map.begin(), map.end());
		return ret;
	}

	template <typename K, typename V>
	std::vector<std::pair<K, V>> pairs(const std::map<K, V>& map)
	{
		auto ret = std::vector<std::pair<K, V>>(map.begin(), map.end());
		return ret;
	}


	struct identity
	{
		template <typename T>
		T&& operator() (T&& x) { return std::forward<T>(x); }
	};

	struct tostring
	{
		template <typename T>
		std::string operator() (const T& x) { return std::to_string(x); }
	};

	struct pair_first
	{
		template <typename T, typename U>
		T operator() (const std::pair<T, U>& x) { return x.first; }
	};

	struct pair_second
	{
		template <typename T, typename U>
		U operator() (const std::pair<T, U>& x) { return x.second; }
	};

	template <typename T>
	struct equals_to
	{
		equals_to(const T& x) : value(x) { }
		bool operator() (const T& x) { return x == this->value; }

	private:
		const T& value;
	};
}















