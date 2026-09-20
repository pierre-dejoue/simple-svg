#ifndef SSVG_SSVG_MATH_H
#define SSVG_SSVG_MATH_H

#include <stdutils/constants.h>

#include <cmath>
#include <limits>

namespace ssvg {
namespace math {

inline constexpr float kFloatMax = std::numeric_limits<float>::max();           // FLT_MAX

inline constexpr float kPi     = stdutils::constants::pi_v<float>;
inline constexpr float kPi2    = stdutils::constants::two_pi_v<float>;
inline constexpr float kPiHalf = stdutils::constants::pi_v<float> / 2.f;

inline constexpr float to_rad(float deg)
{
	return deg * (kPi / 180.f);
}

inline constexpr float sign(float a)
{
	return static_cast<float>( (0.0f < a) - (0.0f > a) );
}

inline float normalizedDiagonal(float w, float h)
{
	return std::sqrt(w * w + h * h) / std::sqrt(2.f);
}

// Returns the number of roots
inline uint32_t solveQuad(float a, float b, float c, float* t)
{
	assert(t);
	t[0] = 0.f;
	t[1] = 0.f;

	if (std::abs(a) < 1e-5f) {
		if (std::abs(b) > 1e-5f) {
			t[0] = -c / b;

			return 1;
		}
	} else {
		const float desc = b * b - 4.0f * a * c;
		if (std::abs(desc) > 1e-5f) {
			const float desc_sqrt = std::sqrt(desc);
			t[0] = (-b + desc_sqrt) / (2.0f * a);
			t[1] = (-b - desc_sqrt) / (2.0f * a);

			return 2;
		}
	}

	// No root
	return 0;
}

} // namespace math
} // namespace ssvg

#endif      // SSVG_SSVG_MATH_H
