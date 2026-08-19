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

// Default parser font size:
//  - The SVG standard says "medium" is the default font size.
//  - The CSS standard defines "medium" as whatever the default of the renderer is.
//  - The de facto standard of most renderers is medium = 12pt = 16px.
#ifndef SSVG_CONFIG_PARSER_DEFAULT_FONT_SIZE_IN_PX
#	define SSVG_CONFIG_PARSER_DEFAULT_FONT_SIZE_IN_PX 16
#endif

#ifndef SSVG_CONFIG_DEFAULT_DPI
#	define SSVG_CONFIG_DEFAULT_DPI 96
#endif

// When parsing an SVG image, in the absence of a viewbox, and if the SVG container
// width and height are dependent on the outside container (e.g. "100%"), use the
// following completely arbitrary viewport size as the default to convert all
// dependent lengths to pixels.
#ifndef SSVG_CONFIG_PARSER_DEFAULT_VIEWPORT_WIDTH_IN_PX
#	define SSVG_CONFIG_PARSER_DEFAULT_VIEWPORT_WIDTH_IN_PX 800
#endif
#ifndef SSVG_CONFIG_PARSER_DEFAULT_VIEWPORT_HEIGHT_IN_PX
#	define SSVG_CONFIG_PARSER_DEFAULT_VIEWPORT_HEIGHT_IN_PX 600
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

struct LengthUnit
{
	enum Enum : uint32_t
	{
		User = 0,   // (No specifier) User Unit
		PX,         // "px" Pixels
		Percent,    // "%"  A percentage
		EM,         // "em" Relative to font size
		EX,         // "ex" Relative to font x-height
		IN,         // "in" Inches
		CM,         // "cm" Centimeters
		MM,         // "mm" Millimeters
		PT,         // "pt" Points
		PC,         // "pc" Picas
	};
};

struct Length
{
	float            m_Length;
	LengthUnit::Enum m_Unit;
};

struct ShapeType
{
	enum Enum : uint32_t
	{
		Group = 0,   // The default Shape is an empty group
		Rect,
		Circle,
		Ellipse,
		Line,
		Polyline,
		Polygon,
		Path,
		Text,
		NumShapeTypes
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

struct PathNum
{
	struct SubpathNum {
		uint32_t numSubpaths;
		uint32_t numNodes;
	};

	SubpathNum closed;
	SubpathNum open;
};

struct ShapesCounters {
	struct PolyNum {
		uint32_t numPoly;
		uint32_t numPoints;
	};

	uint32_t numGroups;
	uint32_t numRects;
	uint32_t numCircles;
	uint32_t numEllipses;
	uint32_t numLines;
	uint32_t numTexts;
	PolyNum polygons;
	PolyNum polylines;
	PathNum paths;
};

struct OwnedString {
	char* m_Buf;
	uint32_t m_Len;

	bool empty() const noexcept {
		return m_Buf == nullptr;
	}

	uint32_t length() const noexcept {
		return m_Buf ? m_Len : 0;
	}

	const char* c_str() const noexcept {
		// If empty() returns true, the pointer points to a single null character.
		return m_Buf ? m_Buf : "";
	}
};

// TODO: alignment-baseline
struct Text
{
	OwnedString m_String;
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
		StrokePaintChanged      = 1 << 0,
		StrokeMiterLimitChanged = 1 << 1,
		StrokeOpacityChanged    = 1 << 2,
		StrokeWidthChanged      = 1 << 3,
		StrokeLineJoinChanged   = 1 << 4,
		StrokeLineCapChanged    = 1 << 5,
		FillPaintChanged        = 1 << 6,
		FillOpacityChanged      = 1 << 7,
		FillRuleChanged         = 1 << 8,
		FontSizeChanged         = 1 << 9,
		FontFamilyChanged       = 1 << 10,
		Opacity                 = 1 << 11,
		Transformation          = 1 << 12,
		ElementID               = 1 << 13,
		ElementClass            = 1 << 14,
		All                     =(1 << 15) - 1
	};
};

inline constexpr uint32_t TRANSFORM_ARRAY_SZ = 6;

struct ShapeAttributes
{
	AttribFlags::Type m_Flags;
	Paint m_StrokePaint;
	Paint m_FillPaint;
	float m_Transform[TRANSFORM_ARRAY_SZ];
	float m_StrokeMiterLimit;
	float m_StrokeOpacity;
	Length m_StrokeWidth;
	float m_FillOpacity;
	float m_Opacity;
	LineJoin::Enum m_StrokeLineJoin;
	LineCap::Enum m_StrokeLineCap;
	FillRule::Enum m_FillRule;
	Length m_FontSize;
	char m_FontFamily[SSVG_CONFIG_FONT_FAMILY_MAX_LEN];
	char m_ID[SSVG_CONFIG_ID_MAX_LEN];
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

struct Group
{
	ShapeList m_ShapeList;
	OwnedString m_Title;
};

struct Shape
{
	ShapeType::Enum m_Type;
	ShapeAttributes* m_Attrs;
	float m_BoundingRect[BOUNDING_RECT_ARRAY_SZ]; // NOTE: Transformation independent axis-aligned bounding rect {minx, miny, maxx, maxy}

