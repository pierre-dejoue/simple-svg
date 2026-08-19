#ifndef SSVG_SSVG_PRIVATE_H
#define SSVG_SSVG_PRIVATE_H

#include <ssvg/ssvg.h>

#include <string_view>
#include <utility>

//
// The private part of the SSVG API
//
namespace ssvg {

std::string_view lengthUnitToString(LengthUnit::Enum lengthUnit);

struct LengthContext
{
	// All lengths below in pixels
	float m_ViewportWidth;
	float m_ViewportHeight;
	float m_ViewportDiag;
	float m_FontSize;
};

struct LengthAxis {
	enum Enum : uint32_t
	{
		X,
		Y,
		Radial,
	};
};

float convertLengthToPixel(const Length& length, LengthAxis::Enum axis = LengthAxis::X, const LengthContext* lengthContext = nullptr);

} // namespace ssvg

#endif      // SSVG_SSVG_PRIVATE_H
