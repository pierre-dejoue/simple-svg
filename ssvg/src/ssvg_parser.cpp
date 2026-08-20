#include <ssvg/ssvg.h>

#include "ssvg_debug.h"
#include "ssvg_math.h"
#include "ssvg_private.h"

#include <stdutils/macros.h>
#include <stdutils/memory.h>
#include <stdutils/minmax.h>
#include <stdutils/string.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string_view>
#include <tuple>

namespace ssvg {

namespace {

const char* strend(std::string_view str)
{
	return str.data() + str.length();
}

int strlenint(std::string_view str)
{
	return static_cast<int>(str.length());
}

struct ParseAttr
{
	enum Result : uint32_t
	{
		OK = 0,
		Fail = 1,
		Unknown = 2
	};
};

struct ParserState
{
	const char* m_XMLString;
	const char* m_Ptr;
	ImageLoadFlags::Type m_Flags;
	ShapeAttributes m_ParsedShapeAttrs;
	LengthContext* m_LengthContext;
	bool m_ExpectClosingTag;             // Temporarily set when the parser is on the '>' character of normal tag (i.e. not a self-closing tag)
};

struct CSSColor
{
	std::string_view m_Name;
	uint32_t m_ABGR;
};

const CSSColor kCSSColors[] = {
	{ "black",            0xFF000000 }, { "silver",               0xFFC0C0C0 }, { "gray",              0xFF808080 }, { "white",           0xFFFFFFFF },
	{ "maroon",           0xFF000080 }, { "red",                  0xFF0000FF }, { "purple",            0xFF800080 }, { "fuchsia",         0xFFFF00FF },
	{ "green",            0xFF008000 }, { "lime",                 0xFF00FF00 }, { "olive",             0xFF008080 }, { "yellow",          0xFF00FFFF },
	{ "navy",             0xFF800000 }, { "blue",                 0xFFFF0000 }, { "teal",              0xFF808000 }, { "aqua",            0xFFFFFF00 },
	{ "orange",           0xFF00A5FF }, { "aliceblue",            0xFFFFF8F0 }, { "antiquewhite",      0xFFD7EBFA }, { "aquamarine",      0xFFD4FF7F },
	{ "azure",            0xFFFFFFF0 }, { "beige",                0xFFDCF5F5 }, { "bisque",            0xFFC4E4FF }, { "blanchedalmond",  0xFFCDEBFF },
	{ "blueviolet",       0xFFE22B8A }, { "brown",                0xFF2A2AA5 }, { "burlywood",         0xFF87B8DE }, { "cadetblue",       0xFFA09E5F },
	{ "chartreuse",       0xFF00FF7F }, { "chocolate",            0xFF1E69D2 }, { "coral",             0xFF507FFF }, { "cornflowerblue",  0xFFED9564 },
	{ "cornsilk",         0xFFDCF8FF }, { "crimson",              0xFF3C14DC }, { "cyan",              0xFFFFFF00 }, { "darkblue",        0xFF8B0000 },
	{ "darkcyan",         0xFF8B8B00 }, { "darkgoldenrod",        0xFF0B86B8 }, { "darkgray",          0xFFA9A9A9 }, { "darkgreen",       0xFF006400 },
	{ "darkgrey",         0xFFA9A9A9 }, { "darkkhaki",            0xFF6BB7BD }, { "darkmagenta",       0xFF8B008B }, { "darkolivegreen",  0xFF2F6B55 },
	{ "darkorange",       0xFF008CFF }, { "darkorchid",           0xFFCC3299 }, { "darkred",           0xFF00008B }, { "darksalmon",      0xFF7A96E9 },
	{ "darkseagreen",     0xFF8FBC8F }, { "darkslateblue",        0xFF8B3D48 }, { "darkslategray",     0xFF4F4F2F }, { "darkslategrey",   0xFF4F4F2F },
	{ "darkturquoise",    0xFFD1CE00 }, { "darkviolet",           0xFFD30094 }, { "deeppink",          0xFF9314FF }, { "deepskyblue",     0xFFFFBF00 },
	{ "dimgray",          0xFF696969 }, { "dimgrey",              0xFF696969 }, { "dodgerblue",        0xFFFF901E }, { "firebrick",       0xFF2222B2 },
	{ "floralwhite",      0xFFF0FAFF }, { "forestgreen",          0xFF228B22 }, { "gainsboro",         0xFFDCDCDC }, { "ghostwhite",      0xFFFFF8F8 },
	{ "gold",             0xFF00D7FF }, { "goldenrod",            0xFF20A5DA }, { "greenyellow",       0xFF2FFFAD }, { "grey",            0xFF808080 },
	{ "honeydew",         0xFFF0FFF0 }, { "hotpink",              0xFFB469FF }, { "indianred",         0xFF5C5CCD }, { "indigo",          0xFF82004B },
	{ "ivory",            0xFFF0FFFF }, { "khaki",                0xFF8CE6F0 }, { "lavender",          0xFFF1E6E6 }, { "lavenderblush",   0xFFF5F0FF },
	{ "lawngreen",        0xFF00FC7C }, { "lemonchiffon",         0xFFCDFAFF }, { "lightblue",         0xFFE6D8AD }, { "lightcoral",      0xFF8080F0 },
	{ "lightcyan",        0xFFFFFFE0 }, { "lightgoldenrodyellow", 0xFFD2FAFA }, { "lightgray",         0xFFD3D3D3 }, { "lightgreen",      0xFF90EE90 },
	{ "lightgrey",        0xFFD3D3D3 }, { "lightpink",            0xFFC1B6FF }, { "lightsalmon",       0xFF7AA0FF }, { "lightseagreen",   0xFFAAB220 },
	{ "lightskyblue",     0xFFFACE87 }, { "lightslategray",       0xFF778899 }, { "lightslategrey",    0xFF778899 }, { "lightsteelblue",  0xFFDEC4B0 },
	{ "lightyellow",      0xFFE0FFFF }, { "limegreen",            0xFF32CD32 }, { "linen",             0xFFE6F0FA }, { "magenta",         0xFFFF00FF },
	{ "mediumaquamarine", 0xFFAACD66 }, { "mediumblue",           0xFFCD0000 }, { "mediumorchid",      0xFFD355BA }, { "mediumpurple",    0xFFDB7093 },
	{ "mediumseagreen",   0xFF71B33C }, { "mediumslateblue",      0xFFEE687B }, { "mediumspringgreen", 0xFF9AFA00 }, { "mediumturquoise", 0xFFCCD148 },
	{ "mediumvioletred",  0xFF8515C7 }, { "midnightblue",         0xFF701919 }, { "mintcream",         0xFFFAFFF5 }, { "mistyrose",       0xFFE1E4FF },
	{ "moccasin",         0xFFB5E4FF }, { "navajowhite",          0xFFADDEFF }, { "oldlace",           0xFFE6F5FD }, { "olivedrab",       0xFF238E6B },
	{ "orangered",        0xFF0045FF }, { "orchid",               0xFFD670DA }, { "palegoldenrod",     0xFFAAE8EE }, { "palegreen",       0xFF98FB98 },
	{ "paleturquoise",    0xFFEEEEAF }, { "palevioletred",        0xFF9370DB }, { "papayawhip",        0xFFF5EFFF }, { "peachpuff",       0xFFB9DAFF },
	{ "peru",             0xFF3F85CD }, { "pink",                 0xFFCBC0FF }, { "plum",              0xFFDDA0DD }, { "powderblue",      0xFFE6E0B0 },
	{ "rosybrown",        0xFF8F8FBC }, { "royalblue",            0xFFE16941 }, { "saddlebrown",       0xFF13458B }, { "salmon",          0xFF7280FA },
	{ "sandybrown",       0xFF60A4F4 }, { "seagreen",             0xFF578B2E }, { "seashell",          0xFFEEF5FF }, { "sienna",          0xFF2D52A0 },
	{ "skyblue",          0xFFEBCE87 }, { "slateblue",            0xFFCDA56A }, { "slategray",         0xFF908070 }, { "slategrey",       0xFF908070 },
	{ "snow",             0xFFFAFAFF }, { "springgreen",          0xFF7FFF00 }, { "steelblue",         0xFFB48246 }, { "tan",             0xFF8CB4D2 },
	{ "thistle",          0xFFD8BFD8 }, { "tomato",               0xFF4763FF }, { "turquoise",         0xFFD0E040 }, { "violet",          0xFFEE82EE },
	{ "wheat",            0xFFB3DEF5 }, { "whitesmoke",           0xFFF5F5F5 }, { "yellowgreen",       0xFF32CD9A }, { "rebeccapurple",   0xFF993366 },
};

constexpr uint32_t kNumCSSColors = sizeof(kCSSColors) / sizeof(CSSColor);

bool parseSVGElements(ParserState* parser, Group* group, const ShapeAttributes& parentAttrs, std::string_view closingTag);
const char* parseCoord(const char* str, const char* end, float* coord);
ParseAttr::Result parseGenericShapeAttribute(const std::string_view& name, const std::string_view& value, ShapeAttributes* attrs);

inline uint8_t charToNibble(char ch)
{
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	} else if (ch >= 'a' && ch <= 'f') {
		return 10 + (ch - 'a');
	} else if (ch >= 'A' && ch <= 'F') {
		return 10 + (ch - 'A');
	}

	SSVG_WARN(false, "Invalid hex char %c", ch);