	union
	{
		Group m_Group;
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
	Group m_RootContainer;
	ShapeAttributes m_BaseAttrs;
	Length m_Width;
	Length m_Height;
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

// Library setup
void initLib();
void shutdownLib();

// Default ShapeAttributes according to the SVG standards
const ShapeAttributes& defaultShapeAttributes();

// Image allocated by imageLoad or imageCreate *must* be freed with imageFree
Image* imageLoad(const char* xmlStr, ImageLoadFlags::Type flags, const ShapeAttributes* baseAttrs = nullptr);
Image* imageCreate(const ShapeAttributes* baseAttrs = nullptr);
void imageFree(Image* img);

// ShapeList allocated by shapeListCreate *must* be freed with shapeListFree
ShapeList* shapeListCreate();
void shapeListFree(ShapeList* shapeList);

// Manipulate Images
bool imageSave(const Image* img, std::ostream& out);
ShapeAttributes*       imageGetShapeAttributes(      Image* img);
const ShapeAttributes* imageGetShapeAttributes(const Image* img);
Group*                 imageGetRootGroup(      Image* img);
const Group*           imageGetRootGroup(const Image* img);
ShapeList*             imageGetRootShapeList(      Image* img);
const ShapeList*       imageGetRootShapeList(const Image* img);
const OwnedString& imageGetTile(const Image* img);
void               imageSetTitle(Image* img, const char* str);
uint32_t imageGetNumShapes(const Image* img);

// Manipulate Groups
ShapeList*             groupGetShapeList(      Group* group);
const ShapeList*       groupGetShapeList(const Group* group);
const OwnedString&     groupGetTitle(const Group* group);
void                   groupSetTitle(Group* group, const char* str);
void groupClear(Group* group);

// Manipulate ShapeLists
Shape* shapeListAllocShape(ShapeList* shapeList, ShapeType::Enum type);
void shapeListReserve(ShapeList* shapeList, uint32_t capacity);
void shapeListShrinkToFit(ShapeList* shapeList);
void shapeListClear(ShapeList* shapeList);
uint32_t shapeListAddShape(ShapeList* shapeList, const Shape* shape);
uint32_t shapeListAddGroup(ShapeList* shapeList, const ShapeList* sourceShapeList = nullptr);
uint32_t shapeListAddGroupWithShapes(ShapeList* shapeList, const Shape* children, uint32_t numChildren);
uint32_t shapeListAddRect(ShapeList* shapeList, float x, float y, float w, float h, float rx, float ry);
uint32_t shapeListAddCircle(ShapeList* shapeList, float x, float y, float r);
uint32_t shapeListAddEllipse(ShapeList* shapeList, float x, float y, float rx, float ry);
uint32_t shapeListAddLine(ShapeList* shapeList, float x1, float y1, float x2, float y2);
uint32_t shapeListAddPolyline(ShapeList* shapeList, const float* coords, uint32_t numPoints);
uint32_t shapeListAddPolygon(ShapeList* shapeList, const float* coords, uint32_t numPoints);
uint32_t shapeListAddPath(ShapeList* shapeList, const Path* sourcePath = nullptr);
uint32_t shapeListAddPathCommands(ShapeList* shapeList, const PathCmd* pathCommands, uint32_t commands);
uint32_t shapeListAddText(ShapeList* shapeList, float x, float y, TextAnchor::Enum anchor, const char* str);
uint32_t shapeListMoveShapeToBack(ShapeList* shapeList, uint32_t shapeIndex);
uint32_t shapeListMoveShapeToFront(ShapeList* shapeList, uint32_t shapeIndex);
ShapeAttributes*       shapeListAllocShapeAttributes(    ShapeList* shapeList, uint32_t shapeIndex, const ShapeAttributes* parentAttrs = nullptr);
ShapeAttributes*       shapeListGetShapeAttributes(      ShapeList* shapeList, uint32_t shapeIndex);
const ShapeAttributes* shapeListGetShapeAttributes(const ShapeList* shapeList, uint32_t shapeIndex);
ShapeType::Enum shapeListGetShapeType(const ShapeList* shapeList, uint32_t shapeIndex);
Shape*     shapeListGetShape(ShapeList* shapeList, uint32_t shapeIndex);
Group*     shapeListGetGroup(ShapeList* shapeList, uint32_t shapeIndex);                        // Only for ShapeType::Group
ShapeList* shapeListGetGroupShapeList(ShapeList* shapeList, uint32_t shapeIndex);               // Only for ShapeType::Group
PointList* shapeListGetPointList(ShapeList* shapeList, uint32_t shapeIndex);                    // Only for ShapeType::Polyline and ShapeType::Polygon
Path*      shapeListGetPath(ShapeList* shapeList, uint32_t shapeIndex);                         // Only for ShapeType::Path
const Shape*     shapeListGetShape(const ShapeList* shapeList, uint32_t shapeIndex);
const Group*     shapeListGetGroup(const ShapeList* shapeList, uint32_t shapeIndex);            // Only for ShapeType::Group
const ShapeList* shapeListGetGroupShapeList(const ShapeList* shapeList, uint32_t shapeIndex);   // Only for ShapeType::Group
const PointList* shapeListGetPointList(const ShapeList* shapeList, uint32_t shapeIndex);        // Only for ShapeType::Polyline and ShapeType::Polygon
const Path*      shapeListGetPath(const ShapeList* shapeList, uint32_t shapeIndex);             // Only for ShapeType::Path
const Rect*      shapeListGetRect(const ShapeList* shapeList, uint32_t shapeIndex);             // Only for ShapeType::Rect
const Circle*    shapeListGetCircle(const ShapeList* shapeList, uint32_t shapeIndex);           // Only for ShapeType::Circle
const Ellipse*   shapeListGetEllipse(const ShapeList* shapeList, uint32_t shapeIndex);          // Only for ShapeType::Ellipse
const Line*      shapeListGetLine(const ShapeList* shapeList, uint32_t shapeIndex);             // Only for ShapeType::Line
const Text*      shapeListGetText(const ShapeList* shapeList, uint32_t shapeIndex);             // Only for ShapeType::Text
void shapeListDeleteShape(ShapeList* shapeList, uint32_t shapeIndex);
void shapeListDeleteLastShape(ShapeList* shapeList);
void shapeListCalcBounds(ShapeList* shapeList, float* bounds);
uint32_t shapeListGetNumShapes(const ShapeList* shapeList);
ShapesCounters shapeListEnumerate(const ShapeList* shapeList);

// Manipulate Paths
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
void pathConvertCommand(Path* path, uint32_t cmdIndex, PathCmdType::Enum newType);
void pathClearCommand(Path* path, uint32_t cmdIndex);
PathNum pathGetSubpathCounters(const Path* path);

// Manipulate PointLists (used by ShapeType::Polyline and ShapeType::Polygon)
float* pointListAllocPoints(PointList* ptList, uint32_t n);
void pointListShrinkToFit(PointList* ptList);
void pointListClear(PointList* ptList);
bool pointListFromString(PointList* ptList, const std::string_view& str);
void pointListCalcBounds(const PointList* ptList, float* bounds);
uint32_t pointListGetNumPoints(const PointList* ptList);

// Manipulate Text
void textSetString(Text* text, const char* str);
void textClear(Text* text);

// Manipulate ShapeAttributes
std::string_view shapeAttrsGetID(const ShapeAttributes* attrs);
std::string_view shapeAttrsGetFontFamily(const ShapeAttributes* attrs);
std::string_view shapeAttrsGetClass(const ShapeAttributes* attrs);
void shapeAttrsSetID(ShapeAttributes* attrs, const std::string_view& value);
void shapeAttrsSetFontFamily(ShapeAttributes* attrs, const std::string_view& value);
void shapeAttrsSetClass(ShapeAttributes* attrs, const std::string_view& value);

// A transformation is an array float[TRANSFORM_ARRAY_SZ]
void transformIdentity(float* transform);
void transformTranslation(float* transform, float x, float y);
void transformMultiply(float* trasnform_a, const float* transform_b);
void transformTranslate(float* transform, float x, float y);
void transformPoint(const float* transform, const float* localPos, float* globalPos);
void transformBoundingRect(const float* transform, const float* localRect, float* globalRect);

// Manipulate Shapes
void shapeClear(Shape* shape);
bool shapeIsEmptyGroup(Shape* shape);       // A cleared shape is an empty ShapeType::Group

void shapeGetTransform(const Shape* shape, float* out_transform);
void shapeSetTransform(Shape* shape, const float* transform);
void shapeSetIdentityTransform(Shape* shape);
void shapeApplyTransform(Shape* shape, const float* transform);
ShapeType::Enum shapeGetType(const Shape* shape);
ShapeAttributes*       shapeAllocAttributes(Shape* shape, const ShapeAttributes* parentAttrs = nullptr);
ShapeAttributes*       shapeGetAttributes(Shape* shape);
const ShapeAttributes* shapeGetAttributes(const Shape* shape);
bool shapeCopy(Shape* dst, const Shape* src, bool copyAttrs = true);
void shapeUpdateBounds(Shape* shape);

namespace internals {

struct AllocatedShapeAttrsCounters {
	uint32_t numNodes;
	uint32_t numAllocAttrs;
	uint32_t numFreeAttrs;
};

AllocatedShapeAttrsCounters enumerateAllocatedShapeAttrs();

} // internals

} // namespace ssvg

#endif      // SSVG_SSVG_H
