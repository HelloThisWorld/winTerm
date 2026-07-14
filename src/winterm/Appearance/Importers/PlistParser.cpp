// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PlistParser.h"

#include <cmath>

using namespace winTerm::Appearance;

namespace
{
    constexpr size_t MaximumPlistNodes{ 4096 };

    class Parser final
    {
    public:
        explicit Parser(const std::string_view content) :
            _content{ content }
        {
            if (_content.size() > MaximumThemeFileSize)
            {
                throw std::runtime_error("The imported file exceeds the maximum allowed size.");
            }
            if (_content.starts_with("bplist00"))
            {
                throw std::runtime_error("Binary plist is not supported in this version. Export the theme as XML plist and try again.");
            }
            if (_content.starts_with("\xEF\xBB\xBF"))
            {
                _position = 3;
            }
        }

        Json::Value ParseDocument()
        {
            _skipMiscellaneous();
            auto root = _parseElement(0);
            _skipMiscellaneous();
            if (_position != _content.size())
            {
                throw std::runtime_error("The selected file contains unexpected XML content.");
            }
            return root;
        }

    private:
        struct Tag
        {
            std::string name;
            bool closing{ false };
            bool selfClosing{ false };
        };

        bool _startsWith(const std::string_view value) const noexcept
        {
            return _content.substr(_position).starts_with(value);
        }

        void _skipWhitespace() noexcept
        {
            while (_position < _content.size() && std::isspace(static_cast<unsigned char>(_content[_position])))
            {
                ++_position;
            }
        }

        void _skipMiscellaneous()
        {
            for (;;)
            {
                _skipWhitespace();
                if (_startsWith("<?"))
                {
                    const auto end = _content.find("?>", _position + 2);
                    if (end == std::string_view::npos)
                    {
                        throw std::runtime_error("The selected file contains an unterminated XML declaration.");
                    }
                    _position = end + 2;
                    continue;
                }
                if (_startsWith("<!--"))
                {
                    const auto end = _content.find("-->", _position + 4);
                    if (end == std::string_view::npos)
                    {
                        throw std::runtime_error("The selected file contains an unterminated XML comment.");
                    }
                    _position = end + 3;
                    continue;
                }
                if (_startsWith("<!DOCTYPE"))
                {
                    const auto end = _content.find('>', _position + 9);
                    if (end == std::string_view::npos)
                    {
                        throw std::runtime_error("The selected file contains an unterminated document type declaration.");
                    }
                    const auto declaration = _content.substr(_position, end - _position + 1);
                    if (declaration.find('[') != std::string_view::npos ||
                        declaration.find("ENTITY") != std::string_view::npos ||
                        declaration.find("plist") == std::string_view::npos)
                    {
                        throw std::runtime_error("XML document type definitions and entities are not supported.");
                    }
                    // The canonical Apple plist declaration is recognized but never resolved.
                    _position = end + 1;
                    continue;
                }
                if (_startsWith("<!"))
                {
                    throw std::runtime_error("XML document type definitions and entities are not supported.");
                }
                break;
            }
        }

        Tag _readTag()
        {
            if (_position >= _content.size() || _content[_position] != '<')
            {
                throw std::runtime_error("The selected file contains malformed XML.");
            }
            ++_position;
            Tag tag;
            if (_position < _content.size() && _content[_position] == '/')
            {
                tag.closing = true;
                ++_position;
            }

            const auto nameStart = _position;
            while (_position < _content.size())
            {
                const auto character = _content[_position];
                if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_' && character != '-' && character != ':' && character != '.')
                {
                    break;
                }
                ++_position;
            }
            if (_position == nameStart)
            {
                throw std::runtime_error("The selected file contains an invalid XML tag.");
            }
            tag.name = std::string{ _content.substr(nameStart, _position - nameStart) };

            char quote = 0;
            for (; _position < _content.size(); ++_position)
            {
                const auto character = _content[_position];
                if (quote)
                {
                    if (character == quote)
                    {
                        quote = 0;
                    }
                    continue;
                }
                if (character == '\'' || character == '"')
                {
                    quote = character;
                    continue;
                }
                if (character == '>')
                {
                    auto index = _position;
                    while (index > nameStart && std::isspace(static_cast<unsigned char>(_content[index - 1])))
                    {
                        --index;
                    }
                    tag.selfClosing = index > nameStart && _content[index - 1] == '/';
                    ++_position;
                    return tag;
                }
                if (character == '<')
                {
                    break;
                }
            }
            throw std::runtime_error("The selected file contains an unterminated XML tag.");
        }

        bool _tryConsumeClosingTag(const std::string_view expected)
        {
            _skipMiscellaneous();
            if (!_startsWith("</"))
            {
                return false;
            }
            const auto tag = _readTag();
            if (!tag.closing || tag.selfClosing || tag.name != expected)
            {
                throw std::runtime_error("The selected file contains mismatched XML tags.");
            }
            return true;
        }

