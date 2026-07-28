/*              V R M L 1 _ P A R S E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "vrml1_parser.h"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace vrml1 {
namespace {

enum class TokenType {
    End,
    Identifier,
    Number,
    String,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    LeftParen,
    RightParen,
    Pipe
};

struct Token {
    TokenType type = TokenType::End;
    std::string text;
    size_t line = 1;
    size_t column = 1;
};

class Lexer {
public:
    explicit Lexer(const std::string &input) : input_(input) {}

    Token next()
    {
	skip_space_and_comments();
	Token token;
	token.line = line_;
	token.column = column_;
	if (position_ == input_.size()) return token;

	const char current = input_[position_];
	switch (current) {
	    case '{': return punctuation(TokenType::LeftBrace);
	    case '}': return punctuation(TokenType::RightBrace);
	    case '[': return punctuation(TokenType::LeftBracket);
	    case ']': return punctuation(TokenType::RightBracket);
	    case '(': return punctuation(TokenType::LeftParen);
	    case ')': return punctuation(TokenType::RightParen);
	    case '|': return punctuation(TokenType::Pipe);
	    case '"': return string_token();
	    default: return word_token();
	}
    }

private:
    Token punctuation(TokenType type)
    {
	Token token;
	token.type = type;
	token.text.assign(1, input_[position_]);
	token.line = line_;
	token.column = column_;
	advance();
	return token;
    }

    Token string_token()
    {
	Token token;
	token.type = TokenType::String;
	token.line = line_;
	token.column = column_;
	advance();
	while (position_ < input_.size()) {
	    const char current = input_[position_];
	    if (current == '"') {
		advance();
		return token;
	    }
	    if (current == '\\' && position_ + 1 < input_.size()) {
		advance();
		switch (input_[position_]) {
		    case 'n': token.text.push_back('\n'); break;
		    case 'r': token.text.push_back('\r'); break;
		    case 't': token.text.push_back('\t'); break;
		    default: token.text.push_back(input_[position_]); break;
		}
		advance();
		continue;
	    }
	    token.text.push_back(current);
	    advance();
	}
	token.type = TokenType::End;
	token.text = "unterminated string";
	return token;
    }

    Token word_token()
    {
	Token token;
	token.line = line_;
	token.column = column_;
	const size_t start = position_;
	while (position_ < input_.size()) {
	    const char current = input_[position_];
	    if (is_separator(current)) break;
	    advance();
	}
	token.text = input_.substr(start, position_ - start);

	char *end = nullptr;
	errno = 0;
	std::strtod(token.text.c_str(), &end);
	token.type = errno == 0 && end && *end == '\0' ? TokenType::Number : TokenType::Identifier;
	return token;
    }

    static bool is_separator(char value)
    {
	switch (value) {
	    case ' ': case '\t': case '\r': case '\n':
	    case '{': case '}': case '[': case ']':
	    case '(': case ')': case ',': case '|': case '"': case '#':
		return true;
	    default:
		return false;
	}
    }

    void skip_space_and_comments()
    {
	while (position_ < input_.size()) {
	    const char current = input_[position_];
	    if (current == '#') {
		while (position_ < input_.size() && input_[position_] != '\n') advance();
		continue;
	    }
	    if (current == ' ' || current == '\t' || current == '\r' || current == '\n' || current == ',') {
		advance();
		continue;
	    }
	    break;
	}
    }

    void advance()
    {
	if (input_[position_] == '\n') {
	    ++line_;
	    column_ = 1;
	} else {
	    ++column_;
	}
	++position_;
    }

    const std::string &input_;
    size_t position_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
};

enum class FieldType {
    Float,
    Integer,
    Vec2,
    Vec3,
    Rotation,
    Matrix,
    String,
    Enum,
    BitMask,
    MultiFloat,
    MultiInteger,
    MultiString,
    MultiVec2,
    MultiVec3,
    Image
};

using FieldSchema = std::unordered_map<std::string, FieldType>;
using NodeSchemas = std::unordered_map<std::string, FieldSchema>;

const NodeSchemas &
schemas()
{
    static const NodeSchemas node_schemas = {
	{"AsciiText", {{"string", FieldType::MultiString}, {"spacing", FieldType::Float},
	    {"justification", FieldType::Enum}, {"width", FieldType::MultiFloat}}},
	{"Cone", {{"parts", FieldType::BitMask}, {"bottomRadius", FieldType::Float},
	    {"height", FieldType::Float}}},
	{"Cube", {{"width", FieldType::Float}, {"height", FieldType::Float},
	    {"depth", FieldType::Float}}},
	{"Cylinder", {{"parts", FieldType::BitMask}, {"radius", FieldType::Float},
	    {"height", FieldType::Float}}},
	{"IndexedFaceSet", {{"coordIndex", FieldType::MultiInteger},
	    {"materialIndex", FieldType::MultiInteger}, {"normalIndex", FieldType::MultiInteger},
	    {"textureCoordIndex", FieldType::MultiInteger}}},
	{"IndexedLineSet", {{"coordIndex", FieldType::MultiInteger},
	    {"materialIndex", FieldType::MultiInteger}, {"normalIndex", FieldType::MultiInteger},
	    {"textureCoordIndex", FieldType::MultiInteger}}},
	{"PointSet", {{"startIndex", FieldType::Integer}, {"numPoints", FieldType::Integer}}},
	{"Sphere", {{"radius", FieldType::Float}}},
	{"Coordinate3", {{"point", FieldType::MultiVec3}}},
	{"FontStyle", {{"size", FieldType::Float}, {"family", FieldType::Enum},
	    {"style", FieldType::BitMask}}},
	{"Info", {{"string", FieldType::String}}},
	{"Material", {{"ambientColor", FieldType::MultiVec3}, {"diffuseColor", FieldType::MultiVec3},
	    {"specularColor", FieldType::MultiVec3}, {"emissiveColor", FieldType::MultiVec3},
	    {"shininess", FieldType::MultiFloat}, {"transparency", FieldType::MultiFloat}}},
	{"MaterialBinding", {{"value", FieldType::Enum}}},
	{"Normal", {{"vector", FieldType::MultiVec3}}},
	{"NormalBinding", {{"value", FieldType::Enum}}},
	{"Texture2", {{"filename", FieldType::String}, {"image", FieldType::Image},
	    {"wrapS", FieldType::Enum}, {"wrapT", FieldType::Enum}}},
	{"Texture2Transform", {{"translation", FieldType::Vec2}, {"rotation", FieldType::Float},
	    {"scaleFactor", FieldType::Vec2}, {"center", FieldType::Vec2}}},
	{"TextureCoordinate2", {{"point", FieldType::MultiVec2}}},
	{"ShapeHints", {{"vertexOrdering", FieldType::Enum}, {"shapeType", FieldType::Enum},
	    {"faceType", FieldType::Enum}, {"creaseAngle", FieldType::Float}}},
	{"MatrixTransform", {{"matrix", FieldType::Matrix}}},
	{"Rotation", {{"rotation", FieldType::Rotation}}},
	{"Scale", {{"scaleFactor", FieldType::Vec3}}},
	{"Transform", {{"translation", FieldType::Vec3}, {"rotation", FieldType::Rotation},
	    {"scaleFactor", FieldType::Vec3}, {"scaleOrientation", FieldType::Rotation},
	    {"center", FieldType::Vec3}}},
	{"Translation", {{"translation", FieldType::Vec3}}},
	{"Separator", {{"renderCaching", FieldType::Enum}, {"boundingBoxCaching", FieldType::Enum},
	    {"renderCulling", FieldType::Enum}, {"pickCulling", FieldType::Enum}}},
	{"TransformSeparator", {}},
	{"Group", {}},
	{"Switch", {{"whichChild", FieldType::Integer}}},
	{"WWWAnchor", {{"name", FieldType::String}, {"description", FieldType::String},
	    {"map", FieldType::Enum}}},
	{"LOD", {{"range", FieldType::MultiFloat}, {"center", FieldType::Vec3}}},
	{"OrthographicCamera", {{"position", FieldType::Vec3}, {"orientation", FieldType::Rotation},
	    {"focalDistance", FieldType::Float}, {"height", FieldType::Float}}},
	{"PerspectiveCamera", {{"position", FieldType::Vec3}, {"orientation", FieldType::Rotation},
	    {"focalDistance", FieldType::Float}, {"heightAngle", FieldType::Float}}},
	{"DirectionalLight", {{"on", FieldType::Enum}, {"intensity", FieldType::Float},
	    {"color", FieldType::Vec3}, {"direction", FieldType::Vec3}}},
	{"PointLight", {{"on", FieldType::Enum}, {"intensity", FieldType::Float},
	    {"color", FieldType::Vec3}, {"location", FieldType::Vec3}}},
	{"SpotLight", {{"on", FieldType::Enum}, {"intensity", FieldType::Float},
	    {"color", FieldType::Vec3}, {"location", FieldType::Vec3},
	    {"direction", FieldType::Vec3}, {"dropOffRate", FieldType::Float},
	    {"cutOffAngle", FieldType::Float}}},
	{"WWWInline", {{"name", FieldType::String}, {"bboxSize", FieldType::Vec3},
	    {"bboxCenter", FieldType::Vec3}}}
    };
    return node_schemas;
}

size_t
fixed_number_count(FieldType type)
{
    switch (type) {
	case FieldType::Float:
	case FieldType::Integer:
	    return 1;
	case FieldType::Vec2:
	    return 2;
	case FieldType::Vec3:
	    return 3;
	case FieldType::Rotation:
	    return 4;
	case FieldType::Matrix:
	    return 16;
	default:
	    return 0;
    }
}

size_t
multi_number_stride(FieldType type)
{
    switch (type) {
	case FieldType::MultiFloat:
	case FieldType::MultiInteger:
	    return 1;
	case FieldType::MultiVec2:
	    return 2;
	case FieldType::MultiVec3:
	    return 3;
	default:
	    return 0;
    }
}

bool
token_to_integer(const Token &token, long long &value)
{
    char *end = nullptr;
    errno = 0;
    value = std::strtoll(token.text.c_str(), &end, 0);
    return errno == 0 && end && *end == '\0';
}

} // namespace

class Parser::Impl {
public:
    explicit Impl(const std::string &input) : lexer_(input)
    {
	advance();
    }

    bool parse(std::vector<NodePtr> &nodes, std::string &error)
    {
	while (current_.type != TokenType::End) {
	    NodePtr node;
	    if (!parse_node(node)) {
		error = error_;
		return false;
	    }
	    if (node) nodes.push_back(node);
	}
	if (!error_.empty()) {
	    error = error_;
	    return false;
	}
	return true;
    }

private:
    bool parse_node(NodePtr &node)
    {
	std::string def_name;
	if (is_identifier("DEF")) {
	    advance();
	    if (!expect(TokenType::Identifier, "DEF name")) return false;
	    def_name = current_.text;
	    advance();
	}

	if (is_identifier("USE")) {
	    advance();
	    if (!expect(TokenType::Identifier, "USE name")) return false;
	    const auto found = definitions_.find(current_.text);
	    if (found == definitions_.end()) return fail("undefined USE name '" + current_.text + "'");
	    node = found->second;
	    advance();
	    return true;
	}

	if (!expect(TokenType::Identifier, "node type")) return false;
	const std::string type = current_.text;
	advance();
	if (!expect(TokenType::LeftBrace, "'{' after node type")) return false;
	advance();

	node = std::make_shared<Node>();
	node->type = type;
	node->def_name = def_name;

	const auto schema_it = schemas().find(type);
	if (schema_it == schemas().end()) {
	    if (!skip_unknown_node(node)) return false;
	    if (!def_name.empty()) definitions_[def_name] = node;
	    return true;
	}

	while (current_.type != TokenType::RightBrace) {
	    if (current_.type == TokenType::End) return fail("unterminated " + type + " node");

	    if (current_.type == TokenType::Identifier) {
		const auto field_it = schema_it->second.find(current_.text);
		if (field_it != schema_it->second.end()) {
		    const std::string field_name = current_.text;
		    advance();
		    Field value;
		    if (!parse_field(field_it->second, value)) return false;
		    node->fields[field_name] = std::move(value);
		    continue;
		}
	    }

	    NodePtr child;
	    if (!parse_node(child)) return false;
	    if (child) node->children.push_back(child);
	}
	advance();
	if (!def_name.empty()) definitions_[def_name] = node;
	return true;
    }

    bool skip_unknown_node(NodePtr &node)
    {
	size_t depth = 1;
	while (current_.type != TokenType::End && depth) {
	    if (current_.type == TokenType::LeftBrace) ++depth;
	    if (current_.type == TokenType::RightBrace) --depth;
	    advance();
	}
	if (depth) return fail("unterminated " + node->type + " node");
	return true;
    }

    bool parse_field(FieldType type, Field &value)
    {
	if (type == FieldType::String) {
	    if (!expect(TokenType::String, "string value")) return false;
	    value.strings.push_back(current_.text);
	    advance();
	    return true;
	}
	if (type == FieldType::Enum || type == FieldType::BitMask) return parse_symbols(type, value);
	if (type == FieldType::MultiString) return parse_multi_strings(value);
	if (type == FieldType::Image) return parse_image(value);

	const size_t count = fixed_number_count(type);
	if (count) return parse_fixed_numbers(type, count, value);
	return parse_multi_numbers(type, multi_number_stride(type), value);
    }

    bool parse_fixed_numbers(FieldType type, size_t count, Field &value)
    {
	for (size_t i = 0; i < count; ++i) {
	    if (type == FieldType::Integer) {
		long long integer = 0;
		if (!token_to_integer(current_, integer))
		    return fail("invalid integer '" + current_.text + "'");
		value.integers.push_back(integer);
	    } else {
		if (!expect(TokenType::Number, "numeric value")) return false;
		value.numbers.push_back(std::strtod(current_.text.c_str(), nullptr));
	    }
	    advance();
	}
	return true;
    }

    bool parse_multi_numbers(FieldType type, size_t stride, Field &value)
    {
	if (!stride) return fail("internal error: invalid numeric field type");
	const bool bracketed = current_.type == TokenType::LeftBracket;
	if (bracketed) advance();

	size_t count = 0;
	while (true) {
	    if (type == FieldType::MultiInteger) {
		long long integer = 0;
		if (!token_to_integer(current_, integer)) break;
		value.integers.push_back(integer);
	    } else {
		if (current_.type != TokenType::Number) break;
		value.numbers.push_back(std::strtod(current_.text.c_str(), nullptr));
	    }
	    ++count;
	    advance();
	    if (!bracketed && count == stride) break;
	}
	if (bracketed) {
	    if (!expect(TokenType::RightBracket, "']' after list")) return false;
	    advance();
	}
	if (count % stride) return fail("numeric list does not contain complete vector values");
	return true;
    }

    bool parse_multi_strings(Field &value)
    {
	const bool bracketed = current_.type == TokenType::LeftBracket;
	if (bracketed) advance();
	while (current_.type == TokenType::String) {
	    value.strings.push_back(current_.text);
	    advance();
	    if (!bracketed) break;
	}
	if (bracketed) {
	    if (!expect(TokenType::RightBracket, "']' after string list")) return false;
	    advance();
	}
	return true;
    }

    bool parse_symbols(FieldType type, Field &value)
    {
	const bool parenthesized = current_.type == TokenType::LeftParen;
	if (parenthesized) advance();
	if (!expect(TokenType::Identifier, "enumeration value")) return false;
	value.symbol = current_.text;
	advance();
	if (type == FieldType::BitMask) {
	    while (current_.type == TokenType::Pipe) {
		advance();
		if (!expect(TokenType::Identifier, "bitmask value")) return false;
		value.symbol.append("|").append(current_.text);
		advance();
	    }
	}
	if (parenthesized) {
	    if (!expect(TokenType::RightParen, "')' after enumeration")) return false;
	    advance();
	}
	return true;
    }

    bool parse_image(Field &value)
    {
	for (size_t i = 0; i < 3; ++i) {
	    if (current_.type != TokenType::Number && current_.type != TokenType::Identifier)
		return fail("expected image dimension");
	    long long integer = 0;
	    if (!token_to_integer(current_, integer)) return fail("invalid image value '" + current_.text + "'");
	    value.integers.push_back(integer);
	    advance();
	}
	if (value.integers[0] < 0 || value.integers[1] < 0)
	    return fail("image dimensions cannot be negative");
	const unsigned long long width = static_cast<unsigned long long>(value.integers[0]);
	const unsigned long long height = static_cast<unsigned long long>(value.integers[1]);
	if (height && width > std::numeric_limits<size_t>::max() / height)
	    return fail("image dimensions are too large");
	const size_t pixels = static_cast<size_t>(width * height);
	for (size_t i = 0; i < pixels; ++i) {
	    if (current_.type != TokenType::Number && current_.type != TokenType::Identifier)
		return fail("expected image pixel");
	    long long integer = 0;
	    if (!token_to_integer(current_, integer)) return fail("invalid image pixel '" + current_.text + "'");
	    value.integers.push_back(integer);
	    advance();
	}
	return true;
    }

    bool is_identifier(const char *value) const
    {
	return current_.type == TokenType::Identifier && current_.text == value;
    }

    bool expect(TokenType type, const char *description)
    {
	if (current_.type == type) return true;
	return fail(std::string("expected ") + description + ", found '" + current_.text + "'");
    }

    bool fail(const std::string &message)
    {
	std::ostringstream stream;
	stream << "line " << current_.line << ", column " << current_.column << ": " << message;
	error_ = stream.str();
	return false;
    }

    void advance()
    {
	current_ = lexer_.next();
	if (current_.type == TokenType::End && current_.text == "unterminated string") fail(current_.text);
    }

    Lexer lexer_;
    Token current_;
    std::unordered_map<std::string, NodePtr> definitions_;
    std::string error_;
};

bool
Parser::parse(const std::string &input, std::vector<NodePtr> &nodes, std::string &error)
{
    nodes.clear();
    error.clear();
    Impl implementation(input);
    return implementation.parse(nodes, error);
}

const Field *
field(const Node &node, const char *name)
{
    const auto found = node.fields.find(name);
    return found == node.fields.end() ? nullptr : &found->second;
}

} // namespace vrml1

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
