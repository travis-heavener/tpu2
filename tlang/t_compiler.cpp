#include <iostream>
#include <fstream>
#include <vector>

#include "toolbox.hpp"
#include "t_exception.hpp"
#include "lexer.hpp"
#include "parser.hpp"

/**
 * Compiles .t files to .tpu assembly files.
 */

int main(int argc, char* argv[]) {
    // format: <executable> <input.t>
    if (argc < 2) {
        std::cerr << "Invalid usage, expected: <executable> <input.t>" << std::endl;
        exit(1);
    }

    // check input file
    const std::string inPath(argv[1]);
    if (inPath.find(".t", inPath.size()-3) == std::string::npos) {
        std::cerr << "Input file must be a T file (.t extension)!" << std::endl;
        exit(1);
    }

    // verify input file exists
    const std::string outPath(inPath + "pu");
    std::ifstream inHandle(inPath);
    if (!inHandle.is_open()) {
        std::cerr << "File does not exist: " << inPath << std::endl;
        exit(1);
    }

    // verify output file doesn't exist
    if (doesFileExist(outPath)) {
        if (argc >= 3 && std::string(argv[2]) == "-f") { // remove by force
            std::remove(outPath.c_str());
        } else {
            std::cerr << "Output file already exists: " << outPath << std::endl;
            inHandle.close();
            exit(1);
        }
    }

    // open output file
    std::ofstream outHandle(outPath);
    if (!outHandle.is_open()) {
        std::cerr << "Failed to open output file: " << outPath << std::endl;
        inHandle.close();
        exit(1);
    }

    /********* compilation starts below *********/

    AST* pAST = nullptr;
    try {
        // 1. tokenize file
        std::vector<Token> tokens;
        tokenize(inHandle, tokens);
        inHandle.close();

        // 2. parse to AST & preliminary error checking
        pAST = parseToAST(tokens);

        // 3. semantic analysis (remaining syntax checking)

        // 4. translate AST to TPU assembly code

        // close files and free AST
        outHandle.close();
        delete pAST;
    } catch (TException& e) {
        std::cerr << e.toString() << '\n';

        // close files
        inHandle.close();
        outHandle.close();

        // free AST
        delete pAST;

        // remove output file
        std::remove(outPath.c_str());
    }

    return 0;
}