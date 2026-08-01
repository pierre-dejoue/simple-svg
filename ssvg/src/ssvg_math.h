#ifndef SVG_SSVG_MATH_H
#define SVG_SSVG_MATH_H

#include <stdutils/constants.h>

#include <limits>

namespace ssvg
{
namespace math
{

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

} // namespace math
} // namespace ssvg

#endif
