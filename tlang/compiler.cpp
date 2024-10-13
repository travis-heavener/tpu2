#include <filesystem>
#include <iostream>
#include <fstream>
#include <stack>
#include <vector>

#include "assembler.hpp"
#include "preprocessor.hpp"
#include "lexer.hpp"
#include "parser/parser.hpp"
#include "util/config.hpp"
#include "util/t_exception.hpp"
#include "util/toolbox.hpp"

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
    const std::string inPath = std::filesystem::canonical(argv[1]).string();
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

    // extract any extra arguments
    bool forceOverwrite = false;
    bool skipPostprocessor = false;
    DELETE_UNUSED_VARIABLES = DELETE_UNUSED_FUNCTIONS = true;

    for (int i = 2; i < argc; ++i) {
        std::string arg( argv[i] );
        if (arg == "-f") {
            forceOverwrite = true;
        } else if (arg == "-skip-post") {
            skipPostprocessor = true;
        } else if (arg == "-keep-unused") {
            DELETE_UNUSED_VARIABLES = DELETE_UNUSED_FUNCTIONS = false;
        } else {
            std::cout << "Warning: Skipping invalid argument: " << arg << '\n';
        }
    }

    // verify output file doesn't exist
    if (doesFileExist(outPath)) {
        if (forceOverwrite) { // remove by force
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
        // 0. load cwd
        cwd_stack cwdStack;
        std::filesystem::path inPathAbs( inPath );
        cwdStack.push( std::filesystem::canonical(inPathAbs).parent_path() );

        // 1. tokenize file
        std::vector<Token> tokens;
        tokenize(inHandle, tokens, cwdStack, inPathAbs.filename().string());
        inHandle.close();

        // 2. parse to AST & syntax checking (semantic analysis)
        pAST = parseToAST(tokens);

        // 3. translate AST to TPU assembly code
        generateAssembly(*pAST, outHandle);

        // close files and free AST
        outHandle.close();
        delete pAST;

        // 4. run through post processor (optional) to reduce redundant expressions more easily than in the assembler
        if (!skipPostprocessor) {
            // grab CWD to find postproc symlink
            const std::string postprocPath = (std::filesystem::canonical("/proc/self/exe").parent_path() / "postproc").string();

            const std::string tempPath = outPath + "_tmp";
            const std::string cmd( '"' + postprocPath + "\" " + outPath + " -o " + tempPath );
            std::system(cmd.c_str());

            // remove original tpu file and replace with tmp file
            if (!std::filesystem::exists(tempPath)) {
                std::cerr << "Failed to invoke post processor.\n";
                std::filesystem::remove(outPath); // remove regardless
            } else {
                std::filesystem::remove(outPath); // remove old
                std::filesystem::rename(tempPath, outPath); // rename buffer to outPath
            }
        }
    } catch (TException& e) {
        std::cerr << e.toString() << '\n';

        // close files
        inHandle.close();
        outHandle.close();

        // free AST
        delete pAST;

        // remove output file
        std::filesystem::remove(outPath);
    }

    return 0;
}