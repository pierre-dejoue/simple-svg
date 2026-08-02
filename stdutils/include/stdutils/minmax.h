// Copyright (c) 2026 Pierre DEJOUE
// This code is distributed under the terms of the MIT License
#pragma once

#include <cassert>
#include <utility>

/**
 * Goal: Do not include <algorithm> whenever you need std::min, std::max on two values, or std::clamp
 *
 * Notes:
 *  - We follow the convention in STL that in case of equality std::min and std::max favor the first argument.
 *  - The convention for clamping is to return the reference to the clamped value if it is not strictly outside the bounds.
 */
namespace stdutils {

// Min/max
template <class T>
constexpr const T& min(const T& a, const T& b)               { return    (b < a) ? b : a; }
template <class T, class Compare>
constexpr const T& min(const T& a, const T& b, Compare comp) { return comp(b, a) ? b : a; }

template <class T>
constexpr const T& max(const T& a, const T& b)               { return    (a < b) ? b : a; }
template <class T, class Compare>
constexpr const T& max(const T& a, const T& b, Compare comp) { return comp(a, b) ? b : a; }

template <class T>
constexpr std::pair<const T&, const T&> minmax(const T& a, const T& b)               { return    (b < a) ? std::pair<const T&, const T&>(b, a) : std::pair<const T&, const T&>(a, b); }
template <class T, class Compare>
constexpr std::pair<const T&, const T&> minmax(const T& a, const T& b, Compare comp) { return comp(b, a) ? std::pair<const T&, const T&>(b, a) : std::pair<const T&, const T&>(a, b); }


// Min/max on triples
template <class T>
constexpr const T& min_triple(const T& a, const T& b, const T& c)               { return stdutils::min<T>(a, stdutils::min<T>(b, c)); }
template <class T, class Compare>
constexpr const T& min_triple(const T& a, const T& b, const T& c, Compare comp) { return stdutils::min<T>(a, stdutils::min<T>(b, c, comp), comp); }

template <class T>
constexpr const T& max_triple(const T& a, const T& b, const T& c)               { return stdutils::max<T>(a, stdutils::max<T>(b, c)); }
template <class T, class Compare>
constexpr const T& max_triple(const T& a, const T& b, const T& c, Compare comp) { return stdutils::max<T>(a, stdutils::max<T>(b, c, comp), comp); }

template <class T>
constexpr std::pair<const T&, const T&> minmax_triple(const T& a, const T& b, const T& c)               { return std::pair<const T&, const T&>(stdutils::min_triple<T>(a, b, c),       stdutils::max_triple<T>(a, b, c)); }
template <class T, class Compare>
constexpr std::pair<const T&, const T&> minmax_triple(const T& a, const T& b, const T& c, Compare comp) { return std::pair<const T&, const T&>(stdutils::min_triple<T>(a, b, c, comp), stdutils::max_triple<T>(a, b, c, comp)); }


// Min/max updates
template <class T>
constexpr void max_update(T& to, const T& from)               { to = stdutils::max(to, from); }
template <class T, class Compare>
constexpr void max_update(T& to, const T& from, Compare comp) { to = stdutils::max(to, from, comp); }

template <class T>
constexpr void min_update(T& to, const T& from)               { to = stdutils::min(to, from); }
template <class T, class Compare>
constexpr void min_update(T& to, const T& from, Compare comp) { to = stdutils::min(to, from, comp); }

template <class T>
constexpr void minmax_update(std::pair<T, T>& to, const T& from)               { assert(!(to.second < to.first));    if      (from < to.first) { to.first = from; } else if      (to.second < from) { to.second = from; } }
template <class T, class Compare>
constexpr void minmax_update(std::pair<T, T>& to, const T& from, Compare comp) { assert(!comp(to.second, to.first)); if (comp(from, to.first)) { to.first = from; } else if (comp(to.second, from)) { to.second = from; } }


// Clamp
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)               { assert(!(hi < lo));     return      v < lo ? lo : (     hi < v ? hi : v); }
template <class T, class Compare>
constexpr const T& clamp(const T& v, const T& lo, const T& hi, Compare comp) { assert(!comp(hi, lo));  return comp(v, lo) ? lo : (comp(hi, v) ? hi : v); }


// Like clamp but with an output boolean flag 'clamped'
// NB:
// - The value is clamped (i.e. clamped = true) if and only if it is strictly outside the range [lo; hi]
// - Those functions cannot be constexpr
template <class T>
const T& clamp(const T& v, const T& lo, const T& hi, bool& clamped)               { assert(lo <= hi);      const T& r = stdutils::clamp<T>(v, lo, hi);       clamped = (r != v); return r; }
template <class T, class Compare>
const T& clamp(const T& v, const T& lo, const T& hi, bool& clamped, Compare comp) { assert(!comp(hi, lo)); const T& r = stdutils::clamp<T>(v, lo, hi, comp); clamped = (r != v); return r; }

} // namespace stdutils
