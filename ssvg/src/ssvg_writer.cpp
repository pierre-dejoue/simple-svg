#include <ssvg/ssvg.h>

#include "ssvg_debug.h"

#include <stdutils/macros.h>
#include <stdutils/memory.h>

#include <cstdio>
#include <cstring>
#include <ostream>

namespace ssvg {

namespace {

struct SaveAttr
{
	enum Enum : uint32_t
	{
		None        = 0,
		ID          = 1 << 0,
		Transform   = 1 << 1,
		Stroke      = 1 << 2,
		Fill        = 1 << 3,
		Font        = 1 << 4,
		Class       = 1 << 5,
		Opacity     = 1 << 6,
		All         =(1 << 7) - 1,

		// Special flag
		ConditionalPaints = 0x80000000, // If set, and PaintType == None || Transparent don't save stroke-width, stroke-opacity, etc.

		// Common combinations
		Unique = Transform | ID,
		Shape = Unique | Stroke | Fill,
		Text = Unique | Fill | Font | ConditionalPaints,
	};
};

const char* baseProfileToString(BaseProfile::Enum bp)
{
	switch (bp) {
	case BaseProfile::Basic:
		return "basic";
	case BaseProfile::Full:
		return "full";
	case BaseProfile::Tiny:
		return "tiny";
	default:
		break;
	}

	SSVG_WARN(false, "Unknown baseProfile value");

	return "full";
}

const char* lineJoinToString(LineJoin::Enum join)
{
	switch (join) {
	case LineJoin::Miter:
		return "miter";
	case LineJoin::Bevel:
		return "bevel";
	case LineJoin::Round:
		return "round";
	}

	SSVG_WARN(false, "Unknown stroke-linejoin value");

	return "miter";
}

const char* lineCapToString(LineCap::Enum cap)
{
	switch (cap) {
	case LineCap::Butt:
		return "butt";
	case LineCap::Square:
		return "square";
	case LineCap::Round:
		return "Round";
	}

	SSVG_WARN(false, "Unknown stroke-linecap value");

	return "butt";
}

const char* textAnchorToString(TextAnchor::Enum anchor)
{
	switch (anchor) {
	case TextAnchor::Start:
		return "start";
	case TextAnchor::Middle:
		return "middle";
	case TextAnchor::End:
		return "end";
	}

	SSVG_WARN(false, "Unknown text-anchor value");

	return "start";
}

const char* fillRuleToString(FillRule::Enum rule)
{
	switch (rule) {
	case FillRule::NonZero:
		return "nonzero";
	case FillRule::EvenOdd:
		return "evenodd";
	}

	SSVG_WARN(false, "Unknown fill-rule value");

	return "nonzero";
}

bool transformIsIdentity(const float* transform)
{
	return transform[0] == 1.0f
		&& transform[1] == 0.0f
		&& transform[2] == 0.0f
		&& transform[3] == 1.0f
		&& transform[4] == 0.0f
		&& transform[5] == 0.0f;
}

void colorToHexString(char* str, uint32_t len, uint32_t abgr)
{
	const uint32_t r = abgr & 0x000000FF;
	const uint32_t g = (abgr >> 8) & 0x000000FF;
	const uint32_t b = (abgr >> 16) & 0x000000FF;

	std::snprintf(str, len, "#%02X%02X%02X", r, g, b);
}

class StreamWriter
{
public:
	StreamWriter(std::ostream& out, uint32_t str_len)
		: m_out{out}
		, m_str{nullptr}
		, m_str_len{str_len}
	{
		assert(str_len > 1);
		m_str = (char*)std::malloc(str_len);
		IGNORE_RETURN stdutils::memset<char>(m_str, str_len, 0, str_len);
	}

	~StreamWriter()
	{
		std::free(m_str);
		m_str = nullptr;
	}

	std::ostream& out() const noexcept { return m_out; }

	template <typename... Args>
	bool write(const char* format, Args&&... args);