	return 0;
}

inline const char* skipWhitespace(const char* ptr, const char* end)
{
	assert(ptr);
	while (ptr != end && stdutils::ascii::isspace(*ptr) && *ptr != '\0') {
		++ptr;
	}

	return ptr;
}

inline const char* skipCommaWhitespace(const char* ptr, const char* end)
{
	assert(ptr);
	// comma-wsp: (wsp+ comma? wsp*) | (comma wsp*)
	ptr = skipWhitespace(ptr, end);
	if (*ptr == ',') {
		ptr = skipWhitespace(ptr + 1, end);
	}

	return ptr;
}

inline bool parserIsDone(ParserState* parser) noexcept
{
	assert(parser);
	return (parser && parser->m_Ptr) ? (*parser->m_Ptr == '\0') : true;
}

inline void parserSkipWhitespace(ParserState* parser)
{
	assert(parser);
	parser->m_Ptr = skipWhitespace(parser->m_Ptr, nullptr);
}

bool parserExpectingChar(ParserState* parser, char ch)
{
	assert(parser);
	if (*parser->m_Ptr == ch) {
		++parser->m_Ptr;
		return true;
	}

	return false;
}

bool parserMatchString(ParserState* parser, std::string_view str)
{
	assert(parser);
	return !std::strncmp(parser->m_Ptr, str.data(), str.size());
}

inline bool parserExpectingString(ParserState* parser, std::string_view str)
{
	assert(parser);
	if (parserMatchString(parser, str)) {
		parser->m_Ptr += str.size();
		return true;
	}

	return false;
}

//
// Advance the stream passed the end of the tag passed as argument.
// It is assumed that the starting pointer is passed the beginning tag
// (after "<tag") but before the closing bracket '>'
//
// Illustrative example:
//
//   <tag id="1"><g><tag id="2">Hello, World!</tag></g></tag><rect x="0"....
//     start ---^                                            ^--- after parserSkipTag
//
bool parserSkipTag(ParserState* parser, std::string_view tag)
{
	assert(parser);
	parser->m_ExpectClosingTag = false;
	uint32_t numOpenTags = 1;                    // We are already passed the 1st opening tag "<tag"
	bool stateLookForEndOfOpeningTag = true;     // Binary state machine (states explained below)
	while (!parserIsDone(parser) && numOpenTags > 0) {
		char ch = *parser->m_Ptr++;     // Advance by at least one character each loop

		if (stateLookForEndOfOpeningTag) {
			//
			// In this state, we are looking for the end of the opening tag "<tag":
			//  - Either a "/>" string, meaning self-closing, e.g. "<tag id="3" />"
			//  - Or a simple ">" string, for a normal opening tag, e.g. "<tag>"
			//
			if (ch == '/' && *parser->m_Ptr == '>') {
				parser->m_Ptr++;
				assert(numOpenTags > 0);
				numOpenTags--;
				stateLookForEndOfOpeningTag = false;
			} else if (ch == '>') {
				stateLookForEndOfOpeningTag = false;
			} // Else, keep advancing....
		} else {
			assert(!stateLookForEndOfOpeningTag);
			//
			// In this state, we are looking for either a closing tag "</tag>", or another opening tag "<tag"
			//
			if (ch == '<') {
				const bool isAClosingTag = (*parser->m_Ptr == '/');
				if (isAClosingTag) { parser->m_Ptr++; }
				if (parserExpectingString(parser, tag)) {
					char nextCh = *parser->m_Ptr;
					if (isAClosingTag) {
						if (nextCh == '>') {
							parser->m_Ptr++;
							assert(numOpenTags > 0);
							numOpenTags--;
						}
					} else { // Opening tag
						if (nextCh == '>' || nextCh == '/' || stdutils::ascii::isspace(nextCh)) {
							numOpenTags++;
							stateLookForEndOfOpeningTag = true;
						}
					}
				}
			} // Else, keep advancing....
		}
	}

	return (numOpenTags == 0);
}

void parserSkipEndOfComment(ParserState* parser)
{
	assert(parser);
	while (!parserIsDone(parser)) {
		if (std::string_view(parser->m_Ptr, 3) == "-->") {
			parser->m_Ptr += 3;
			break;
		}
		parser->m_Ptr++;
	}
}

void parserSkipWhitespaceAndComments(ParserState* parser)
{
	assert(parser);
	const char* prevPtr = nullptr;
	while (!parserIsDone(parser) && parser->m_Ptr > prevPtr) {
		prevPtr = parser->m_Ptr;
		parserSkipWhitespace(parser);

		// Not all comment tags are followed by a space, so specifically detect those
		if (std::string_view(parser->m_Ptr, 4) != "<!--") {
			break;
		}
		parser->m_Ptr += 4;
		parserSkipEndOfComment(parser);
	}
}

std::string_view parserGetTag(ParserState* parser)
{
	assert(parser);

	parserSkipWhitespaceAndComments(parser);
	if (!parserExpectingChar(parser, '<')) {
		return std::string_view();
	}
	parserSkipWhitespace(parser); // Is it valid to have a whitespace after the < for a tag?

	const char* tagBegin = parser->m_Ptr;
	++parser->m_Ptr;

	// Search for the next whitespace or closing angle bracket
	while (!parserIsDone(parser) && !stdutils::ascii::isspace(*parser->m_Ptr) && *parser->m_Ptr != '>') {
		++parser->m_Ptr;
	}

	if (parserIsDone(parser)) {
		return std::string_view();
	}

	const char* tagEnd = parser->m_Ptr;
	assert(tagBegin <= tagEnd);
	std::string_view tag(tagBegin, tagEnd - tagBegin);

	SSVG_WARN(!tag.empty(), "Empty tag");
	SSVG_WARN(*tagBegin != '/', "Unexpected closing tag \"<%s>\"", std::string(tag).c_str());
	if (*tagBegin == '/') {
		return std::string_view();
	}

	return tag;
}

bool parserGetAttribute(ParserState* parser, std::string_view* name, std::string_view* value)
{
	assert(parser);
	parserSkipWhitespace(parser);

	if (!stdutils::ascii::isalpha(*parser->m_Ptr)) {
		return false;
	}

	const char* namePtr = parser->m_Ptr;

	// Skip the identifier
	while (stdutils::ascii::isalnum(*parser->m_Ptr)
		|| *parser->m_Ptr == '-'
		|| *parser->m_Ptr == '_'
		|| *parser->m_Ptr == ':') {
		++parser->m_Ptr;
	}

	assert(namePtr <= parser->m_Ptr);
	*name = std::string_view(namePtr, parser->m_Ptr - namePtr);

	// Check of the equal sign
	parserSkipWhitespace(parser);
	if (!parserExpectingChar(parser, '=')) {
		return false;
	}

	// Check of opening quote
	parserSkipWhitespace(parser);
	if (!parserExpectingChar(parser, '\"')) {
		return false;
	}

	const char* valuePtr = parser->m_Ptr;

	// Find the closing quote
	while (*parser->m_Ptr != '\"') {
		// Check for invalid strings (i.e. tag closes before closing quote or we reached the end of the buffer)
		char ch = *parser->m_Ptr;
		if (ch == '>' || ch == '\0') {
			return false;
		}

		++parser->m_Ptr;
	}

	assert(valuePtr <= parser->m_Ptr);
	*value = std::string_view(valuePtr, parser->m_Ptr - valuePtr);

	++parser->m_Ptr;

	return true;
}

bool parseVersion(const std::string_view& verStr, uint16_t* maj, uint16_t* min)
{
	assert(maj);
	assert(min);

	const float fver = static_cast<float>(atof(verStr.data()));
	*maj = static_cast<uint16_t>(std::floor(fver));
	const float fmaj = static_cast<float>(*maj);
	*min = static_cast<uint16_t>(std::floor((fver - fmaj) * 10.0f));

	return true;
}

inline std::istream& parseNumber(std::istream& in, float& val, bool& success, float min = -math::kFloatMax, float max = math::kFloatMax)
{
	success = false;
	in >> val;
	if (!std::isfinite(val)) {
		val = 0.f;
		return in;
	}
	val = stdutils::clamp<float>(val, min, max);
	success = std::isfinite(val);

	return in;
}

bool parseNumber(const std::string_view& str, float& val, float min = -math::kFloatMax, float max = math::kFloatMax)
{
	std::stringstream in;
	in << str;
	bool success = false;
	parseNumber(in, val, success, min, max);

	return success;
}

LengthUnit::Enum lengthUnitFind(std::string_view length)
{
	const std::string lowerLength = stdutils::string::tolower(length);
	if (stdutils::string::ends_with(lowerLength, "px")) {
		return LengthUnit::PX;
	} else if (stdutils::string::ends_with(lowerLength, '%')) {
		return LengthUnit::Percent;
	} else if (stdutils::string::ends_with(lowerLength, "em")) {
		return LengthUnit::EM;
	} else if (stdutils::string::ends_with(lowerLength, "ex")) {
		return LengthUnit::EX;
	} else if (stdutils::string::ends_with(lowerLength, "in")) {
		return LengthUnit::IN;
	} else if (stdutils::string::ends_with(lowerLength, "cm")) {
		return LengthUnit::CM;
	} else if (stdutils::string::ends_with(lowerLength, "mm")) {
		return LengthUnit::MM;
	} else if (stdutils::string::ends_with(lowerLength, "pt")) {
		return LengthUnit::PT;
	} else if (stdutils::string::ends_with(lowerLength, "pc")) {
		return LengthUnit::PC;
	}
	// The default
	return LengthUnit::User;
}

bool parseLength(const std::string_view& str, Length& length)
{
	stdutils::memset<Length>(&length, 0);

	// Parse the unit first
	length.m_Unit = lengthUnitFind(str);

	// Strip the unit from the end of the input string
	const auto unitLen = lengthUnitToString(length.m_Unit).size();
	const std::string_view value = str.substr(0, str.length() - unitLen);

	// Parse the numerical value
	const bool success = parseNumber(value, length.m_Length);
	SSVG_WARN(success, "Failed to parse length \"%s\" (value: \"%s\")", std::string(str).c_str(), std::string(value).c_str());

	return success;
}

bool parsePositiveLength(const std::string_view& str, Length& length)
{
	constexpr float MIN_LENGTH = 0.f;
	constexpr float MAX_LENGTH = math::kFloatMax;
	return stdutils::clamp<float>(parseLength(str, length), MIN_LENGTH, MAX_LENGTH);
}

bool parsePaint(const std::string_view& str, Paint* paint)
{
	// TODO: Handle more cases.
	if (str == "none") {
		paint->m_Type = PaintType::None;
	} else if (str == "transparent") {
		paint->m_Type = PaintType::Transparent;
	} else {
		paint->m_Type = PaintType::Color;

		const char* ptr = str.data();
		paint->m_ColorABGR = 0xFF000000;
		if (*ptr == '#') {
			// Hex color
			if (str.length() == 7) {
				const uint8_t r = (charToNibble(ptr[1]) << 4) | charToNibble(ptr[2]);
				const uint8_t g = (charToNibble(ptr[3]) << 4) | charToNibble(ptr[4]);
				const uint8_t b = (charToNibble(ptr[5]) << 4) | charToNibble(ptr[6]);
				paint->m_ColorABGR |= ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
			} else if (str.length() == 4) {
				const uint8_t r = charToNibble(ptr[1]);
				const uint8_t g = charToNibble(ptr[2]);
				const uint8_t b = charToNibble(ptr[3]);
				const uint32_t rgb = ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
				paint->m_ColorABGR |= rgb | (rgb << 4);
			} else {
				SSVG_WARN(false, "Unknown hex color format %.*s", strlenint(str), str.data());
			}
		} else if (str.substr(0, 4) == "rgb(") {
			// TODO: This doesn't work with percentages.
			float color[3];
			const char* end = strend(str);
			ptr += 4;
			ptr = parseCoord(ptr, end, &color[0]);
			ptr = parseCoord(ptr, end, &color[1]);
			ptr = parseCoord(ptr, end, &color[2]);

			paint->m_ColorABGR |= ((uint32_t)color[0]) | ((uint32_t)color[1] << 8) | ((uint32_t)color[2] << 16);
		} else if (str.substr(0, 5) ==  "rgba(") {
			// TODO: This doesn't work with percentages.
			float color[4];
			const char* end = strend(str);
			ptr += 5;
			ptr = parseCoord(ptr, end, &color[0]);
			ptr = parseCoord(ptr, end, &color[1]);
			ptr = parseCoord(ptr, end, &color[2]);
			ptr = parseCoord(ptr, end, &color[3]);

			paint->m_ColorABGR = ((uint32_t)color[0]) | ((uint32_t)color[1] << 8) | ((uint32_t)color[2] << 16) | ((uint32_t)(color[3] * 255.0f) << 24);
		} else {
			// Check if it's a known color.
			bool found = false;
			for (uint32_t i = 0; i < kNumCSSColors; ++i) {
				if (str == kCSSColors[i].m_Name) {
					paint->m_ColorABGR = kCSSColors[i].m_ABGR;
					found = true;
					break;
				}
			}

			if (!found) {
				SSVG_WARN(false, "Unhandled paint value: %.*s", strlenint(str), str.data());
			}
		}
	}

	return true;
}

const char* parseCoord(const char* str, const char* end, float* coord)
{
	const char* ptr = skipCommaWhitespace(str, end);

	SSVG_CHECK(ptr != end && !stdutils::ascii::isalpha(*ptr), "Parse error");

	char* coordEnd;
	*coord = strtof(ptr, &coordEnd);

	SSVG_CHECK(coordEnd != nullptr, "Failed to parse coordinate");
	SSVG_CHECK(coordEnd <= end, "strtof() read past end of buffer");

	return skipCommaWhitespace(coordEnd, end);
}

const char* parseFlag(const char* str, const char* end, float* flag)
{
	const char* ptr = skipCommaWhitespace(str, end);

	SSVG_CHECK(ptr != end && !stdutils::ascii::isalpha(*ptr), "Parse error");

	if (*ptr == '0') {
		*flag = 0.0f;
	} else {
		*flag = 1.0f;
	}

	return skipCommaWhitespace(ptr + 1, end);
}

bool parseViewBox(const std::string_view& str, float* viewBox)
{
	const char* ptr = str.data();
	const char* end = strend(str);

	ptr = parseCoord(ptr, end, &viewBox[0]);
	ptr = parseCoord(ptr, end, &viewBox[1]);
	ptr = parseCoord(ptr, end, &viewBox[2]);
	ptr = parseCoord(ptr, end, &viewBox[3]);

	return true;
}

// Scans str and extracts type and value. Expected format is:
//    type '(' value ')'
// where type is an identifier and value is any kind of text
const char* parseTransformComponent(const char* str, const char* end, std::string_view* type, std::string_view* value)
{
	SSVG_CHECK(stdutils::ascii::isalpha(*str), "Parse error: Excepted identifier");

	const char* ptr = str;
	while (ptr != end && stdutils::ascii::isalpha(*ptr)) {
		++ptr;
	}

	if (ptr == end) {
		SSVG_CHECK(false, "Parse error: Transformation component ended early");
		return nullptr;
	}

	assert(str <= ptr);
	*type = std::string_view(str, ptr - str);

	ptr = skipWhitespace(ptr, end);

	if (*ptr != '(') {
		SSVG_CHECK(false, "Parse error: Expected '('");
		return nullptr;
	}

	ptr = skipWhitespace(ptr + 1, end);

	const char* valuePtr = ptr;

	// Skip until closing parenthesis
	while (ptr != end && *ptr != ')') {
		++ptr;
	}

	if (ptr == end) {
		SSVG_CHECK(false, "Parse error: Couldn't find closing parenthesis");
		return nullptr;
	}
	SSVG_CHECK(*ptr == ')', "Parse error: Expected ')'");

	const char* endPtr = ptr + 1;

	// Walk back ptr to get a tight value string (without trailing whitespaces)
	while (ptr != valuePtr && stdutils::ascii::isspace(*ptr)) {
		--ptr;
	}

	if (ptr < valuePtr) {
		SSVG_CHECK(false, "Parse error: Walk back too far");
		return nullptr;
	}
	*value = std::string_view(valuePtr, ptr - valuePtr);

	return endPtr;
}

bool parseTransform(const std::string_view& str, float* transform)
{
	const char* ptr = str.data();
	const char* end = strend(str);

	transformIdentity(transform);
	ptr = skipWhitespace(ptr, end);
	while (ptr != end) {
		std::string_view type, value;

		ptr = parseTransformComponent(ptr, end, &type, &value);
		if (!ptr) {
			SSVG_CHECK(false, "Parse error");
			return false;
		}

		ptr = skipCommaWhitespace(ptr, end);

		// Parse the transform value.
		const char* valuePtr = value.data();
		const char* valueEnd = strend(value);

		float comp[6];
		transformIdentity(&comp[0]);
		if (type == "matrix") {
			valuePtr = parseCoord(valuePtr, valueEnd, &comp[0]);
			valuePtr = parseCoord(valuePtr, valueEnd, &comp[1]);
			valuePtr = parseCoord(valuePtr, valueEnd, &comp[2]);
			valuePtr = parseCoord(valuePtr, valueEnd, &comp[3]);
			valuePtr = parseCoord(valuePtr, valueEnd, &comp[4]);
			valuePtr = parseCoord(valuePtr, valueEnd, &comp[5]);
		} else if (type == "translate") {
			valuePtr = parseCoord(valuePtr, valueEnd, &comp[4]);
			if (valuePtr != valueEnd) {
				valuePtr = parseCoord(valuePtr, valueEnd, &comp[5]);
			}
		} else if (type == "scale") {
			valuePtr = parseCoord(valuePtr, valueEnd, &comp[0]);
			if (valuePtr != valueEnd) {
				valuePtr = parseCoord(valuePtr, valueEnd, &comp[3]);
			} else {
				comp[3] = comp[0];
			}
		} else if (type == "rotate") {
			float angle_deg;
			valuePtr = parseCoord(valuePtr, valueEnd, &angle_deg);

			const float angle_rad = math::to_rad(angle_deg);
			const float cosAngle = std::cos(angle_rad);
			const float sinAngle = std::sin(angle_rad);
			comp[0] = cosAngle;
			comp[1] = sinAngle;
			comp[2] = -sinAngle;
			comp[3] = cosAngle;

			if (valuePtr != valueEnd) {
				float cX, cY;
				valuePtr = parseCoord(valuePtr, valueEnd, &cX);
				valuePtr = parseCoord(valuePtr, valueEnd, &cY);

				// translate(cX, cY) rotate() translate(-cX, -cY)
				comp[4] = cX * (1.0f - cosAngle) + cY * sinAngle;
				comp[5] = cY * (1.0f - cosAngle) - cX * sinAngle;
			}
		} else if (type == "skewX") {
			float angle_deg;
			valuePtr = parseCoord(valuePtr, valueEnd, &angle_deg);

			const float angle_rad = math::to_rad(angle_deg);
			comp[2] = std::tan(angle_rad);
		} else if (type == "skewY") {
			float angle_deg;
			valuePtr = parseCoord(valuePtr, valueEnd, &angle_deg);

			const float angle_rad = math::to_rad(angle_deg);
			comp[1] = std::tan(angle_rad);
		} else {
			SSVG_WARN(false, "Unknown transform component %.*s(%.*s)", strlenint(type), type.data(), strlenint(value), value.data());
			valuePtr = valueEnd;
		}

		SSVG_WARN(valuePtr == valueEnd, "Incomplete transformation parsing");

		transformMultiply(transform, comp);
	}

	return true;
}

} // namespace

