#ifndef SSVG_SSVG_H
#define SSVG_SSVG_H

#include <cstdint>
#include <ostream>
#include <string_view>

#ifndef SSVG_CONFIG_ID_MAX_LEN
#	define SSVG_CONFIG_ID_MAX_LEN 16
#endif

#ifndef SSVG_CONFIG_FONT_FAMILY_MAX_LEN
#	define SSVG_CONFIG_FONT_FAMILY_MAX_LEN 16
#endif

#ifndef SSVG_CONFIG_CLASS_MAX_LEN
#	define SSVG_CONFIG_CLASS_MAX_LEN 0
#endif

#ifndef SSVG_CONFIG_OUTPUT_SVG_INDENT
#	define SSVG_CONFIG_OUTPUT_SVG_INDENT 2
#endif

namespace ssvg {

struct BaseProfile
{
	enum Enum : uint32_t
	{
		None = 0,
		Full,
		Basic,
		Tiny
	};
};

struct ShapeType
{
	enum Enum : uint32_t
	{
		Group = 0,
		Rect,
		Circle,
		Ellipse,
		Line,
		Polyline,
		Polygon,
		Path,
		Text,
		NbOfShapeTypes
	};
};

struct PathCmdType
{
	enum Enum : uint32_t
	{
		Nop = 0,     // No operation
		MoveTo,      // Data: [0] = x, [1] = y
		LineTo,      // Data: [0] = x, [1] = y
		CubicTo,     // Data: [0] = x1, [1] = y1, [2] = x2, [3] = y2, [4] = x, [5] = y
		QuadraticTo, // Data: [0] = x1, [1] = y1, [2] = x, [3] = y
		ArcTo,       // Data: [0] = rx, [1] = ry, [2] = x-axis-rotation, [3] = large-arc-flag, [4] = sweep-flag, [5] = x, [6] = y
		ClosePath,   // No data
	};
};

struct TextAnchor
{
	enum Enum : uint32_t
	{
		Start,
		Middle,
		End
	};
};

struct PaintType
{
	enum Enum : uint32_t
	{
		None = 0,
		Transparent, // Does it make sense?
		Color
	};
};

struct LineJoin
{
	enum Enum : uint32_t
	{
		Miter,
		Round,
		Bevel
	};
};

struct LineCap
{
	enum Enum : uint32_t
	{
		Butt,
		Round,
		Square
	};
};

struct FillRule
{
	enum Enum : uint32_t
	{
		NonZero,
		EvenOdd
	};
};

struct Rect
{
	float x;
	float y;
	float width;
	float height;
	float rx;
	float ry;
};

struct Circle
{
	float cx;
	float cy;
	float r;
};

struct Ellipse
{
	float cx;
	float cy;
	float rx;
	float ry;
};

struct Line
{
	float x1;
	float y1;
	float x2;
	float y2;
};

struct PointList
{
	float* m_Coords;
	uint32_t m_NumPoints;
	uint32_t m_Capacity;
};

inline constexpr uint32_t PATH_DATA_ARRAY_SZ = 7;

struct PathCmd
{
	PathCmdType::Enum m_Type;
	float m_Data[PATH_DATA_ARRAY_SZ]; // NOTE: The amount and meaning of each value depends on m_Type (see PathCmdType::Enum)
};

struct Path
{
	PathCmd* m_Commands;
	uint32_t m_NumCommands;
	uint32_t m_Capacity;
};

// TODO: alignment-baseline
struct Text
{
	char* m_String;
	float x;
	float y;
	TextAnchor::Enum m_Anchor;
};

// TODO: Gradients
// TODO: Images (???)
struct Paint
{
	PaintType::Enum m_Type;

