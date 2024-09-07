#include <fstream>
#include <vector>

#include "lexer.hpp"
#include "toolbox.hpp"
#include "t_exception.hpp"

// tokenize a document to a vector of Tokens
void tokenize(std::ifstream& inHandle, std::vector<Token>& tokens) {
    // continuously read in the entire document, adding tokens
    std::string line;

    while (std::getline(inHandle, line)) {
        // remove trailing \r if present (CRLF for Windows systems)
        if (*line.rbegin() == '\r') line.pop_back();

        // tokenize line
        tokenizeLine(line, tokens);
    }
}

// tokenize a particular line
void tokenizeLine(const std::string& line, std::vector<Token>& tokens, line_t lineNumber) {
    // iterate over all characters
    const size_t lineLen = line.size();
    std::string buffer; // mutable buffer cleared each iteration
    for (size_t i = 0; i < lineLen; i++) {
        if (std::isspace(line[i])) continue; // skip whitespace

        // reset buffer & prepare error info object
        buffer.clear();
        ErrInfo err(lineNumber, i);

        // handle the current character
        if (std::isdigit(line[i])) { // int/double literals (cannot start with a decimal/period!!)
            TokenType tokenType = TokenType::LIT_INT;
            buffer = line[i]; // update buffer
            while (++i < lineLen && (std::isdigit(line[i]) || line[i] == '.')) {
                if (line[i] == '.') tokenType = TokenType::LIT_DOUBLE; // set as double if encountering a decimal
                buffer.push_back(line[i]);
            }

            // rollback iterator if not at end
            if (i != lineLen) i--;

            // add the token
            tokens.push_back(Token(err, buffer, tokenType));
        } else if (line[i] == '\'') { // character literals
            // look for closing quote
            buffer = line[i];
            while (++i < lineLen) {
                buffer.push_back(line[i]); // add character
                if (line[i] == '\\') { // check for escape characters and skip next character
                    if (i+1 == lineLen) throw TInvalidEscapeException(err);
                    buffer.push_back(line[++i]); // add escaped character
                } else if (line[i] == '\'') { // break on closing quote
                    break;
                }
            }

            // if reached the end of the line, unclosed quote (don't rollback, breaks when hitting quote)
            if (i == lineLen) throw TUnclosedQuoteException(err);
            
            // otherwise, add the token
            tokens.push_back(Token(err, buffer, TokenType::LIT_CHAR));
        }
    }
}