bool pathFromString(Path* path, const std::string_view& str, ImageLoadFlags::Type flags)
{
	const char* ptr = str.data();
	const char* end = strend(str);
	float firstX = 0.0f;
	float firstY = 0.0f;
	float lastX = 0.0f;
	float lastY = 0.0f;
	float lastCPX = 0.0f;
	float lastCPY = 0.0f;
	char lastCommand = 0;

	ptr = skipWhitespace(ptr, end);

	while (ptr != end) {
		char ch = *ptr;

		SSVG_CHECK(!stdutils::ascii::isspace(ch) && ch != ',', "Parse error");

		if (stdutils::ascii::isalpha(ch)) {
			++ptr;
		} else {
			ch = lastCommand;
		}

		const char lch = stdutils::ascii::tolower(ch);

		if (lch == 'm') {
			// MoveTo
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::MoveTo);
			ptr = parseCoord(ptr, end, &cmd->m_Data[0]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[1]);

			if (ch == lch) {
				cmd->m_Data[0] += lastX;
				cmd->m_Data[1] += lastY;
			}

			firstX = lastX = cmd->m_Data[0];
			firstY = lastY = cmd->m_Data[1];

			// https://www.w3.org/TR/SVG/paths.html#PathDataMovetoCommands
			// If a moveto is followed by multiple pairs of coordinates, the subsequent pairs are treated
			// as implicit lineto commands. Hence, implicit lineto commands will be relative if the moveto
			// is relative, and absolute if the moveto is absolute. If a relative moveto (m) appears as the
			// first element of the path, then it is treated as a pair of absolute coordinates. In this case,
			// subsequent pairs of coordinates are treated as relative even though the initial moveto is
			// interpreted as an absolute moveto.
			ch = stdutils::ascii::islower(ch) ? 'l' : 'L';
		} else if (lch == 'l') {
			// LineTo abs
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::LineTo);
			ptr = parseCoord(ptr, end, &cmd->m_Data[0]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[1]);

			if (ch == lch) {
				cmd->m_Data[0] += lastX;
				cmd->m_Data[1] += lastY;
			}

			lastX = cmd->m_Data[0];
			lastY = cmd->m_Data[1];
		} else if (lch == 'h') {
			// Horizontal LineTo abs
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::LineTo);
			ptr = parseCoord(ptr, end, &cmd->m_Data[0]);
			cmd->m_Data[1] = lastY;

			if (ch == lch) {
				cmd->m_Data[0] += lastX;
			}

			lastX = cmd->m_Data[0];
			lastY = cmd->m_Data[1];
		} else if (lch == 'v') {
			// Vertical LineTo abs
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::LineTo);
			ptr = parseCoord(ptr, end, &cmd->m_Data[1]);
			cmd->m_Data[0] = lastX;

			if (ch == lch) {
				cmd->m_Data[1] += lastY;
			}

			lastX = cmd->m_Data[0];
			lastY = cmd->m_Data[1];
		} else if (lch == 'z') {
			// ClosePath
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::ClosePath);
			UNUSED(cmd);
			lastX = firstX;
			lastY = firstY;
			// No data
			ptr = skipCommaWhitespace(ptr, end);
		} else if (lch == 'c') {
			// CubicTo abs
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::CubicTo);
			ptr = parseCoord(ptr, end, &cmd->m_Data[0]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[1]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[2]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[3]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[4]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[5]);

			if (ch == lch) {
				cmd->m_Data[0] += lastX;
				cmd->m_Data[1] += lastY;
				cmd->m_Data[2] += lastX;
				cmd->m_Data[3] += lastY;
				cmd->m_Data[4] += lastX;
				cmd->m_Data[5] += lastY;
			}

			lastCPX = cmd->m_Data[2];
			lastCPY = cmd->m_Data[3];
			lastX = cmd->m_Data[4];
			lastY = cmd->m_Data[5];
		} else if (lch == 's') {
			// CubicTo abs
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::CubicTo);
			ptr = parseCoord(ptr, end, &cmd->m_Data[2]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[3]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[4]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[5]);

			// The first control point is assumed to be the reflection of the second control point on
			// the previous command relative to the current point. (If there is no previous command or
			// if the previous command was not an C, c, S or s, assume the first control point is
			// coincident with the current point.)
			const char lastCmdLower = stdutils::ascii::tolower(lastCommand);
			if (lastCmdLower == 'c' || lastCmdLower == 's') {
				const float dx = lastX - lastCPX;
				const float dy = lastY - lastCPY;
				cmd->m_Data[0] = lastX + dx;
				cmd->m_Data[1] = lastY + dy;
			} else {
				cmd->m_Data[0] = lastX;
				cmd->m_Data[1] = lastY;
			}

			if (ch == lch) {
				cmd->m_Data[2] += lastX;
				cmd->m_Data[3] += lastY;
				cmd->m_Data[4] += lastX;
				cmd->m_Data[5] += lastY;
			}

			lastCPX = cmd->m_Data[2];
			lastCPY = cmd->m_Data[3];
			lastX = cmd->m_Data[4];
			lastY = cmd->m_Data[5];
		} else if (lch == 'q') {
			// QuadraticTo abs
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::QuadraticTo);
			ptr = parseCoord(ptr, end, &cmd->m_Data[0]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[1]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[2]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[3]);

			if (ch == lch) {
				cmd->m_Data[0] += lastX;
				cmd->m_Data[1] += lastY;
				cmd->m_Data[2] += lastX;
				cmd->m_Data[3] += lastY;
			}

			lastCPX = cmd->m_Data[0];
			lastCPY = cmd->m_Data[1];
			lastX = cmd->m_Data[2];
			lastY = cmd->m_Data[3];

			if ((flags & ImageLoadFlags::ConvertQuadToCubicBezier) != 0) {
				pathConvertCommand(path, (uint32_t)(cmd - path->m_Commands), PathCmdType::CubicTo);
			}
		} else if (lch == 't') {
			// QuadraticTo abs
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::QuadraticTo);
			ptr = parseCoord(ptr, end, &cmd->m_Data[2]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[3]);

			// The control point is assumed to be the reflection of the control point on the
			// previous command relative to the current point. (If there is no previous command
			// or if the previous command was not a Q, q, T or t, assume the control point is
			// coincident with the current point.)
			const char lastCmdLower = stdutils::ascii::tolower(lastCommand);
			if (lastCmdLower == 'q' || lastCmdLower == 't') {
				const float dx = lastX - lastCPX;
				const float dy = lastY - lastCPY;
				cmd->m_Data[0] = lastX + dx;
				cmd->m_Data[1] = lastY + dy;
			} else {
				cmd->m_Data[0] = lastX;
				cmd->m_Data[1] = lastY;
			}

			if (ch == lch) {
				cmd->m_Data[2] += lastX;
				cmd->m_Data[3] += lastY;
			}

			lastCPX = cmd->m_Data[0];
			lastCPY = cmd->m_Data[1];
			lastX = cmd->m_Data[2];
			lastY = cmd->m_Data[3];

			if ((flags & ImageLoadFlags::ConvertQuadToCubicBezier) != 0) {
				pathConvertCommand(path, (uint32_t)(cmd - path->m_Commands), PathCmdType::CubicTo);
			}
		} else if (lch == 'a') {
			// ArcTo abs
			PathCmd* cmd = pathAllocCommand(path, PathCmdType::ArcTo);
			ptr = parseCoord(ptr, end, &cmd->m_Data[0]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[1]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[2]);
			ptr = parseFlag(ptr, end, &cmd->m_Data[3]);
			ptr = parseFlag(ptr, end, &cmd->m_Data[4]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[5]);
			ptr = parseCoord(ptr, end, &cmd->m_Data[6]);

			if (ch == lch) {
				cmd->m_Data[5] += lastX;
				cmd->m_Data[6] += lastY;
			}

			lastX = cmd->m_Data[5];
			lastY = cmd->m_Data[6];

			if ((flags & ImageLoadFlags::ConvertArcToCubicBezier) != 0) {
				pathConvertCommand(path, (uint32_t)(cmd - path->m_Commands), PathCmdType::CubicTo);
			}
		} else {
			SSVG_WARN(false, "Encountered unknown path command");
			return false;
		}

		lastCommand = ch;
	}

	pathShrinkToFit(path);

	return true;
}

