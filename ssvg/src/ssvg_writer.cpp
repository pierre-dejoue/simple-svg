#include <ssvg/ssvg.h>

#include "ssvg_debug.h"
#include "ssvg_math.h"
#include "ssvg_private.h"

#include <stdutils/macros.h>
#include <stdutils/memory.h>
#include <stdutils/minmax.h>

#include <cstdio>
#include <cstring>
#include <ostream>

namespace ssvg {

namespace {

struct SaveAttr
{
	using Type = uint32_t;
	enum Enum : Type
	{
		None        = 0,
		ID          = 1 << 0,
		Transform   = 1 << 1,
		Stroke      = 1 << 2,
		Fill        = 1 << 3,
		Color       = 1 << 4,
		Font        = 1 << 5,
		Class       = 1 << 6,
		Opacity     = 1 << 7,
		All         =(1 << 8) - 1,

		// Special flag
		ConditionalPaints = 0x80000000, // If set, and PaintType == None || Transparent don't save stroke-width, stroke-opacity, etc.

		// Common combinations
		Unique = Transform | ID,
		Shape = Unique  | Stroke | Fill | Color,
		Text = Unique | Stroke | Fill | Color | Font | ConditionalPaints,
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

class StreamWriter
{
public:
	StreamWriter(std::ostream& out, uint32_t strLen, uint32_t baseIndentation = 0)
		: m_Out{out}
		, m_Str{nullptr}
		, m_StrLen{strLen}
		, m_BaseIndent{stdutils::clamp<uint32_t>(baseIndentation, 0, 32)}
	{
		assert(strLen > 1);
		m_Str = (char*)std::malloc(strLen);
		IGNORE_RETURN stdutils::memset<char>(m_Str, strLen, 0, strLen);
	}

	~StreamWriter()
	{
		std::free(m_Str);
		m_Str = nullptr;
	}

	std::ostream& out() const noexcept { return m_Out; }

	template <typename... Args>
	bool write(const char* format, Args&&... args);

	void indent(uint32_t lvl);

