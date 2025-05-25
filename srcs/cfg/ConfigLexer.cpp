#include "ConfigLexer.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <iostream>

ConfigLexer::ConfigLexer(const std::string& filename)
    : _current(0), _line(1)
{
    _source = readFile(filename);
    lex();
}

std::string ConfigLexer::readFile(const std::string& filename)
{
    std::ifstream file(filename.c_str());
    if (!file) {
        throw std::runtime_error("Unable to open configuration file: " + filename);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

const std::vector<Token>& ConfigLexer::tokenize()
{
    return _tokens;
}

void ConfigLexer::lex()
{
    while (_current < _source.length())
    {
        char c = _source[_current];

        if (std::isspace(c))
        {
            if (c == '\n') _line++;
            _current++;
            continue;
        }

        // SYMBOLS: { } ;
        if (c == '{' || c == '}' || c == ';')
        {
            Token token;
            token.type = TOKEN_SYMBOL;
            token.value = c;
            token.line = _line;
            _tokens.push_back(token);
            _current++;
            continue;
        }

        // NUMBERS
        if (std::isdigit(c))
        {
            std::string number;
            while (_current < _source.length() && std::isdigit(_source[_current]))
            {
                number.push_back(_source[_current]);
                _current++;
            }
            Token token;
            token.type = TOKEN_NUMBER;
            token.value = number;
            token.line = _line;
            _tokens.push_back(token);
            continue;
        }

        // IDENTIFIERS or PATHS (stop before semicolon if needed)
        if (std::isalpha(c) || c == '_' || c == '/' || c == '.')
        {
            std::string identifier;
            while (_current < _source.length() &&
                   (std::isalnum(_source[_current]) ||
                    _source[_current] == '_' ||
                    _source[_current] == '-' ||
                    _source[_current] == '/' ||
                    _source[_current] == '.' ||
                    _source[_current] == ':'))
            {
                if (_source[_current] == ';') // stop at semicolon
                    break;
                identifier.push_back(_source[_current]);
                _current++;
            }

            Token token;
            token.type = TOKEN_IDENTIFIER;
            token.value = identifier;
            token.line = _line;
            _tokens.push_back(token);

            // If next char is a semicolon, push it as a separate token
            if (_current < _source.length() && _source[_current] == ';') {
                Token semi;
                semi.type = TOKEN_SYMBOL;
                semi.value = ";";
                semi.line = _line;
                _tokens.push_back(semi);
                _current++;
            }

            continue;
        }

        // Skip unknown characters (optional: you can throw if you want strict syntax)
        _current++;
    }

    Token eof;
    eof.type = TOKEN_END_OF_FILE;
    eof.value = "";
    eof.line = _line;
    _tokens.push_back(eof);
}