bool pointListFromString(PointList* ptList, const std::string_view& str)
{
	const char* ptr = str.data();
	const char* end = strend(str);
	while (ptr != end) {
		float* pt = pointListAllocPoints(ptList, 1);
		ptr = parseCoord(ptr, end, &pt[0]);
		ptr = parseCoord(ptr, end, &pt[1]);
	}

	pointListShrinkToFit(ptList);

	return true;
}

namespace {

ParseAttr::Result parseStyle(const std::string_view& str, ShapeAttributes* attrs)
{
	assert(attrs);
	const char* end = strend(str);
	const char* ptr = skipWhitespace(str.data(), end);
	while (ptr != end) {
		const char* nameStart = ptr;
		while (ptr != end && (stdutils::ascii::isalpha(*ptr) || *ptr == '-')) {
			ptr++;
		}

		if (ptr == end) {
			return ParseAttr::Fail;
		}

		assert(nameStart <= ptr);
		const std::string_view name(nameStart, ptr - nameStart);

		ptr = skipWhitespace(ptr, end);

		if (*ptr != ':') {
			return ParseAttr::Fail;
		}

		ptr = skipWhitespace(ptr + 1, end);

		const char* valueStart = ptr;
		while (ptr != end && *ptr != ';') {
			++ptr;
		}

		assert(valueStart <= ptr);
		const std::string_view value(valueStart, ptr - valueStart);

		ptr = skipWhitespace(ptr + (ptr != end ? 1 : 0), end);

		if (parseGenericShapeAttribute(name, value, attrs) == ParseAttr::Fail) {
			return ParseAttr::Fail;
		}
	}

	return ParseAttr::OK;
}

ParseAttr::Result parseGenericShapeAttribute(const std::string_view& name, const std::string_view& value, ShapeAttributes* attrs)
{
	if (name == "style") {
		return parseStyle(value, attrs);
	} else if (name.substr(0, 6) == "stroke") {
		const std::string_view nameSuffix = name.substr(6);
		if (nameSuffix.length() == 0) {
			attrs->m_Flags |= AttribFlags::StrokePaintChanged;
			return parsePaint(value, &attrs->m_StrokePaint) ? ParseAttr::OK : ParseAttr::Fail;
		} else if (nameSuffix == "-miterlimit") {
			attrs->m_Flags |= AttribFlags::StrokeMiterLimitChanged;
			return parseNumber(value, attrs->m_StrokeMiterLimit, 1.0f) ? ParseAttr::OK : ParseAttr::Fail;
		} else if (nameSuffix == "-linejoin") {
			attrs->m_Flags |= AttribFlags::StrokeLineJoinChanged;
			if (value == "miter") {
				attrs->m_StrokeLineJoin = LineJoin::Miter;
			} else if (value == "round") {
				attrs->m_StrokeLineJoin = LineJoin::Round;
			} else if (value == "bevel") {
				attrs->m_StrokeLineJoin = LineJoin::Bevel;
			} else {
				return ParseAttr::Fail;
			}

			return ParseAttr::OK;
		} else if (nameSuffix == "-linecap") {
			attrs->m_Flags |= AttribFlags::StrokeLineCapChanged;
			if (value == "butt") {
				attrs->m_StrokeLineCap = LineCap::Butt;
			} else if (value == "round") {
				attrs->m_StrokeLineCap = LineCap::Round;
			} else if (value == "square") {
				attrs->m_StrokeLineCap = LineCap::Square;
			} else {
				return ParseAttr::Fail;
			}

			return ParseAttr::OK;
		} else if (nameSuffix == "-opacity") {
			attrs->m_Flags |= AttribFlags::StrokeOpacityChanged;
			return parseNumber(value, attrs->m_StrokeOpacity, 0.0f, 1.0f) ? ParseAttr::OK : ParseAttr::Fail;
		} else if (nameSuffix == "-width") {
			attrs->m_Flags |= AttribFlags::StrokeWidthChanged;
			return parsePositiveLength(value, attrs->m_StrokeWidth) ? ParseAttr::OK : ParseAttr::Fail;
		}
	} else if (name.substr(0, 4) == "fill") {
		const std::string_view nameSuffix = name.substr(4);
		if (nameSuffix.length() == 0) {
			attrs->m_Flags |= AttribFlags::FillPaintChanged;
			return parsePaint(value, &attrs->m_FillPaint) ? ParseAttr::OK : ParseAttr::Fail;
		} else if (nameSuffix == "-opacity") {
			attrs->m_Flags |= AttribFlags::FillOpacityChanged;
			return parseNumber(value, attrs->m_FillOpacity, 0.0f, 1.0f) ? ParseAttr::OK : ParseAttr::Fail;
		} else if (nameSuffix == "-rule") {
			attrs->m_Flags |= AttribFlags::FillRuleChanged;
			if (value == "nonzero") {
				attrs->m_FillRule = FillRule::NonZero;
			} else if (value == "evenodd") {
				attrs->m_FillRule = FillRule::EvenOdd;
			} else {
				return ParseAttr::Fail;
			}

			return ParseAttr::OK;
		}
	} else if (name.substr(0, 4) == "font") {
		const std::string_view nameSuffix = name.substr(4);
		if (nameSuffix == "-family") {
			attrs->m_Flags |= AttribFlags::FontFamilyChanged;
			shapeAttrsSetFontFamily(attrs, value);
			return ParseAttr::OK;
		} else if (nameSuffix == "-size") {
			attrs->m_Flags |= AttribFlags::FontSizeChanged;
			return parsePositiveLength(value, attrs->m_FontSize) ? ParseAttr::OK : ParseAttr::Fail;
		}
	} else if (name == "opacity") {
		attrs->m_Flags |= AttribFlags::Opacity;
		return parseNumber(value, attrs->m_Opacity, 0.0f, 1.0f) ? ParseAttr::OK : ParseAttr::Fail;
	} else if (name == "transform") {
		attrs->m_Flags |= AttribFlags::Transformation;
		return parseTransform(value, &attrs->m_Transform[0]) ? ParseAttr::OK : ParseAttr::Fail;
	} else if (name == "id") {
		attrs->m_Flags |= AttribFlags::ElementID;
		shapeAttrsSetID(attrs, value);
		return ParseAttr::OK;
	} else if (name == "class") {
#if SSVG_CONFIG_CLASS_MAX_LEN
		attrs->m_Flags |= AttribFlags::ElementClass;
		shapeAttrsSetClass(attrs, value);
#endif
		return ParseAttr::OK;
	}

	return ParseAttr::Unknown;
}

void selectiveCopyShapeAttributes(ShapeAttributes* targetAttrs, const ShapeAttributes* sourceAttrs)
{
	assert(targetAttrs);
	assert(sourceAttrs);

	const auto flags = sourceAttrs->m_Flags;
	if (flags & AttribFlags::StrokePaintChanged) {
		targetAttrs->m_StrokePaint = sourceAttrs->m_StrokePaint;
	}
	if (flags & AttribFlags::StrokeMiterLimitChanged) {
		targetAttrs->m_StrokeMiterLimit = sourceAttrs->m_StrokeMiterLimit;
	}
	if (flags & AttribFlags::StrokeOpacityChanged) {
		targetAttrs->m_StrokeOpacity = sourceAttrs->m_StrokeOpacity;
	}
	if (flags & AttribFlags::StrokeWidthChanged) {
		targetAttrs->m_StrokeWidth = sourceAttrs->m_StrokeWidth;
	}
	if (flags & AttribFlags::StrokeLineJoinChanged) {
		targetAttrs->m_StrokeLineJoin = sourceAttrs->m_StrokeLineJoin;
	}
	if (flags & AttribFlags::StrokeLineCapChanged) {
		targetAttrs->m_StrokeLineCap = sourceAttrs->m_StrokeLineCap;
	}
	if (flags & AttribFlags::FillPaintChanged) {
		targetAttrs->m_FillPaint = sourceAttrs->m_FillPaint;
	}
	if (flags & AttribFlags::FillOpacityChanged) {
		targetAttrs->m_FillOpacity = sourceAttrs->m_FillOpacity;
	}
	if (flags & AttribFlags::FillRuleChanged) {
		targetAttrs->m_FillRule = sourceAttrs->m_FillRule;
	}
	if (flags & AttribFlags::FontSizeChanged) {
		targetAttrs->m_FontSize = sourceAttrs->m_FontSize;
	}
	if (flags & AttribFlags::FontFamilyChanged) {
		shapeAttrsSetFontFamily(targetAttrs, shapeAttrsGetFontFamily(sourceAttrs));
	}
	if (flags & AttribFlags::Opacity) {
		targetAttrs->m_Opacity = sourceAttrs->m_Opacity;
	}
	if (flags & AttribFlags::Transformation) {
		stdutils::memcpy<float>(&targetAttrs->m_Transform[0], TRANSFORM_ARRAY_SZ, &sourceAttrs->m_Transform[0], sizeof(float) * TRANSFORM_ARRAY_SZ);
	}
	if (flags & AttribFlags::ElementID) {
		shapeAttrsSetID(targetAttrs, shapeAttrsGetID(sourceAttrs));
	}
#if SSVG_CONFIG_CLASS_MAX_LEN
	if (flags & AttribFlags::ElementClass) {
		shapeAttrsSetClass(targetAttrs, shapeAttrsGetClass(sourceAttrs));
	}
#endif

	targetAttrs->m_Flags |= sourceAttrs->m_Flags;
}

bool parseNonShapeElement_Title(ParserState* parser, Group* group)
{
	assert(group);
	if (!group) { return false; }
	bool err = false;
	while (!parserIsDone(parser) && !err) {
		SSVG_CHECK(!(parser->m_Ptr[0] == '/' && parser->m_Ptr[1] == '>'), "Empty <title> element");
		if (parser->m_Ptr[0] == '>') {
			parser->m_Ptr++;
			break;
		}

		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			SSVG_WARN(false, "Ignoring title attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
		}
	}

	if (err || parserIsDone(parser)) {
		return false;
	}

	const char* txtBegin = parser->m_Ptr;
	while (!parserIsDone(parser) && std::string_view(parser->m_Ptr, 2) != "</") {
		++parser->m_Ptr;
	}
	const char* txtEnd = parser->m_Ptr;

	if (parserIsDone(parser) || !parserExpectingString(parser, "</title>")) {
		return false;
	}

	const uint32_t txtLen = (uint32_t)(txtEnd - txtBegin);
	std::string txt(txtBegin, txtEnd);	// TODO: filter text string
	groupSetTitle(group, txt.c_str());

	return true;
}

bool parseShape_Group(ParserState* parser, Shape* shape)
{
	assert(parser);
	if (!parser) { return false; }
	assert(parser->m_LengthContext);
	assert(shape);
	if (!shape) { return false; }
	Group& group = shape->m_Group;
	ShapeAttributes* attrs = shape->m_Attrs;
	SSVG_CHECK(attrs, "A contrainer type shape must always allocate its own ShapeAttributes");
	if (!attrs) { return false; }
	assert(attrs->m_Flags == AttribFlags::None);
	bool err = false;
	while (!parserIsDone(parser) && !err) {
		parserSkipWhitespace(parser);
		if (parserExpectingChar(parser, '>')) {
			// Expect aa closing tag, which is handled in this function
			break;
		} else if (parser->m_Ptr[0] == '/' && parser->m_Ptr[1] == '>') {
			// TODO: Test this!
			const auto group_id = shapeAttrsGetID(attrs);
			SSVG_WARN(false, "Empty group element id=\"%.*s\"", strlenint(group_id), group_id.data());
			parser->m_Ptr += 2;
			return true;
		}
		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			// Check if this a generic attribute (i.e. styling)
			ParseAttr::Result res = parseGenericShapeAttribute(name, value, attrs);
			if (res == ParseAttr::Fail) {
				err = true;
			} else if (res == ParseAttr::Unknown) {
				// No specific attributes for groups. Ignore it.
				SSVG_WARN(false, "Ignoring g attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
			}
		}
	}

	if (err) {
		return false;
	}

	if (parser->m_LengthContext) {
		parser->m_LengthContext->m_FontSize = convertLengthToPixel(attrs->m_FontSize, LengthAxis::Radial, parser->m_LengthContext);
	}

	return parseSVGElements(parser, &group, *attrs, "</g>");
}

bool parseShape_Text(ParserState* parser, Shape* shape)
{
	assert(shape);
	if (!shape) { return false; }
	Text& text = shape->m_Text;
	bool err = false;
	while (!parserIsDone(parser) && !err) {
		SSVG_CHECK(!(parser->m_Ptr[0] == '/' && parser->m_Ptr[1] == '>'), "Empty <text> element");
		if (parser->m_Ptr[0] == '>') {
			parser->m_Ptr++;
			break;
		}

		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			// Check if this a generic attribute (i.e. styling)
			ParseAttr::Result res = parseGenericShapeAttribute(name, value, &parser->m_ParsedShapeAttrs);
			if (res == ParseAttr::Fail) {
				err = true;
			} else if (res == ParseAttr::Unknown) {
				// Text specific attributes
				if (name == "x") {
					Length x;
					err = !parseLength(value, x);
					text.x = !err ? convertLengthToPixel(x, LengthAxis::X, parser->m_LengthContext) : 0.f;
				} else if (name == "y") {
					Length y;
					err = !parseLength(value, y);
					text.y = !err ? convertLengthToPixel(y, LengthAxis::Y, parser->m_LengthContext) : 0.f;
				} else if (name == "text-anchor") {
					if (value == "start") {
						text.m_Anchor = TextAnchor::Start;
					} else if (value == "middle") {
						text.m_Anchor = TextAnchor::Middle;
					} else if (value == "end") {
						text.m_Anchor = TextAnchor::End;
					} else {
						err = true;
					}
				} else {
					SSVG_WARN(false, "Ignoring text attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
				}
			}
		}
	}

	if (err || parserIsDone(parser)) {
		return false;
	}

	// TODO: Parse <tspan> blocks

	const char* txtBegin = parser->m_Ptr;
	while (!parserIsDone(parser) && std::string_view(parser->m_Ptr, 2) != "</") {
		++parser->m_Ptr;
	}
	const char* txtEnd = parser->m_Ptr;

	if (parserIsDone(parser) || !parserExpectingString(parser, "</text>")) {
		return false;
	}

	const uint32_t txtLen = (uint32_t)(txtEnd - txtBegin);
	std::string txt(txtBegin, txtEnd);	// TODO: filter text string
	textSetString(&text, txt.c_str());

	return true;
}

bool parseShape_Path(ParserState* parser, Shape* shape)
{
	assert(shape);
	if (!shape) { return false; }
	Path& path = shape->m_Path;
	bool err = false;
	while (!parserIsDone(parser) && !err) {
		parserSkipWhitespace(parser);
		if (parser->m_Ptr[0] == '>') {
			// NOTE: Don't skip the closing bracket because parserSkipTag() expects it.
			parser->m_ExpectClosingTag = true;
			break;
		} else if (parser->m_Ptr[0] == '/' && parser->m_Ptr[1] == '>') {
			parser->m_Ptr += 2;
			break;
		}

		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			// Check if this a generic attribute (i.e. styling)
			ParseAttr::Result res = parseGenericShapeAttribute(name, value, &parser->m_ParsedShapeAttrs);
			if (res == ParseAttr::Fail) {
				err = true;
			} else if (res == ParseAttr::Unknown) {
				// Path specific attributes.
				if (name == "d") {
					err = !pathFromString(&path, value, parser->m_Flags);
				} else {
					SSVG_WARN(false, "Ignoring path attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
				}
			}
		}
	}

	return !err;
}

bool parseShape_Rect(ParserState* parser, Shape* shape)
{
	assert(shape);
	if (!shape) { return false; }
	Rect& rect = shape->m_Rect;
	bool err = false;
	while (!parserIsDone(parser) && !err) {
		parserSkipWhitespace(parser);

		if (parser->m_Ptr[0] == '>') {
			// NOTE: Don't skip the closing bracket because parserSkipTag() expects it.
			parser->m_ExpectClosingTag = true;
			break;
		} else if (parser->m_Ptr[0] == '/' && parser->m_Ptr[1] == '>') {
			parser->m_Ptr += 2;
			break;
		}

		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			// Check if this a generic attribute (i.e. styling)
			ParseAttr::Result res = parseGenericShapeAttribute(name, value, &parser->m_ParsedShapeAttrs);
			if (res == ParseAttr::Fail) {
				err = true;
			} else if (res == ParseAttr::Unknown) {
				Length len;
				// Rect specific attributes.
				if (name == "width") {
					err = !parsePositiveLength(value, len);
					rect.width = !err ? convertLengthToPixel(len, LengthAxis::X, parser->m_LengthContext) : 0.f;
				} else if (name == "height") {
					err = !parsePositiveLength(value, len);
					rect.height = !err ? convertLengthToPixel(len, LengthAxis::Y, parser->m_LengthContext) : 0.f;
				} else if (name == "rx") {
					err = !parsePositiveLength(value, len);
					rect.rx = !err ? convertLengthToPixel(len, LengthAxis::X, parser->m_LengthContext) : 0.f;
				} else if (name == "ry") {
					err = !parsePositiveLength(value, len);
					rect.ry = !err ? convertLengthToPixel(len, LengthAxis::Y, parser->m_LengthContext) : 0.f;
				} else if (name == "x") {
					err = !parseLength(value, len);
					rect.x = !err ? convertLengthToPixel(len, LengthAxis::X, parser->m_LengthContext) : 0.f;
				} else if (name == "y") {
					err = !parseLength(value, len);
					rect.y = !err ? convertLengthToPixel(len, LengthAxis::Y, parser->m_LengthContext) : 0.f;
				} else {
					SSVG_WARN(false, "Ignoring rect attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
				}
			}
		}
	}

	return !err;
}

bool parseShape_Circle(ParserState* parser, Shape* shape)
{
	assert(shape);
	if (!shape) { return false; }
	Circle& circle = shape->m_Circle;
	bool err = false;
	while (!parserIsDone(parser) && !err) {
		parserSkipWhitespace(parser);

		if (parser->m_Ptr[0] == '>') {
			// NOTE: Don't skip the closing bracket because parserSkipTag() expects it.
			parser->m_ExpectClosingTag = true;
			break;
		} else if (parser->m_Ptr[0] == '/' && parser->m_Ptr[1] == '>') {
			parser->m_Ptr += 2;
			break;
		}

		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			// Check if this a generic attribute (i.e. styling)
			ParseAttr::Result res = parseGenericShapeAttribute(name, value, &parser->m_ParsedShapeAttrs);
			if (res == ParseAttr::Fail) {
				err = true;
			} else if (res == ParseAttr::Unknown) {
				Length len;
				// Circle specific attributes.
				if (name == "cx") {
					err = !parseLength(value, len);
					circle.cx = !err ? convertLengthToPixel(len, LengthAxis::X, parser->m_LengthContext) : 0.f;
				} else if (name == "cy") {
					err = !parseLength(value, len);
					circle.cy = !err ? convertLengthToPixel(len, LengthAxis::Y, parser->m_LengthContext) : 0.f;
				} else if (name == "r") {
					err = !parsePositiveLength(value, len);
					circle.r = !err ? convertLengthToPixel(len, LengthAxis::Radial, parser->m_LengthContext) : 0.f;
				} else {
					SSVG_WARN(false, "Ignoring circle attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
				}
			}
		}
	}

	return !err;
}

bool parseShape_Line(ParserState* parser, Shape* shape)
{
	assert(shape);
	if (!shape) { return false; }
	Line& line = shape->m_Line;
	bool err = false;
	while (!parserIsDone(parser) && !err) {
		parserSkipWhitespace(parser);

		if (parser->m_Ptr[0] == '>') {
			// NOTE: Don't skip the closing bracket because parserSkipTag() expects it.
			parser->m_ExpectClosingTag = true;
			break;
		} else if (parser->m_Ptr[0] == '/' && parser->m_Ptr[1] == '>') {
			parser->m_Ptr += 2;
			break;
		}

		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			// Check if this a generic attribute (i.e. styling)
			ParseAttr::Result res = parseGenericShapeAttribute(name, value, &parser->m_ParsedShapeAttrs);
			if (res == ParseAttr::Fail) {
				err = true;
			} else if (res == ParseAttr::Unknown) {
				Length len;
				// Line specific attributes.
				if (name == "x1") {
					err = !parseLength(value, len);
					line.x1 = !err ? convertLengthToPixel(len, LengthAxis::X, parser->m_LengthContext) : 0.f;
				} else if (name == "x2") {
					err = !parseLength(value, len);
					line.x2 = !err ? convertLengthToPixel(len, LengthAxis::X, parser->m_LengthContext) : 0.f;
				} else if (name == "y1") {
					err = !parseLength(value, len);
					line.y1 = !err ? convertLengthToPixel(len, LengthAxis::Y, parser->m_LengthContext) : 0.f;
				} else if (name == "y2") {
					err = !parseLength(value, len);
					line.y2 = !err ? convertLengthToPixel(len, LengthAxis::Y, parser->m_LengthContext) : 0.f;
				} else {
					SSVG_WARN(false, "Ignoring line attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
				}
			}
		}
	}

	return !err;
}

bool parseShape_Ellipse(ParserState* parser, Shape* shape)
{
	assert(shape);
	if (!shape) { return false; }
	Ellipse& ellipse = shape->m_Ellipse;
	bool err = false;
	while (!parserIsDone(parser) && !err) {
		parserSkipWhitespace(parser);

		if (parser->m_Ptr[0] == '>') {
			// NOTE: Don't skip the closing bracket because parserSkipTag() expects it.
			parser->m_ExpectClosingTag = true;
			break;
		} else if (parser->m_Ptr[0] == '/' && parser->m_Ptr[1] == '>') {
			parser->m_Ptr += 2;
			break;
		}

		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			// Check if this a generic attribute (i.e. styling)
			ParseAttr::Result res = parseGenericShapeAttribute(name, value, &parser->m_ParsedShapeAttrs);
			if (res == ParseAttr::Fail) {
				err = true;
			} else if (res == ParseAttr::Unknown) {
				Length len;
				// Ellipse specific attributes.
				if (name == "cx") {
					err = !parseLength(value, len);
					ellipse.cx = !err ? convertLengthToPixel(len, LengthAxis::X, parser->m_LengthContext) : 0.f;
				} else if (name == "cy") {
					err = !parseLength(value, len);
					ellipse.cy = !err ? convertLengthToPixel(len, LengthAxis::Y, parser->m_LengthContext) : 0.f;
				} else if (name == "rx") {
					err = !parsePositiveLength(value, len);
					ellipse.rx = !err ? convertLengthToPixel(len, LengthAxis::X, parser->m_LengthContext) : 0.f;
				} else if (name == "ry") {
					err = !parsePositiveLength(value, len);
					ellipse.ry = !err ? convertLengthToPixel(len, LengthAxis::Y, parser->m_LengthContext) : 0.f;
				} else {
					SSVG_WARN(false, "Ignoring ellipse attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
				}
			}
		}
	}

	return !err;
}

bool parseShape_PointList(ParserState* parser, Shape* shape)
{
	assert(shape);
	if (!shape) { return false; }
	PointList& pointList = shape->m_PointList;
	bool err = false;
	while (!parserIsDone(parser) && !err) {
		parserSkipWhitespace(parser);

		if (parser->m_Ptr[0] == '>') {
			// NOTE: Don't skip the closing bracket because parserSkipTag() expects it.
			parser->m_ExpectClosingTag = true;
			break;
		} else if (parser->m_Ptr[0] == '/' && parser->m_Ptr[1] == '>') {
			parser->m_Ptr += 2;
			break;
		}

		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			// Check if this a generic attribute (i.e. styling)
			ParseAttr::Result res = parseGenericShapeAttribute(name, value, &parser->m_ParsedShapeAttrs);
			if (res == ParseAttr::Fail) {
				err = true;
			} else if (res == ParseAttr::Unknown) {
				if (name == "points") {
					PointList ptList;
					stdutils::memset<PointList>(&ptList, 0);
					err = !pointListFromString(&ptList, value);

					if (!err && ptList.m_NumPoints >= 2 &&
						((shape->m_Type == ShapeType::Polygon && (parser->m_Flags & ImageLoadFlags::ConvertPolygonsToPaths) != 0) ||
						(shape->m_Type == ShapeType::Polyline && (parser->m_Flags & ImageLoadFlags::ConvertPolylinesToPaths) != 0)))
					{
						const float* coords = ptList.m_Coords;

						Path* path = &shape->m_Path;
						PathCmd* cmd = pathAllocCommand(path, PathCmdType::MoveTo);
						cmd->m_Data[0] = *coords++;
						cmd->m_Data[1] = *coords++;

						for (uint32_t i = 1; i < ptList.m_NumPoints; ++i) {
							cmd = pathAllocCommand(path, PathCmdType::LineTo);
							cmd->m_Data[0] = *coords++;
							cmd->m_Data[1] = *coords++;
						}

						if (shape->m_Type == ShapeType::Polygon) {
							pathAllocCommand(path, PathCmdType::ClosePath);
						}

						pointListClear(&ptList);
						shape->m_Type = ShapeType::Path;
					} else {
						stdutils::memcpy<PointList>(&pointList, &ptList);
					}
				} else {
					SSVG_WARN(false, "Ignoring polygon/polyline attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
				}
			}
		}
	}

	return !err;
}

bool parseSVGElements(ParserState* parser, Group* group, const ShapeAttributes& parentAttrs, std::string_view closingTag)
{
	assert(parser);
	assert(parser->m_LengthContext);
	assert(group);
	assert(closingTag.size() > 2 && closingTag[0] == '<' && closingTag[1] == '/' && closingTag[closingTag.size()-1] == '>');

	struct ParseNonShapeElementsFunc
	{
		std::string_view tag;
		bool(*parseFunc)(ParserState*, Group*);
	};
	static const ParseNonShapeElementsFunc parseNonShapeElementsFuncs[] = {
		{ std::string_view("title"), parseNonShapeElement_Title },
	};
	static const uint32_t numParseNonShapeElementFuncs = sizeof(parseNonShapeElementsFuncs) / sizeof(ParseNonShapeElementsFunc);

	struct ParseContainerFunc
	{
		std::string_view tag;
		ShapeType::Enum type;
		bool(*parseFunc)(ParserState*, Shape*);
	};
	static const ParseContainerFunc parseContainerFuncs[] = {
		{ std::string_view("g"),        ShapeType::Group,    parseShape_Group     },
	};
	static const uint32_t numParseContainerFuncs = sizeof(parseContainerFuncs) / sizeof(ParseContainerFunc);

	struct ParseShapeFunc
	{
		std::string_view tag;
		ShapeType::Enum type;
		bool(*parseFunc)(ParserState*, Shape*);
	};
	static const ParseShapeFunc parseShapeFuncs[] = {
		{ std::string_view("polyline"), ShapeType::Polyline, parseShape_PointList },
		{ std::string_view("polygon"),  ShapeType::Polygon,  parseShape_PointList },
		{ std::string_view("ellipse"),  ShapeType::Ellipse,  parseShape_Ellipse   },
		{ std::string_view("circle"),   ShapeType::Circle,   parseShape_Circle    },
		{ std::string_view("line"),     ShapeType::Line,     parseShape_Line      },
		{ std::string_view("rect"),     ShapeType::Rect,     parseShape_Rect      },
		{ std::string_view("path"),     ShapeType::Path,     parseShape_Path      },
		{ std::string_view("text"),     ShapeType::Text,     parseShape_Text      },
	};
	static const uint32_t numParseShapeFuncs = sizeof(parseShapeFuncs) / sizeof(ParseShapeFunc);

	bool err = false;
	std::string_view tag;

	ShapeList* shapeList = &group->m_ShapeList;

	// Parse until the end-of-buffer
	while (!parserIsDone(parser)) {
		parserSkipWhitespaceAndComments(parser);
		if (parserMatchString(parser, closingTag)) {
			break;
		}

		tag = parserGetTag(parser);
		if (tag.empty()) {
			err = true;
			break;
		}

		bool tagFound = false;

		// Non-rendered elements (e.g. <title>)
		for (uint32_t i = 0; i < numParseNonShapeElementFuncs && !tagFound; ++i) {
			const auto& parseNonShapeElementsFunc = parseNonShapeElementsFuncs[i];
			if (tag == parseNonShapeElementsFunc.tag) {
				tagFound = true;
				// Parse the non-shape element
				err = !parseNonShapeElementsFunc.parseFunc(parser, group);

				break;
			}
		}
		if (err) {
			break;
		}

		// Containers (e.g. <g>)
		for (uint32_t i = 0; i < numParseContainerFuncs && !tagFound; ++i) {
			const auto& parseContainerFunc = parseContainerFuncs[i];
			if (tag == parseContainerFunc.tag) {
				tagFound = true;
				const auto type = parseContainerFunc.type;
				Shape* shape = shapeListAllocShape(shapeList, type);
				SSVG_CHECK(shape != nullptr, "Shape allocation failed");

				// For container, always preallocate the attributes:
				//   - groups almost always have an ID
				//   - The recursive nature of parsing a group requires the copy of the parent attrs
				shapeAllocAttributes(shape, &parentAttrs);

				LengthContext lengthContext = *parser->m_LengthContext;
				stdutils::ScopedPtrToLocal<LengthContext> scopedLengthContext(&parser->m_LengthContext, lengthContext);

				// Parse the shape
				err = !parseContainerFunc.parseFunc(parser, shape);

				break;
			}
		}
		if (err) {
			break;
		}

		// Rendered elements (e.g. <path>, <rect>, <text> etc.)
		for (uint32_t i = 0; i < numParseShapeFuncs && !tagFound; ++i) {
			const auto& parseShapeFunc = parseShapeFuncs[i];
			if (tag == parseShapeFunc.tag) {
				tagFound = true;
				const auto type = parseShapeFunc.type;
				Shape* shape = shapeListAllocShape(shapeList, type);
				SSVG_CHECK(shape != nullptr, "Shape allocation failed");
				parser->m_ParsedShapeAttrs.m_Flags = AttribFlags::None;
				parser->m_ExpectClosingTag = false;

				// Parse the shape
				err = !parseShapeFunc.parseFunc(parser, shape);

				if (!err && parser->m_ParsedShapeAttrs.m_Flags) {
					ShapeAttributes* shapeAttrs = shapeAllocAttributes(shape, &parentAttrs);
					selectiveCopyShapeAttributes(shapeAttrs, &parser->m_ParsedShapeAttrs);
				}

				if (!err && parser->m_ExpectClosingTag) {
					err = !parserSkipTag(parser, tag);
				}

				break;
			}
		}
		if (err) {
			break;
		}

		SSVG_WARN(tagFound, "Ignoring element <%.*s>", strlenint(tag), tag.data());
		if (!tagFound) {
			err = !parserSkipTag(parser, tag);
		}
	}

	SSVG_WARN(!err, "Error parsing the SVG elements. Last tag: \"%s\"", std::string(tag).c_str());
	SSVG_WARN(!parserIsDone(parser), "End of stream before the closing tag \"%s\". Last opening tag: \"%s\"", std::string(closingTag).c_str(), std::string(tag).c_str());
	if (err || parserIsDone(parser)) {
		return false;
	}

	shapeListShrinkToFit(shapeList);

	// Skip the closing tag
	return parserExpectingString(parser, closingTag);
}

bool parseTag_svg(ParserState* parser, Image* img)
{
	assert(parser);
	if (!parser) { return false; }

	// Parse svg tag attributes...
	bool err = false;
	bool hasViewBox = false;
	while (!parserIsDone(parser) && !err) {
		if (parserExpectingChar(parser, '>')) {
			break;
		}

		std::string_view name, value;
		if (!parserGetAttribute(parser, &name, &value)) {
			err = true;
		} else {
			if (name == "version") {
				parseVersion(value, &img->m_VerMajor, &img->m_VerMinor);
			} else if (name == "baseProfile") {
				if (value == "full") {
					img->m_BaseProfile = BaseProfile::Full;
				} else if (value == "basic") {
					img->m_BaseProfile = BaseProfile::Basic;
				} else if (value == "tiny") {
					img->m_BaseProfile = BaseProfile::Tiny;
				} else {
					// Unknown base profile. Ignore.
					SSVG_WARN(false, "Unknown baseProfile \"%.*s\"", strlenint(value), value.data());
				}
			} else if (name == "width") {
				IGNORE_RETURN parsePositiveLength(value, img->m_Width);
			} else if (name == "height") {
				IGNORE_RETURN parsePositiveLength(value, img->m_Height);
			} else if (name == "viewBox") {
				hasViewBox = parseViewBox(value, &img->m_ViewBox[0]);
				SSVG_WARN(hasViewBox, "Failed to parse the ViewBox");
			} else if (name == "xmlns" || name == "id") {
				// Ignore. This is here in order to shut up the trace message below.
			} else {
				// Unknown attribute. Ignore it (parser has already moved forward)
				SSVG_WARN(false, "Ignoring SVG attribute: %.*s=\"%.*s\"", strlenint(name), name.data(), strlenint(value), value.data());
			}
		}
	}

	SSVG_WARN(hasViewBox, "The SVG has no ViewBox");

	if (err) {
		return false;
	}

	// Set the length context, used to convert length units to pixels
	LengthContext lengthContext = [img, hasViewBox]() {
		LengthContext context;
		const float viewBoxWidth  = hasViewBox ? img->m_ViewBox[2] : 0.f;
		const float viewBoxHeight = hasViewBox ? img->m_ViewBox[3] : 0.f;
		assert(img->m_BaseAttrs.m_FontSize.m_Unit != LengthUnit::EM
			&& img->m_BaseAttrs.m_FontSize.m_Unit != LengthUnit::EX
			&& img->m_BaseAttrs.m_FontSize.m_Unit != LengthUnit::Percent);
		context.m_FontSize = convertLengthToPixel(img->m_BaseAttrs.m_FontSize);
		context.m_ViewportWidth  = SSVG_CONFIG_PARSER_DEFAULT_VIEWPORT_WIDTH_IN_PX;
		context.m_ViewportHeight = SSVG_CONFIG_PARSER_DEFAULT_VIEWPORT_HEIGHT_IN_PX;
		if (viewBoxWidth > 0.f && viewBoxHeight > 0.f) {
			context.m_ViewportWidth  = viewBoxWidth;
			context.m_ViewportHeight = viewBoxHeight;
		} else if (img->m_Width.m_Length  > 0.f && img->m_Width.m_Unit  != LengthUnit::Percent
				&& img->m_Height.m_Length > 0.f && img->m_Height.m_Unit != LengthUnit::Percent) {
			context.m_ViewportWidth  = convertLengthToPixel(img->m_Width,  LengthAxis::X, &context);
			context.m_ViewportHeight = convertLengthToPixel(img->m_Height, LengthAxis::Y, &context);
		}
		context.m_ViewportDiag = math::normalizedDiagonal(context.m_ViewportWidth, context.m_ViewportHeight);
		assert(context.m_FontSize > 0.f);
		assert(context.m_ViewportWidth > 0.f);
		assert(context.m_ViewportHeight > 0.f);
		assert(context.m_ViewportDiag > 0.f);
		return context;
	}();
	parser->m_LengthContext = &lengthContext;
	return parseSVGElements(parser, &img->m_RootContainer, img->m_BaseAttrs, "</svg>");
}

ParserState initialParserState(const char* xmlStr, uint32_t flags)
{
	ParserState parser;
	stdutils::memset<ParserState>(&parser, 0);
	parser.m_XMLString = xmlStr;
	parser.m_Ptr = xmlStr;
	parser.m_Flags = flags;

	return parser;
}

} // namespace