	union
	{
		uint32_t m_ColorABGR;
	};
};

struct AttribFlags
{
	using Type = uint32_t;
	enum Enum : Type
	{
		None                    = 0,
		StrokePaintInherit      = 1 << 0,
		StrokeMiterLimitInherit = 1 << 1,
		StrokeOpacityInherit    = 1 << 2,
		StrokeWidthInherit      = 1 << 3,
		StrokeLineJoinInherit   = 1 << 4,
		StrokeLineCapInherit    = 1 << 5,
		FillPaintInherit        = 1 << 6,
		FillOpacityInherit      = 1 << 7,
		FillRuleInherit         = 1 << 8,
		FontSizeInherit         = 1 << 9,
		FontFamilyInherit       = 1 << 10,
		InheritAll              =(1 << 11) - 1
	};
};

inline constexpr uint32_t TRANSFORM_ARRAY_SZ = 6;

struct ShapeAttributes
{
	const ShapeAttributes* m_Parent;
	Paint m_StrokePaint;
	Paint m_FillPaint;
	float m_Transform[TRANSFORM_ARRAY_SZ];
	float m_StrokeMiterLimit;
	float m_StrokeOpacity;
	float m_StrokeWidth;
	float m_FillOpacity;
	float m_FontSize;
	float m_Opacity;
	AttribFlags::Type m_Flags;
	LineJoin::Enum m_StrokeLineJoin;
	LineCap::Enum m_StrokeLineCap;
	FillRule::Enum m_FillRule;
	char m_ID[SSVG_CONFIG_ID_MAX_LEN];
	char m_FontFamily[SSVG_CONFIG_FONT_FAMILY_MAX_LEN];
#if SSVG_CONFIG_CLASS_MAX_LEN
	char m_Class[SSVG_CONFIG_CLASS_MAX_LEN];
#endif
};

inline constexpr uint32_t BOUNDING_RECT_ARRAY_SZ = 4;
inline constexpr uint32_t VIEW_BOX_ARRAY_SZ = 4;

struct Shape;

struct ShapeList
{
	Shape* m_Shapes;
	uint32_t m_NumShapes;
	uint32_t m_Capacity;
};

struct Shape
{
	ShapeType::Enum m_Type;
	ShapeAttributes* m_Attrs;
	float m_BoundingRect[BOUNDING_RECT_ARRAY_SZ]; // NOTE: Transformation independent axis-aligned bounding rect {minx, miny, maxx, maxy}

