// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureAI — the little JSON this tool needs: reading its request, the AI clients' output, and the
// proposal at the end of an answer. A full library would be a heavy dependency for that; this parser
// covers the whole grammar (objects, arrays, strings with escapes and surrogate pairs, numbers, literals)
// and nothing else.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace cygc::json
{
	struct Value
	{
		enum class Type { Null, Bool, Number, String, Array, Object };
		Type type = Type::Null;
		bool boolean = false;
		double number = 0.0;
		std::string string;
		std::vector<Value> array;
		std::vector<std::pair<std::string, Value>> object;

		const Value *Get(const char *key) const
		{
			if (type != Type::Object)
				return nullptr;
			for (const auto &[name, value] : object)
				if (name == key)
					return &value;
			return nullptr;
		}
		std::string String(const char *key, const std::string &fallback = {}) const
		{
			const Value *const value = Get(key);
			return value != nullptr && value->type == Type::String ? value->string : fallback;
		}
		double Number(const char *key, double fallback = 0.0) const
		{
			const Value *const value = Get(key);
			return value != nullptr && value->type == Type::Number ? value->number : fallback;
		}
		bool Bool(const char *key, bool fallback = false) const
		{
			const Value *const value = Get(key);
			return value != nullptr && value->type == Type::Bool ? value->boolean : fallback;
		}
	};

	class Parser
	{
	public:
		Parser(const char *begin, const char *end) : p_(begin), end_(end) {}

		/** Parses one value; `Position()` then points right after it. */
		bool Parse(Value &out, int depth = 0)
		{
			SkipSpace();
			if (p_ >= end_ || depth > 64)
				return false;
			switch (*p_)
			{
			case '{': return ParseObject(out, depth);
			case '[': return ParseArray(out, depth);
			case '"': out.type = Value::Type::String; return ParseString(out.string);
			case 't': return Literal("true", out, Value::Type::Bool, true);
			case 'f': return Literal("false", out, Value::Type::Bool, false);
			case 'n': return Literal("null", out, Value::Type::Null, false);
			default: return ParseNumber(out);
			}
		}
		const char *Position() const { return p_; }

	private:
		void SkipSpace()
		{
			while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r'))
				++p_;
		}
		bool Literal(const char *word, Value &out, Value::Type type, bool boolean)
		{
			for (const char *w = word; *w != '\0'; ++w, ++p_)
				if (p_ >= end_ || *p_ != *w)
					return false;
			out.type = type;
			out.boolean = boolean;
			return true;
		}
		bool ParseNumber(Value &out)
		{
			const char *const start = p_;
			while (p_ < end_ && (strchr("+-0123456789.eE", *p_) != nullptr))
				++p_;
			if (p_ == start)
				return false;
			out.type = Value::Type::Number;
			out.number = strtod(std::string(start, p_).c_str(), nullptr);
			return true;
		}
		static void AppendUtf8(std::string &out, uint32_t code)
		{
			if (code < 0x80)
				out += static_cast<char>(code);
			else if (code < 0x800)
			{
				out += static_cast<char>(0xC0 | (code >> 6));
				out += static_cast<char>(0x80 | (code & 0x3F));
			}
			else if (code < 0x10000)
			{
				out += static_cast<char>(0xE0 | (code >> 12));
				out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (code & 0x3F));
			}
			else
			{
				out += static_cast<char>(0xF0 | (code >> 18));
				out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
				out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (code & 0x3F));
			}
		}
		bool Hex4(uint32_t &out)
		{
			if (end_ - p_ < 4)
				return false;
			out = 0;
			for (int i = 0; i < 4; ++i, ++p_)
			{
				const char c = *p_;
				out <<= 4;
				if (c >= '0' && c <= '9') out |= static_cast<uint32_t>(c - '0');
				else if (c >= 'a' && c <= 'f') out |= static_cast<uint32_t>(c - 'a' + 10);
				else if (c >= 'A' && c <= 'F') out |= static_cast<uint32_t>(c - 'A' + 10);
				else return false;
			}
			return true;
		}
		bool ParseString(std::string &out)
		{
			++p_;   // opening quote
			out.clear();
			while (p_ < end_)
			{
				const char c = *p_++;
				if (c == '"')
					return true;
				if (c != '\\')
				{
					out += c;
					continue;
				}
				if (p_ >= end_)
					return false;
				const char e = *p_++;
				switch (e)
				{
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u':
				{
					uint32_t code = 0;
					if (!Hex4(code))
						return false;
					if (code >= 0xD800 && code <= 0xDBFF && end_ - p_ >= 6 && p_[0] == '\\' && p_[1] == 'u')
					{
						p_ += 2;
						uint32_t low = 0;
						if (!Hex4(low))
							return false;
						code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
					}
					AppendUtf8(out, code);
					break;
				}
				default:
					return false;
				}
			}
			return false;
		}
		bool ParseArray(Value &out, int depth)
		{
			++p_;
			out.type = Value::Type::Array;
			SkipSpace();
			if (p_ < end_ && *p_ == ']')
				return ++p_, true;
			while (true)
			{
				Value item;
				if (!Parse(item, depth + 1))
					return false;
				out.array.push_back(std::move(item));
				SkipSpace();
				if (p_ >= end_)
					return false;
				if (*p_ == ',') { ++p_; continue; }
				if (*p_ == ']') { ++p_; return true; }
				return false;
			}
		}
		bool ParseObject(Value &out, int depth)
		{
			++p_;
			out.type = Value::Type::Object;
			SkipSpace();
			if (p_ < end_ && *p_ == '}')
				return ++p_, true;
			while (true)
			{
				SkipSpace();
				if (p_ >= end_ || *p_ != '"')
					return false;
				std::string key;
				if (!ParseString(key))
					return false;
				SkipSpace();
				if (p_ >= end_ || *p_ != ':')
					return false;
				++p_;
				Value value;
				if (!Parse(value, depth + 1))
					return false;
				out.object.emplace_back(std::move(key), std::move(value));
				SkipSpace();
				if (p_ >= end_)
					return false;
				if (*p_ == ',') { ++p_; continue; }
				if (*p_ == '}') { ++p_; return true; }
				return false;
			}
		}

		const char *p_;
		const char *end_;
	};

	inline bool Parse(const std::string &text, Value &out)
	{
		Parser parser(text.data(), text.data() + text.size());
		return parser.Parse(out);
	}

	inline void AppendString(std::string &out, const std::string &text)
	{
		out += '"';
		for (const char c : text)
		{
			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (static_cast<unsigned char>(c) < 0x20)
				{
					static const char hex[] = "0123456789abcdef";
					out += "\\u00";
					out += hex[(c >> 4) & 0xF];
					out += hex[c & 0xF];
				}
				else
					out += c;
			}
		}
		out += '"';
	}
}