Image* imageLoad(const char* xmlStr, uint32_t flags, const ShapeAttributes* baseAttrs)
{
	if (!xmlStr || *xmlStr == '\0') {
		return nullptr;
	}

	Image* img = imageCreate(baseAttrs);
	if (!img) { return nullptr; }

	ParserState parser = initialParserState(xmlStr, flags);

	bool err = false;
	while (!parserIsDone(&parser) && !err) {
		const std::string_view tag = parserGetTag(&parser);
		if (tag.empty()) {
			err = !parserIsDone(&parser);
		} else {
			if (tag == "?xml") {
				// Special case: Search for "?>".
				while (!parserIsDone(&parser)) {
					if (parser.m_Ptr[0] == '?' && parser.m_Ptr[1] == '>') {
						parser.m_Ptr += 2;
						break;
					}
					++parser.m_Ptr;
				}

				err = parserIsDone(&parser);
			} else if (tag == "!DOCTYPE") {
				// Special case: Search for first '>'.
				while (!parserIsDone(&parser)) {
					char ch = *parser.m_Ptr++;
					if (ch == '>') {
						break;
					}
				}

				err = parserIsDone(&parser);
			} else if (tag == "svg") {
				err = !parseTag_svg(&parser, img);
				if (!err && (parser.m_Flags & ImageLoadFlags::CalcShapeBounds) != 0) {
					shapeListCalcBounds(&img->m_RootContainer.m_ShapeList, &img->m_BoundingRect[0]);
				}
			} else {
				SSVG_WARN(false, "Ignoring unknown XML root tag %.*s", strlenint(tag), tag.data());
				err = !parserSkipTag(&parser, tag);
			}
		}
	}

	if (err) {
		imageFree(img);
		img = nullptr;
	}

	return img;
}

} // namespace ssvg
