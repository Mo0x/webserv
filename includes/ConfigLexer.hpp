#ifndef CONFIG_LEXER_HPP
#define CONFIG_LEXER_HPP

#include <string>
#include <vector>

enum TokenType
{
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_KEYWORD,
    TOKEN_SYMBOL,    // chars bizarre '{', '}', ';'
    TOKEN_END_OF_FILE
};

struct Token
{
    TokenType type;
    std::string value;
    int line; // pour report errors
};

class ConfigLexer
{
public:
    ConfigLexer(const std::string& filename);
    const std::vector<Token>& tokenize();
    
private:
    std::string readFile(const std::string& filename);
    void lex();
    
    std::string _source;
    std::vector<Token> _tokens;
    size_t _current;
    int _line;
};

#endif