	uint32_t baseIndentation() const noexcept { return m_BaseIndent; }

private:
	std::ostream& m_Out;
	char*         m_Str;
	uint32_t      m_StrLen;
	uint32_t      m_BaseIndent;
};

template <typename... Args>
bool StreamWriter::write(const char* format, Args&&... args)
{
	assert(m_Str);
	const int written = std::snprintf(m_Str, m_StrLen, format, std::forward<Args>(args)...);
	const bool success = 0 <= written && written < m_StrLen;
	if (success) { m_Out << m_Str; }
	return success;
}

void StreamWriter::indent(uint32_t lvl)
{
	const uint32_t numSpaces = lvl * m_BaseIndent;
	for (uint32_t n = 0; n < numSpaces; n++) {
		m_Out.put(' ');
	}
}

void colorToHex(StreamWriter& writer, uint32_t abgr)
{
	const uint32_t r = (abgr      ) & 0x000000FF;
	const uint32_t g = (abgr >>  8) & 0x000000FF;
	const uint32_t b = (abgr >> 16) & 0x000000FF;
	IGNORE_RETURN writer.write("#%02X%02X%02X", r, g, b);
}

void writePaintColorValue(StreamWriter& writer, const Paint& paint)
{
	switch (paint.m_Type) {
		case PaintType::None:
			writer.out() << "none";
			break;
		case PaintType::Transparent:
			writer.out() << " transparent";
			break;
		case PaintType::CurrentColor:
			writer.out() << "currentColor";
			break;
		case PaintType::Color:
			colorToHex(writer, paint.m_ColorABGR);
			break;
		default:
			SSVG_CHECK(false, "Unknown paint type (%d)", paint.m_Type);
			break;
	}
}

bool writeShapeAttributes(StreamWriter& writer, const ShapeAttributes* attrs, SaveAttr::Type flags = SaveAttr::All)
{
	if (!attrs) {
		return true;
	}

	const bool conditionalPaints = (flags & SaveAttr::ConditionalPaints) != 0;

	if ((flags & SaveAttr::ID) != 0 && attrs->m_ID[0] != '\0') {
		writer.out() << " id=\"" << attrs->m_ID << "\"";
	}

#if SSVG_CONFIG_CLASS_MAX_LEN
	if ((flags & SaveAttr::Class) != 0 && attrs->m_Class[0] != '\0') {
		writer.out() << " class=\"" << attrs->m_Class << "\"";
	}
#endif

	if ((flags & SaveAttr::Transform) != 0 && !transformIsIdentity(&attrs->m_Transform[0])) {
		writer.write(" transform=\"matrix(%g,%g,%g,%g,%g,%g)\""
			, attrs->m_Transform[0]
			, attrs->m_Transform[1]
			, attrs->m_Transform[2]
			, attrs->m_Transform[3]
			, attrs->m_Transform[4]
			, attrs->m_Transform[5]);
	}

	if (flags & SaveAttr::Stroke) {
		if (attrs->m_Flags & AttribFlags::StrokePaintChanged) {
			writer.out() << " stroke=\"";
			writePaintColorValue(writer, attrs->m_StrokePaint);
			writer.out() << "\"";
		}

		const PaintType::Enum strokeType = attrs->m_StrokePaint.m_Type;
		const bool saveExtra = !conditionalPaints || (strokeType != PaintType::None && strokeType != PaintType::Transparent);
		if (saveExtra) {
			if (attrs->m_Flags & AttribFlags::StrokeMiterLimitChanged) {
				const float miterLimit = stdutils::clamp(attrs->m_StrokeMiterLimit, 1.0f, math::kFloatMax);
				writer.write(" stroke-miterlimit=\"%g\"", miterLimit);
			}

			if (attrs->m_Flags & AttribFlags::StrokeWidthChanged) {
				const float width = stdutils::clamp(attrs->m_StrokeWidth.m_Length, 0.f, math::kFloatMax);
			    const char* widthUnit = width > 0.f ? lengthUnitToString(attrs->m_StrokeWidth.m_Unit).data() : "";
				writer.write(" stroke-width=\"%g%s\"", width, widthUnit);
			}

			if (attrs->m_Flags & AttribFlags::StrokeOpacityChanged) {
				const float opacity = stdutils::clamp(attrs->m_StrokeOpacity, 0.f, 1.f);
				writer.write(" stroke-opacity=\"%g\"", opacity);
			}

			if (attrs->m_Flags & AttribFlags::StrokeLineJoinChanged) {
				const LineJoin::Enum lineJoin = attrs->m_StrokeLineJoin;
				writer.out() << " stroke-linejoin=\"" << lineJoinToString(lineJoin) << "\"";
			}

			if (attrs->m_Flags & AttribFlags::StrokeLineCapChanged) {
				const LineCap::Enum lineCap = attrs->m_StrokeLineCap;
				writer.out() << " stroke-linecap=\"" << lineCapToString(lineCap) << "\"";
			}
		}
	}

	if (flags & SaveAttr::Fill) {
		if (attrs->m_Flags & AttribFlags::FillPaintChanged) {
			writer.out() << " fill=\"";
			writePaintColorValue(writer, attrs->m_FillPaint);
			writer.out() << "\"";
		}

		const PaintType::Enum fillType = attrs->m_FillPaint.m_Type;
		const bool saveExtra = !conditionalPaints || (fillType != PaintType::None && fillType != PaintType::Transparent);
		if (saveExtra) {
			const float opacity = stdutils::clamp(attrs->m_FillOpacity, 0.f, 1.f);
			if (attrs->m_Flags & AttribFlags::FillOpacityChanged) {
				writer.write(" fill-opacity=\"%g\"", opacity);
			}

			if (attrs->m_Flags & AttribFlags::FillRuleChanged) {
				writer.out() << " fill-rule=\"" << fillRuleToString(attrs->m_FillRule) << "\"";
			}
		}
	}

	if (flags & SaveAttr::Color) {
		if (attrs->m_Flags & AttribFlags::ColorPaintChanged) {
			writer.out() << " color=\"";
			writePaintColorValue(writer, attrs->m_ColorPaint);
			writer.out() << "\"";
		}
	}

	if ((flags & SaveAttr::Font) != 0) {
		const char* fontFamily = attrs->m_FontFamily;
		if (fontFamily[0] != '\0') {
			writer.out() << " font-family=\"" << fontFamily << "\"";
		}

		const float& fontSize = attrs->m_FontSize.m_Length;
		const char* fontSizeUnit = lengthUnitToString(attrs->m_FontSize.m_Unit).data();
		if (attrs->m_Flags & AttribFlags::FontSizeChanged) {
			writer.write(" font-size=\"%g%s\"", fontSize, fontSizeUnit);
		}
	}

	if (flags & SaveAttr::Opacity) {
		if (attrs->m_Flags & AttribFlags::Opacity) {
			writer.write(" opacity=\"%g\"", attrs->m_Opacity);
		}
	}

	return true;
}

bool writePointList(StreamWriter& writer, const PointList* pointList)
{
	writer.out() << " points=\"";
	const uint32_t numPoints = pointList->m_NumPoints;
	for (uint32_t i = 0; i < numPoints; ++i) {
		const float* coords = &pointList->m_Coords[i * 2];
		writer.write("%g,%g ", coords[0], coords[1]);
	}
	writer.out() << "\"";

	return true;
}

bool writePath(StreamWriter& writer, const Path* path)
{
	writer.out() << " d=\"";
	// TODO: Extra minification can be achieved by using relative commands
	// (because adjacent commands/coords are usually close to the last position).
	const uint32_t numCommands = path->m_NumCommands;
	for (uint32_t iCmd = 0; iCmd < numCommands; ++iCmd) {
		if (iCmd > 0) { writer.out().put(' '); }
		const PathCmd* cmd = &path->m_Commands[iCmd];
		const PathCmdType::Enum type = cmd->m_Type;
		const float* data = &cmd->m_Data[0];
		switch (type) {
		case PathCmdType::Nop:
			// Nothing to write
			break;
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
	writer.out() << "\"";

	return true;
}

void writeTitle(StreamWriter& writer, const OwnedString& title, uint32_t indentationLevel)
{
	if (title.empty()) {
		return;
	}
	writer.indent(indentationLevel);
	writer.out() << "<title>" << title.c_str() << "</title>\n";
}

bool writeShapeList(StreamWriter& writer, const ShapeList* shapeList, const ShapeAttributes* parentAttrs, uint32_t treeDepth = 0)
{
	const uint32_t numShapes = shapeList->m_NumShapes;
	const uint32_t indentationLevel = treeDepth + 1;
	for (uint32_t iShape = 0; iShape < numShapes; ++iShape) {
		const Shape* shape = &shapeList->m_Shapes[iShape];

		const ShapeType::Enum shapeType = shape->m_Type;
		switch (shapeType) {
		case ShapeType::Group:
			{
				writer.indent(indentationLevel);
				writer.out() << "<g";
				if (!writeShapeAttributes(writer, shape->m_Attrs)) {
					return false;
				}
				writer.out() << ">\n";

				writeTitle(writer, shape->m_Group.m_Title, indentationLevel + 1);

				if (!writeShapeList(writer, &shape->m_Group.m_ShapeList, shape->m_Attrs, treeDepth + 1)) {
					return false;
				}

				writer.indent(indentationLevel);
				writer.out() << "</g>\n";
			}
			break;
		case ShapeType::Rect:
			writer.indent(indentationLevel);
			writer.out() << "<rect";
			if (!writeShapeAttributes(writer, shape->m_Attrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			writer.write(" x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\""
				, shape->m_Rect.x
				, shape->m_Rect.y
				, shape->m_Rect.width
				, shape->m_Rect.height);

			if (shape->m_Rect.rx != 0.0f) {
				writer.write(" rx=\"%g\"", shape->m_Rect.rx);
			}
			if (shape->m_Rect.ry != 0.0f) {
				writer.write(" ry=\"%g\"", shape->m_Rect.ry);
			}
			writer.out() << " />\n";
			break;
		case ShapeType::Circle:
			writer.indent(indentationLevel);
			writer.out() << "<circle";
			if (!writeShapeAttributes(writer, shape->m_Attrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			writer.write(" cx=\"%g\" cy=\"%g\" r=\"%g\" />\n"
				, shape->m_Circle.cx
				, shape->m_Circle.cy
				, shape->m_Circle.r);
			break;
		case ShapeType::Ellipse:
			writer.indent(indentationLevel);
			writer.out() << "<ellipse";
			if (!writeShapeAttributes(writer, shape->m_Attrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			writer.write(" cx=\"%g\" cy=\"%g\" rx=\"%g\" ry=\"%g\" />\n"
				, shape->m_Ellipse.cx
				, shape->m_Ellipse.cy
				, shape->m_Ellipse.rx
				, shape->m_Ellipse.ry);
			break;
		case ShapeType::Line:
			writer.indent(indentationLevel);
			writer.out() << "<line";
			if (!writeShapeAttributes(writer, shape->m_Attrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			writer.write(" x1=\"%g\" y1=\"%g\" x2=\"%g\" y2=\"%g\" />\n"
				, shape->m_Line.x1
				, shape->m_Line.y1
				, shape->m_Line.x2
				, shape->m_Line.y2);
			break;
		case ShapeType::Polyline:
			writer.indent(indentationLevel);
			writer.out() << "<polyline";
			if (!writeShapeAttributes(writer, shape->m_Attrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			if (!writePointList(writer, &shape->m_PointList)) {
				return false;
			}
			writer.out() << " />\n";
			break;
		case ShapeType::Polygon:
			writer.indent(indentationLevel);
			writer.out() << "<polygon";
			if (!writeShapeAttributes(writer, shape->m_Attrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			if (!writePointList(writer, &shape->m_PointList)) {
				return false;
			}
			writer.out() << " />\n";
			break;
		case ShapeType::Path:
			writer.indent(indentationLevel);
			writer.out() << "<path";
			if (!writeShapeAttributes(writer, shape->m_Attrs, SaveAttr::Shape | SaveAttr::ConditionalPaints)) {
				return false;
			}
			if (!writePath(writer, &shape->m_Path)) {
				return false;
			}
			writer.out() << " />\n";
			break;
		case ShapeType::Text:
			writer.indent(indentationLevel);
			writer.out() << "<text";
			if (!writeShapeAttributes(writer, shape->m_Attrs, SaveAttr::Text)) {
				return false;
			}
			writer.write(" x=\"%g\" y=\"%g\"", shape->m_Text.x, shape->m_Text.y);
			writer.out() << " text-anchor=\"" << textAnchorToString(shape->m_Text.m_Anchor) << "\">" << shape->m_Text.m_String.c_str() << "</text>\n";
			break;
		default:
			SSVG_WARN(false, "Unknown shape type");
		}
	}

	return true;
}

} // namespace

const ImageWriterOptions& defaultImageWriterOptions()
{
	static const ImageWriterOptions defaultOptions = []() {
		ImageWriterOptions options;
		stdutils::memset<ImageWriterOptions>(&options, 0);
		options.m_Indentation = SSVG_CONFIG_WRITER_DEFAULT_INDENTATION;
		return options;
	}();
	return defaultOptions;
}

bool imageSave(const Image* img, std::ostream& out, const ImageWriterOptions* options)
{
	assert(img);
	if (!img) { return false; }

	// Use the default writer options if none is provided as argument
	options = options ? options : &defaultImageWriterOptions();

	constexpr uint32_t SSVG_FORMAT_BUFFER_LEN = 256;
	StreamWriter writer(out, SSVG_FORMAT_BUFFER_LEN, options->m_Indentation);

	// Open the <svg> element
	writer.out() << "<svg";
	if (img->m_Width.m_Length != 0.0f) {
		const float width = img->m_Width.m_Length;
		const char* widthUnit = lengthUnitToString(img->m_Width.m_Unit).data();
		writer.write(" width=\"%g%s\"", width, widthUnit);
	}
	if (img->m_Height.m_Length != 0.0f) {
		const float height = img->m_Height.m_Length;
		const char* heightUnit = lengthUnitToString(img->m_Height.m_Unit).data();
		writer.write(" height=\"%g%s\"", height, heightUnit);
	}
	if (img->m_VerMajor != 0) {
		writer.write(" version=\"%u.%u\"", img->m_VerMajor, img->m_VerMinor);
	}
	if (img->m_BaseProfile != BaseProfile::None) {
		writer.out() << " baseProfile=\"" << baseProfileToString(img->m_BaseProfile) << "\"";
	}
	const float viewBoxWidth  = img->m_ViewBox[2];
	const float viewBoxHeight = img->m_ViewBox[3];
	if (viewBoxWidth > 0.0f && viewBoxHeight > 0.0f) {
		writer.write(" viewBox=\"%g %g %g %g\"", img->m_ViewBox[0], img->m_ViewBox[1], img->m_ViewBox[2], img->m_ViewBox[3]);
	}
	writer.out() << " xmlns=\"http://www.w3.org/2000/svg\">\n";

	// Write image title
	constexpr uint32_t indendationLevel = 1;
	writeTitle(writer, img->m_RootContainer.m_Title, indendationLevel);

	// Write shapes
	if (!writeShapeList(writer, &img->m_RootContainer.m_ShapeList, &img->m_BaseAttrs)) {
		return false;
	}

	// Close the <svg> element
	writer.out() << "</svg>\n";

	return true;
}

} // namespace ssvg