	void indent(uint32_t n);

private:
	std::ostream& m_out;
	char*         m_str;
	uint32_t      m_str_len;
};

template <typename... Args>
bool StreamWriter::write(const char* format, Args&&... args)
{
	assert(m_str);
    const int written = std::snprintf(m_str, m_str_len, format, std::forward<Args>(args)...);
	const bool success = 0 <= written && written < m_str_len;
	if (success) { m_out << m_str; }
	return success;
}

void StreamWriter::indent(uint32_t n)
{
	for (uint32_t c = 0; c < n; c++) {
		m_out.put(' ');
	}
}

bool writeShapeAttributes(StreamWriter& writer, const ShapeAttributes* attrs, const ShapeAttributes* parentAttrs, uint32_t flags)
{
	const bool conditionalPaints = (flags & SaveAttr::ConditionalPaints) != 0;

	if ((flags & SaveAttr::ID) != 0 && attrs->m_ID[0] != '\0') {
		writer.out() << "id=\"" << attrs->m_ID << "\" ";
	}

#if SSVG_CONFIG_CLASS_MAX_LEN
	if ((flags & SaveAttr::Class) != 0 && attrs->m_Class[0] != '\0') {
		writer.out() << "class=\"" << attrs->m_Class << "\" ";
	}
#endif

	if ((flags & SaveAttr::Transform) != 0 && !transformIsIdentity(&attrs->m_Transform[0])) {
		writer.write("transform=\"matrix(%g,%g,%g,%g,%g,%g)\" "
			, attrs->m_Transform[0]
			, attrs->m_Transform[1]
			, attrs->m_Transform[2]
			, attrs->m_Transform[3]
			, attrs->m_Transform[4]
			, attrs->m_Transform[5]);
	}

	if ((flags & SaveAttr::Stroke) != 0) {
		const PaintType::Enum strokeType = attrs->m_StrokePaint.m_Type;
		const PaintType::Enum parentStrokeType = parentAttrs->m_StrokePaint.m_Type;

		if (strokeType == PaintType::None && parentStrokeType != PaintType::None) {
			writer.out() << "stroke=\"none\" ";
		} else if (strokeType == PaintType::Transparent && parentStrokeType != PaintType::Transparent) {
			writer.out() << "stroke=\"transparent\" ";
		} else if (strokeType == PaintType::Color) {
			const uint32_t abgr = attrs->m_StrokePaint.m_ColorABGR;
			if (parentStrokeType != PaintType::Color || parentAttrs->m_StrokePaint.m_ColorABGR != abgr) {
				char hexColor[32];
				colorToHexString(hexColor, 32, abgr);
				writer.out() << "stroke=\"" << hexColor << "\" ";
			}
		}

		const bool saveExtra = !conditionalPaints || (strokeType != PaintType::None && strokeType != PaintType::Transparent);
		if (saveExtra) {
			const float miterLimit = attrs->m_StrokeMiterLimit;
			if (miterLimit >= 1.0f && parentAttrs->m_StrokeMiterLimit != miterLimit) {
				writer.write("stroke-miterlimit=\"%g\" ", miterLimit);
			}

			const float width = attrs->m_StrokeWidth;
			if (width >= 0.0f && parentAttrs->m_StrokeWidth != width) {
				writer.write("stroke-width=\"%g\" ", width);
			}

			const float opacity = attrs->m_StrokeOpacity;
			if (opacity >= 0.0f && opacity <= 1.0f && parentAttrs->m_StrokeOpacity != opacity) {
				writer.write("stroke-opacity=\"%g\" ", opacity);
			}

			const LineJoin::Enum lineJoin = attrs->m_StrokeLineJoin;
			if (lineJoin != parentAttrs->m_StrokeLineJoin) {
				writer.out() << "stroke-linejoin=\"" << lineJoinToString(lineJoin) << "\" ";
			}

			const LineCap::Enum lineCap = attrs->m_StrokeLineCap;
			if (lineCap != parentAttrs->m_StrokeLineCap) {
				writer.out() << "stroke-linecap=\"" << lineCapToString(lineCap) << "\" ";
			}
		}
	}

	if ((flags & SaveAttr::Fill) != 0) {
		const PaintType::Enum fillType = attrs->m_FillPaint.m_Type;
		const PaintType::Enum parentFillType = parentAttrs->m_FillPaint.m_Type;

		if (fillType == PaintType::None && parentFillType != PaintType::None) {
			writer.out() << "fill=\"none\" ";
		} else if (fillType == PaintType::Transparent && parentFillType != PaintType::Transparent) {
			writer.out() << "fill=\"transparent\" ";
		} else if (fillType == PaintType::Color) {
			const uint32_t abgr = attrs->m_FillPaint.m_ColorABGR;
			if (parentFillType != PaintType::Color || parentAttrs->m_FillPaint.m_ColorABGR != abgr) {
				char hexColor[32];
				colorToHexString(hexColor, 32, abgr);
				writer.out() << "fill=\"" << hexColor << "\" ";
			}
		}

		const bool saveExtra = !conditionalPaints || (fillType != PaintType::None && fillType != PaintType::Transparent);
		if (saveExtra) {
			const float opacity = attrs->m_FillOpacity;
			if (opacity >= 0.0f && opacity <= 1.0f && opacity != parentAttrs->m_FillOpacity) {
				writer.write("fill-opacity=\"%g\" ", opacity);
			}

			if (attrs->m_FillRule != parentAttrs->m_FillRule) {
				writer.out() << "fill-rule=\"" << fillRuleToString(attrs->m_FillRule) << "\" ";
			}
		}
	}

	if ((flags & SaveAttr::Font) != 0) {
		const char* fontFamily = attrs->m_FontFamily;
		if (fontFamily[0] != '\0' && std::strncmp(fontFamily, &parentAttrs->m_FontFamily[0], SSVG_CONFIG_FONT_FAMILY_MAX_LEN)) {
			writer.out() << "font-family=\"" << fontFamily << "\" ";
		}

		const float fontSize = attrs->m_FontSize;
		if (fontSize > 0.0f && fontSize != parentAttrs->m_FontSize) {
			writer.write("font-size=\"%g\" ", fontSize);
		}
	}

	if ((flags & SaveAttr::Opacity) != 0 && attrs->m_Opacity != parentAttrs->m_Opacity) {
		writer.write("opacity=\"%g\" ", attrs->m_Opacity);
	}

	return true;
}

bool writePointList(StreamWriter& writer, const PointList* pointList)
{
	writer.out() << "points=\"";
	const uint32_t numPoints = pointList->m_NumPoints;
	for (uint32_t i = 0; i < numPoints; ++i) {
		const float* coords = &pointList->m_Coords[i * 2];
		writer.write("%g,%g ", coords[0], coords[1]);
	}
	writer.out() << "\" ";

	return true;
}

bool writePath(StreamWriter& writer, const Path* path)
{
	writer.out() << "d=\"";
	// TODO: Extra minification can be achieved by using relative commands
	// (because adjacent commands/coords are usually close to the last position).
	const uint32_t numCommands = path->m_NumCommands;
	for (uint32_t iCmd = 0; iCmd < numCommands; ++iCmd) {
		const PathCmd* cmd = &path->m_Commands[iCmd];

		const PathCmdType::Enum type = cmd->m_Type;
		const float* data = &cmd->m_Data[0];
		switch (type) {
		case PathCmdType::MoveTo:
			writer.write("M%g %g", data[0], data[1]);
			break;
		case PathCmdType::LineTo:
			writer.write("L%g %g", data[0], data[1]);
			break;
		case PathCmdType::CubicTo:
			writer.write("C%g %g,%g %g,%g %g", data[0], data[1], data[2], data[3], data[4], data[5]);
			break;
		case PathCmdType::QuadraticTo:
			writer.write("Q%g %g,%g %g", data[0], data[1], data[2], data[3]);
			break;
		case PathCmdType::ArcTo:
			writer.write("A%g %g %g %d %d %g %g", data[0], data[1], data[2], (int)data[3], (int)data[4], data[5], data[6]);
			break;
		case PathCmdType::ClosePath:
			writer.out() << 'Z';
			break;
		default:
			SSVG_WARN(false, "Unknown path command");
			break;
		}
	}
	writer.out() << "\" ";

	return true;
}

bool writeShapeList(StreamWriter& writer, const ShapeList* shapeList, const ShapeAttributes* parentAttrs, uint32_t indentation)
{
	const uint32_t numShapes = shapeList->m_NumShapes;
	for (uint32_t iShape = 0; iShape < numShapes; ++iShape) {
		const Shape* shape = &shapeList->m_Shapes[iShape];

		const ShapeType::Enum shapeType = shape->m_Type;
		switch (shapeType) {
		case ShapeType::Group:
			writer.indent(indentation);
			writer.out() << "<g ";
			if (!writeShapeAttributes(writer, shape->m_Attrs, parentAttrs, SaveAttr::All)) {
				return false;
			}
			writer.out() << ">\n";

			if (!writeShapeList(writer, &shape->m_ShapeList, shape->m_Attrs, indentation + 2)) {
				return false;
			}

			writer.indent(indentation);
			writer.out() << "</g>\n";
			break;
		case ShapeType::Rect:
			writer.indent(indentation);
			writer.out() << "<rect ";
			if (!writeShapeAttributes(writer, shape->m_Attrs, parentAttrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			writer.write("x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" "
				, shape->m_Rect.x
				, shape->m_Rect.y
				, shape->m_Rect.width
				, shape->m_Rect.height);

			if (shape->m_Rect.rx != 0.0f) {
				writer.write("rx=\"%g\" ", shape->m_Rect.rx);
			}
			if (shape->m_Rect.ry != 0.0f) {
				writer.write("ry=\"%g\" ", shape->m_Rect.ry);
			}
			writer.out() << "/>\n";
			break;
		case ShapeType::Circle:
			writer.indent(indentation);
			writer.out() << "<circle ";
			if (!writeShapeAttributes(writer, shape->m_Attrs, parentAttrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			writer.write("cx=\"%g\" cy=\"%g\" r=\"%g\" />\n"
				, shape->m_Circle.cx
				, shape->m_Circle.cy
				, shape->m_Circle.r);
			break;
		case ShapeType::Ellipse:
			writer.indent(indentation);
			writer.out() << "<ellipse ";
			if (!writeShapeAttributes(writer, shape->m_Attrs, parentAttrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			writer.write("cx=\"%g\" cy=\"%g\" rx=\"%g\" ry=\"%g\" />\n"
				, shape->m_Ellipse.cx
				, shape->m_Ellipse.cy
				, shape->m_Ellipse.rx
				, shape->m_Ellipse.ry);
			break;
		case ShapeType::Line:
			writer.indent(indentation);
			writer.out() << "<line ";
			if (!writeShapeAttributes(writer, shape->m_Attrs, parentAttrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			writer.write("x1=\"%g\" y1=\"%g\" x2=\"%g\" y2=\"%g\" />\n"
				, shape->m_Line.x1
				, shape->m_Line.y1
				, shape->m_Line.x2
				, shape->m_Line.y2);
			break;
		case ShapeType::Polyline:
			writer.indent(indentation);
			writer.out() << "<polyline ";
			if (!writeShapeAttributes(writer, shape->m_Attrs, parentAttrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			if (!writePointList(writer, &shape->m_PointList)) {
				return false;
			}
			writer.out() << "/>\n";
			break;
		case ShapeType::Polygon:
			writer.indent(indentation);
			writer.out() << "<polygon ";
			if (!writeShapeAttributes(writer, shape->m_Attrs, parentAttrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			if (!writePointList(writer, &shape->m_PointList)) {
				return false;
			}
			writer.out() << "/>\n";
			break;
		case ShapeType::Path:
			writer.indent(indentation);
			writer.out() << "<path ";
			if (!writeShapeAttributes(writer, shape->m_Attrs, parentAttrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			if (!writePath(writer, &shape->m_Path)) {
				return false;
			}
			writer.out() << "/>\n";
			break;
		case ShapeType::Text:
			writer.indent(indentation);
			writer.out() << "<text ";
			if (!writeShapeAttributes(writer, shape->m_Attrs, parentAttrs, SaveAttr::Text)) {
				return false;
			}
			writer.write("x=\"%g\" y=\"%g\" ", shape->m_Text.x, shape->m_Text.y);
			writer.out() << "text-anchor=\"" << textAnchorToString(shape->m_Text.m_Anchor) << "\">" << shape->m_Text.m_String << "</text>\n";
			break;
		default:
			SSVG_WARN(false, "Unknown shape type");
		}
	}

	return true;
}

} // namespace

bool imageSave(const Image* img, std::ostream& out)
{
	assert(img);
	constexpr uint32_t SSVG_FORMAT_BUFFER_LEN = 256;

	StreamWriter writer(out, SSVG_FORMAT_BUFFER_LEN);

	// Open <svg> element
	writer.out() << "<svg ";
	if (img->m_Width != 0.0f) {
		writer.write("width=\"%g\" ", img->m_Width);
	}
	if (img->m_Height != 0.0f) {
		writer.write("height=\"%g\" ", img->m_Height);
	}
	if (img->m_VerMajor != 0) {
		writer.write("version=\"%u.%u\" ", img->m_VerMajor, img->m_VerMinor);
	}
	if (img->m_BaseProfile != BaseProfile::None) {
		writer.out() << "baseProfile=\"" << baseProfileToString(img->m_BaseProfile) << "\" ";
	}
	if (img->m_ViewBox[2] > 0.0f && img->m_ViewBox[3] > 0.0f) {
		writer.write("viewBox=\"%g %g %g %g\" ", img->m_ViewBox[0], img->m_ViewBox[1], img->m_ViewBox[2], img->m_ViewBox[3]);
	}
	writer.out() << "xmlns=\"http://www.w3.org/2000/svg\">\n";

	// Write shapes
	if (!writeShapeList(writer, &img->m_ShapeList, &img->m_BaseAttrs, 1)) {
		return false;
	}

	// Close the <svg> element
	writer.out() << "</svg>\n";

	return true;
}

} // namespace ssvg
