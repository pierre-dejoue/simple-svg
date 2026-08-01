// Copyright (c) 2023 Pierre DEJOUE
// This code is distributed under the terms of the MIT License
#pragma once

// Inspiration taken from C++20: https://en.cppreference.com/w/cpp/header/numbers.html
namespace stdutils {
namespace constants {

template <typename F>
inline constexpr F e_v                          = F{};  // Invalid
template <typename F>
inline constexpr F pi_v                         = F{};  // Invalid
template <typename F>
inline constexpr F two_pi_v                     = F{};  // Invalid

template <>
inline constexpr double e_v<double>             = 2.718281828459045;
template <>
inline constexpr double pi_v<double>            = 3.141592653589793;
template <>
inline constexpr double two_pi_v<double>        = 2.0 * pi_v<double>;

template <>
inline constexpr float e_v<float>               = static_cast<float>(e_v<double>);
template <>
inline constexpr float pi_v<float>              = static_cast<float>(pi_v<double>);
template <>
inline constexpr float two_pi_v<float>          = static_cast<float>(two_pi_v<double>);

inline constexpr double e                       = e_v<double>;
inline constexpr double pi                      = pi_v<double>;
inline constexpr double two_pi                  = two_pi_v<double>;

} // namespace constants
} // namespace stdutils