	union
	{
		ShapeList m_ShapeList; // NOTE: Used for ShapeType::Group
		PointList m_PointList; // NOTE: Used for ShapeType::Polyline and ShapeType::Polygon
		Rect m_Rect;
		Circle m_Circle;
		Ellipse m_Ellipse;
		Line m_Line;
		Path m_Path;
		Text m_Text;
	};
};

struct Image
{
	ShapeList m_ShapeList;
	ShapeAttributes m_BaseAttrs;
	float m_Width;
	float m_Height;
	float m_ViewBox[VIEW_BOX_ARRAY_SZ];
	float m_BoundingRect[BOUNDING_RECT_ARRAY_SZ];
	BaseProfile::Enum m_BaseProfile;
	uint16_t m_VerMajor;
	uint16_t m_VerMinor;
};

struct ImageLoadFlags
{
	using Type = uint32_t;
	enum Enum : Type
	{
		None                   	    = 0,
		ConvertPolygonsToPaths      = 1 << 0,
		ConvertPolylinesToPaths     = 1 << 1,
		ConvertQuadToCubicBezier    = 1 << 2,
		ConvertArcToCubicBezier     = 1 << 3,
		CalcShapeBounds             = 1 << 4,
		CalcPathConvexity           = 1 << 5,
	};
};

void initLib();
void shutdownLib();

ShapeAttributes defaultShapeAttributes();

// Image allocated by imageLoad and imageCreate *must* be freed with imageFree
Image* imageLoad(const char* xmlStr, ImageLoadFlags::Type flags, const ShapeAttributes* baseAttrs = nullptr);
Image* imageCreate(const ShapeAttributes* baseAttrs = nullptr);
void imageFree(Image* img);
bool imageSave(const Image* img, std::ostream& out);

// ShapeList allocated by shapeListCreate *must* be freed with shapeListFree
ShapeList* shapeListCreate();
void shapeListFree(ShapeList* shapeList);

Shape* shapeListAllocShape(ShapeList* shapeList, ShapeType::Enum type, const ShapeAttributes* parentAttrs);
void shapeListReserve(ShapeList* shapeList, uint32_t capacity);
void shapeListShrinkToFit(ShapeList* shapeList);
void shapeListClear(ShapeList* shapeList);
uint32_t shapeListAddShape(ShapeList* shapeList, const Shape* shape);
uint32_t shapeListAddGroup(ShapeList* shapeList, const ShapeAttributes* parentAttrs, const ShapeList* sourceShapeList = nullptr);
uint32_t shapeListAddGroupWithShapes(ShapeList* shapeList, const ShapeAttributes* parentAttrs, const Shape* children, uint32_t numChildren);
uint32_t shapeListAddRect(ShapeList* shapeList, const ShapeAttributes* parentAttrs, float x, float y, float w, float h, float rx, float ry);
uint32_t shapeListAddCircle(ShapeList* shapeList, const ShapeAttributes* parentAttrs, float x, float y, float r);
uint32_t shapeListAddEllipse(ShapeList* shapeList, const ShapeAttributes* parentAttrs, float x, float y, float rx, float ry);
uint32_t shapeListAddLine(ShapeList* shapeList, const ShapeAttributes* parentAttrs, float x1, float y1, float x2, float y2);
uint32_t shapeListAddPolyline(ShapeList* shapeList, const ShapeAttributes* parentAttrs, const float* coords, uint32_t numPoints);
uint32_t shapeListAddPolygon(ShapeList* shapeList, const ShapeAttributes* parentAttrs, const float* coords, uint32_t numPoints);
uint32_t shapeListAddPath(ShapeList* shapeList, const ShapeAttributes* parentAttrs, const Path* sourcePath = nullptr);
uint32_t shapeListAddPathCommands(ShapeList* shapeList, const ShapeAttributes* parentAttrs, const PathCmd* pathCommands, uint32_t commands);
uint32_t shapeListAddText(ShapeList* shapeList, const ShapeAttributes* parentAttrs, float x, float y, TextAnchor::Enum anchor, const char* text);
uint32_t shapeListMoveShapeToBack(ShapeList* shapeList, uint32_t shapeID);
uint32_t shapeListMoveShapeToFront(ShapeList* shapeList, uint32_t shapeID);
void shapeListDeleteShape(ShapeList* shapeList, uint32_t shapeID);
void shapeListCalcBounds(ShapeList* shapeList, float* bounds);

PathCmd* pathAllocCommand(Path* path, PathCmdType::Enum type);
PathCmd* pathAllocCommands(Path* path, uint32_t n);
PathCmd* pathInsertCommands(Path* path, uint32_t at, uint32_t n);
void pathReserveCommands(Path* path, uint32_t capacity);
void pathShrinkToFit(Path* path);
void pathClear(Path* path);
bool pathFromString(Path* path, const std::string_view& str, ImageLoadFlags::Type flags);
uint32_t pathMoveTo(Path* path, float x, float y);
uint32_t pathLineTo(Path* path, float x, float y);
uint32_t pathCubicTo(Path* path, float x1, float y1, float x2, float y2, float x, float y);
uint32_t pathQuadraticTo(Path* path, float x1, float y1, float x, float y);
uint32_t pathArcTo(Path* path, float rx, float ry, float xAxisRotation, int largeArcFlag, int sweepFlag, float x, float y);
uint32_t pathClose(Path* path);
void pathCalcBounds(const Path* path, float* bounds);
void pathConvertCommand(Path* path, uint32_t cmdID, PathCmdType::Enum newType);

float* pointListAllocPoints(PointList* ptList, uint32_t n);
void pointListShrinkToFit(PointList* ptList);
void pointListClear(PointList* ptList);
bool pointListFromString(PointList* ptList, const std::string_view& str);
void pointListCalcBounds(const PointList* ptList, float* bounds);

void textClear(Text* text);

std::string_view shapeAttrsGetID(const ShapeAttributes* attrs);
std::string_view shapeAttrsGetFontFamily(const ShapeAttributes* attrs);
std::string_view shapeAttrsGetClass(const ShapeAttributes* attrs);
void shapeAttrsSetID(ShapeAttributes* attrs, const std::string_view& value);
void shapeAttrsSetFontFamily(ShapeAttributes* attrs, const std::string_view& value);
void shapeAttrsSetClass(ShapeAttributes* attrs, const std::string_view& value);

void shapeClear(Shape* shape);
bool shapeCopy(Shape* dst, const Shape* src, bool copyAttrs = true);
void shapeUpdateBounds(Shape* shape);

// A transformation is an array float[TRANSFORM_ARRAY_SZ]
void transformIdentity(float* transform);
void transformTranslation(float* transform, float x, float y);
void transformMultiply(float* trasnform_a, const float* transform_b);
void transformTranslate(float* transform, float x, float y);
void transformPoint(const float* transform, const float* localPos, float* globalPos);
void transformBoundingRect(const float* transform, const float* localRect, float* globalRect);

void shapeSetTransform(Shape* shape, const float* transform);
void shapeSetIdentityTransform(Shape* shape);
void shapeApplyTransform(Shape* shape, const float* transform);

} // namespace ssvg

#endif		// SSVG_SSVG_H
