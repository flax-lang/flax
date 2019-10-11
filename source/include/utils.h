// utils.h
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include <map>
#include <string>
#include <vector>
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

	ret.insert(ret.begin(), b.begin(), b.end());
	return ret;
}


namespace util
{
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

	template <typename T>
	std::vector<T> merge(const std::vector<T>& x)
	{
		return x;
	}

	template <typename T, typename... Args>
	std::vector<T> merge(const std::vector<T>& x, const Args&... xs)
	{
		return x + merge(xs...);
	}





	template <typename T, class UnaryOp, typename K = typename std::result_of<UnaryOp(T)>::type>
	std::vector<K> map(const std::vector<T>& input, UnaryOp fn)
	{
		std::vector<K> ret;
		for(const auto& i : input)
			ret.push_back(fn(i));

		return ret;
	}

	template <typename T, class UnaryOp>
	void foreach(const std::vector<T>& input, UnaryOp fn)
	{
		for(const auto& i : input)
			fn(i);
	}

	template <typename T, class UnaryOp>
	void foreachIdx(const std::vector<T>& input, UnaryOp fn)
	{
		for(size_t i = 0; i < input.size(); i++)
			fn(input[i], i);
	}


	template <typename T, class UnaryOp, typename K = typename std::result_of<UnaryOp(T, size_t)>::type>
	std::vector<K> mapidx(const std::vector<T>& input, UnaryOp fn)
	{
		std::vector<K> ret;
		for(size_t i = 0; i < input.size(); i++)
			ret.push_back(fn(input[i], i));

		return ret;
	}



	template <typename T, class UnaryOp, class Predicate, typename K = typename std::result_of<UnaryOp(T)>::type>
	std::vector<K> filterMap(const std::vector<T>& input, Predicate cond, UnaryOp fn)
	{
		std::vector<K> ret;
		for(const auto& i : input)
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
		for(const auto& i : input)
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

	template <typename T>
	std::vector<T> take(const std::vector<T>& v, size_t num)
	{
		return std::vector<T>(v.begin(), v.begin() + std::min(num, v.size()));
	}

	template <typename T>
	std::vector<T> drop(const std::vector<T>& v, size_t num)
	{
		return std::vector<T>(v.begin() + std::min(num, v.size()), v.end());
	}

	template <typename T, typename U>
	std::vector<std::pair<T, U>> cartesian(const std::vector<T>& a, const std::vector<U>& b)
	{
		std::vector<std::pair<T, U>> ret;

		for(size_t i = 0; i < a.size(); i++)
			for(size_t k = 0; k < b.size(); k++)
				ret.push_back({ a[i], b[k] });

		return ret;
	}

	inline std::string join(const std::vector<std::string>& list, const std::string& sep)
	{
		if(list.empty())            return "";
		else if(list.size() == 1)   return list[0];

		std::string ret;
		for(size_t i = 0; i < list.size() - 1; i++)
			ret += list[i] + sep;

		return ret + list.back();
	}





	inline std::string serialiseScope(const std::vector<std::string>& scope)
	{
		if(scope.empty()) return "";

		std::string ret = scope[0];
		for(size_t i = 1; i < scope.size(); i++)
			ret += "::" + scope[i];

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

	template <typename K, typename V>
	std::vector<std::pair<K, V>> pairs(const std::map<K, V>& map)
	{
		auto ret = std::vector<std::pair<K, V>>(map.begin(), map.end());
		return ret;
	}
}



