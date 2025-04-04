#include "ConfigLexer.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>

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
            continue ;
        }
        
        if (c == '{' || c == '}' || c == ';')
        {
            Token token;
            token.type = TOKEN_SYMBOL;
            token.value = c;
            token.line = _line;
            _tokens.push_back(token);
            _current++;
            continue ;
        }
        
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
            continue ;
        }
        
        if (std::isalpha(c) || c == '_' || c == '/') {
            std::string identifier;
            while (_current < _source.length() && (std::isalnum(_source[_current]) || _source[_current]=='_' || _source[_current]=='-' || _source[_current]=='/'))
            {
                identifier.push_back(_source[_current]);
                _current++;
            }
            Token token;
            // add plus tard si un identifiant est un keywork ? 
            token.type = TOKEN_IDENTIFIER;
            token.value = identifier;
            token.line = _line;
            _tokens.push_back(token);
            continue;
        }
        
        // si R on skip ou error unkown token ?
        _current++;
    }
    
    Token eof;
    eof.type = TOKEN_END_OF_FILE;
    eof.value = "";
    eof.line = _line;
    _tokens.push_back(eof);
}
