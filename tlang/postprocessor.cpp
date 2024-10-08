#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "postprocessor.hpp"
#include "util/toolbox.hpp"

// strips comments from a given line
void stripComments(std::string& line) {
    size_t len = line.size();
    bool isInChar = false;
    for (size_t i = 0; i < line.size(); ++i) {
        if (isInChar) {
            if (line[i] == '\'') isInChar = false;
        } else if (line[i] == ';') {
            // break out here
            len = i;
            break;
        } else if (line[i] == '\'') {
            isInChar = true;
        }
    }

    // trim string
    while (line.size() >= len)
        line.pop_back();
}

// an optional postprocessor for reducing the number of instructions of a given TPU file
void postProcess(const std::string& inPath, const post_process_opts& opts) {
    // iterate over the entire file, writing to a temporary buffer file
    if (inPath.size() < 4 || inPath.substr(inPath.size()-4) != ".tpu") {
        std::cerr << "Failed to post process file: \"" << inPath << "\"\nFile must have .tpu extension!\n";
        return;
    }

    // determine tab sizing
    const std::string TAB = opts.minify ? "    " : "";

    // open input handle
    std::ifstream inHandle(inPath);

    // create buffer file
    std::string bufferPath = inPath + "_buf";
    std::ofstream bufferHandle(bufferPath);

    // read the current and last instructions into memory (break/reset on labels)
    std::vector<std::string> cachedInsts;
    std::string line, strippedLine;

    while (std::getline(inHandle, line)) {
        // strip whitespace and comments
        strippedLine = line;
        stripComments(strippedLine); // strip comments
        ltrimString(strippedLine); // strip whitespace

        // TODO: handle arguments & such (storing lines in cachedInsts if needed)

        // base case, just write the instruction to the file
        if (opts.stripComments) stripComments(line);
        if (opts.minify) { // remove any leading whitespace
            bufferHandle << line << '\n';
        } else { // don't minify
            bool isLabelStart = strippedLine.back() == ':';
            if (isLabelStart) {
                bufferHandle << line << '\n'; // don't indent
            } else {
                bufferHandle << TAB << line << '\n'; // indent
            }
        }

        // remove any cached instructions since they aren't vaild anymore
        cachedInsts.clear();
    }

    // close file handles & overwrite temp .tpu file
    inHandle.close();
    bufferHandle.close();

    std::filesystem::remove(inPath); // remove old
    std::filesystem::rename(bufferPath, inPath); // rename buffer to inPath
}