        std::string _decodeText(const std::string_view raw) const
        {
            if (raw.size() > MaximumThemeStringLength)
            {
                throw std::runtime_error("The imported theme contains a string that is too long.");
            }
            std::string decoded;
            decoded.reserve(raw.size());
            for (size_t i = 0; i < raw.size(); ++i)
            {
                if (raw[i] != '&')
                {
                    decoded.push_back(raw[i]);
                    continue;
                }
                const auto end = raw.find(';', i + 1);
                if (end == std::string_view::npos)
                {
                    throw std::runtime_error("The selected file contains an invalid XML entity.");
                }
                const auto entity = raw.substr(i, end - i + 1);
                if (entity == "&amp;")
                    decoded.push_back('&');
                else if (entity == "&lt;")
                    decoded.push_back('<');
                else if (entity == "&gt;")
                    decoded.push_back('>');
                else if (entity == "&quot;")
                    decoded.push_back('"');
                else if (entity == "&apos;")
                    decoded.push_back('\'');
                else
                    throw std::runtime_error("XML entity expansion is not supported.");
                i = end;
            }
            return decoded;
        }

        std::string _readScalarText(const std::string_view tagName)
        {
            const auto start = _position;
            const auto end = _content.find('<', start);
            if (end == std::string_view::npos)
            {
                throw std::runtime_error("The selected file contains unterminated XML text.");
            }
            _position = end;
            auto result = _decodeText(_content.substr(start, end - start));
            if (!_tryConsumeClosingTag(tagName))
            {
                throw std::runtime_error("The selected file contains nested data where text was expected.");
            }
            return result;
        }

        Json::Value _parseElement(const size_t depth)
        {
            if (depth > MaximumThemeNestingDepth)
            {
                throw std::runtime_error("The imported theme exceeds the maximum nesting depth.");
            }
            if (++_nodes > MaximumPlistNodes)
            {
                throw std::runtime_error("The imported plist contains too many values.");
            }

            _skipMiscellaneous();
            const auto tag = _readTag();
            if (tag.closing)
            {
                throw std::runtime_error("The selected file contains an unexpected closing XML tag.");
            }
            if (tag.name == "plist")
            {
                if (tag.selfClosing)
                {
                    throw std::runtime_error("The selected plist is empty.");
                }
                auto result = _parseElement(depth + 1);
                if (!_tryConsumeClosingTag("plist"))
                {
                    throw std::runtime_error("The selected plist contains multiple root values.");
                }
                return result;
            }
            if (tag.name == "dict")
            {
                Json::Value object{ Json::objectValue };
                if (tag.selfClosing)
                {
                    return object;
                }
                while (!_tryConsumeClosingTag("dict"))
                {
                    _skipMiscellaneous();
                    const auto keyTag = _readTag();
                    if (keyTag.name != "key" || keyTag.closing || keyTag.selfClosing)
                    {
                        throw std::runtime_error("A plist dictionary contains a value without a key.");
                    }
                    const auto key = _readScalarText("key");
                    if (object.isMember(key))
                    {
                        throw std::runtime_error("A plist dictionary contains a duplicate key.");
                    }
                    object[key] = _parseElement(depth + 1);
                }
                return object;
            }
            if (tag.name == "array")
            {
                Json::Value array{ Json::arrayValue };
                if (tag.selfClosing)
                {
                    return array;
                }
                while (!_tryConsumeClosingTag("array"))
                {
                    array.append(_parseElement(depth + 1));
                }
                return array;
            }
            if (tag.name == "true" || tag.name == "false")
            {
                if (!tag.selfClosing && !_tryConsumeClosingTag(tag.name))
                {
                    throw std::runtime_error("The selected file contains an invalid plist boolean.");
                }
                return tag.name == "true";
            }
            if (tag.selfClosing)
            {
                if (tag.name == "string" || tag.name == "data")
                {
                    return "";
                }
                throw std::runtime_error("The selected file contains an invalid empty plist value.");
            }

            auto text = _readScalarText(tag.name);
            if (tag.name == "string" || tag.name == "key" || tag.name == "data" || tag.name == "date")
            {
                return text;
            }
            if (tag.name == "integer")
            {
                size_t consumed = 0;
                try
                {
                    const auto value = std::stoll(text, &consumed, 10);
                    if (consumed != text.size())
                    {
                        throw std::invalid_argument{ "trailing data" };
                    }
                    return Json::Value{ static_cast<Json::Int64>(value) };
                }
                catch (...)
                {
                    throw std::runtime_error("The selected file contains an invalid plist integer.");
                }
            }
            if (tag.name == "real")
            {
                size_t consumed = 0;
                try
                {
                    const auto value = std::stod(text, &consumed);
                    if (consumed != text.size() || !std::isfinite(value))
                    {
                        throw std::invalid_argument{ "invalid real" };
                    }
                    return value;
                }
                catch (...)
                {
                    throw std::runtime_error("The selected file contains an invalid plist real number.");
                }
            }
            throw std::runtime_error("This plist value type is not supported.");
        }

        std::string_view _content;
        size_t _position{ 0 };
        size_t _nodes{ 0 };
    };
}

Json::Value PlistParser::Parse(const std::string_view content)
{
    return Parser{ content }.ParseDocument();
}
