#include <filesystem>
#include <fstream>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>

#include "lexer.hpp"
#include "preprocessor.hpp"
#include "util/token.hpp"
#include "util/toolbox.hpp"

// break apart a string into its keywords from spaces
void breakKeywords(const std::string& line, std::vector<std::string>& kwds) {
    // create stringstream from line
    std::stringstream sstream(line);

    // split on spaces
    std::string buf;
    while (std::getline(sstream, buf, ' ')) {
        if (buf.size() == 0) continue; // skip empty buffers
        kwds.push_back(buf); // append argument
    }
}

bool preprocessLine(std::string line, macrodef_map& macroMap, std::vector<Token>& tokens, cwd_stack& cwdStack) {
    // trim the line
    trimString(line);

    // if the line doesn't start with a macro, return false
    if (line.size() == 0 || line[0] != '#') return false;

    // extract kwds
    std::vector<std::string> kwds;
    breakKeywords(line, kwds);

    const std::string macroType = kwds[0].substr(1);

    if (macroType == "define") { // define something
        if (kwds.size() < 3)
            throw std::runtime_error("Malformed macro define.");

        std::string defStr;
        for (size_t i = 2; i < kwds.size(); i++) {
            if (i != 2) defStr += ' ';
            defStr += kwds[i];
        }

        macroMap[kwds[1]] = defStr;
        return true;
    } else if (macroType == "include") { // include something
        if (kwds.size() != 2) throw std::runtime_error("Malformed macro include.");

        // get the desired file
        bool isLocalInclude = kwds[1][0] == '"';
        const std::string& inPath = kwds[1].substr(1, kwds[1].size()-2);

        if (isLocalInclude) {
            // load the file, tokenize
            std::filesystem::path inPathAbs = cwdStack.top() / std::filesystem::path(inPath);
            std::ifstream inHandle(inPathAbs);

            if (!inHandle.is_open())
                throw std::runtime_error("Failed to open included file: " + inPath);

            // set CWD
            cwdStack.push( std::filesystem::absolute(inPathAbs).parent_path() );

            // tokenize
            tokenize(inHandle, tokens, cwdStack);

            // close file
            cwdStack.pop();
            inHandle.close();
        } else {
            throw std::runtime_error("TODO: implement stdlib.");
        }
        return true;
    }

    // base case, return false since it hasn't run
    return false;
}

// replace all macro-defined matches
void replaceMacrodefs(std::string& line, macrodef_map& macroMap) {
    for (auto [oldStr, newStr] : macroMap) {
        size_t i = 0;

        while ((i = line.find(oldStr, i)) != std::string::npos) {
            // verify this is its own word
            size_t end = i + oldStr.size();
            if ((i == 0 || !isCharValidIdentifier(line[i-1])) &&
                (end == line.size() || std::isspace(line[end]) || !isCharValidIdentifier(line[end]))) {
                line.erase(line.begin() + i, line.begin() + end);
                line.insert(i, newStr);
                i = end;
            }
            ++i;
        }
    }
}