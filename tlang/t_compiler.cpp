#include <iostream>
#include <fstream>
#include <vector>

#include "toolbox.hpp"
#include "t_exception.hpp"
#include "lexer.hpp"

/**
 * Compiles .t files to .tpu assembly files.
 */

int main(int argc, char* argv[]) {
    // format: <executable> <input.t>
    if (argc != 2) {
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
        std::cerr << "Output file already exists: " << outPath << std::endl;
        inHandle.close();
        exit(1);
    }

    // open output file
    std::ofstream outHandle(outPath);
    if (!outHandle.is_open()) {
        std::cerr << "Failed to open output file: " << outPath << std::endl;
        inHandle.close();
        exit(1);
    }

    /********* compilation starts below *********/

    try {
        // 1. tokenize file
        std::vector<Token> tokens;
        tokenize(inHandle, tokens);
        inHandle.close();

        for (Token t : tokens)
            std::cout << t.raw << ' ';
        std::cout << '\n';

        // 2. parse to AST
        
        // 3. semantic analysis

        // 4. translate AST to TPU assembly code

        // close files
        outHandle.close();
    } catch (TException& e) {
        std::cerr << e.toString() << '\n';

        // close files
        inHandle.close();
        outHandle.close();

        // remove output file
        std::remove(outPath.c_str());
    }

    return 0;
}