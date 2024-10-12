#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <string>

#define TAB "    "

typedef struct post_process_opts {
    // combines any consecutive imm8 push operations into imm16 pushw operations
    bool mergeImm8Pushes        = true; // default: true

    // combines any push/pop operations between registers to mov/movw instructions
    bool reducePushPopsToRegs   = true; // default: true

    // combines any consecutive target-less pop/popw instructions to just subtracting from the SP
    bool dissolvePops           = true; // default: true

    // removes any comments from the input file
    bool stripComments          = false; // default: false

    // removes any unnecessary whitespace (ex. leading tabs)
    bool minify                 = false; // default: false
} post_process_opts;

// fwd declarations
void write(post_process_opts&, std::ofstream&, const std::string&, const std::string&);
void ltrimString(std::string&);
void rtrimString(std::string&);
void stripComments(std::string&);

// a postprocessor for reducing the number of instructions of a given TPU file
int main(int argc, char* argv[]) {
    // grab cmd args
    if (argc < 2) {
        std::cerr << "Invalid usage: ./postproc <in.tpu> <optional: args>\n";
        return 1;
    }

    // create post opts
    post_process_opts opts;

    // grab any extra args
    std::string outPath;
    for (int i = 2; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "-minify" || arg == "--m") {
            opts.minify = true;
        } else if (arg == "-strip-comments" || arg == "--sc") {
            opts.stripComments = true;
        } else if (arg == "-o") {
            if (i+1 == argc) {
                std::cerr << "Error: Invalid usage, output file must be specified after \"-o\" flag.\n";
                return 1;
            }

            // grab output file
            outPath = std::string(argv[++i]);
        } else {
            std::cout << "Warning: Skipping invalid argument: " << arg << '\n';
        }
    }

    // verify output file was set
    if (outPath.size() == 0) {
        std::cerr << "Error: Missing output path (usage: \"-o <out.tpu>\")\n";
        return 1;
    }

    // grab input file
    const std::string inPath(argv[1]);
    if (inPath == outPath) {
        std::cerr << "Error: Cannot use input file as output file\n";
        return 1;
    }

    // open input file
    std::ifstream inHandle(inPath);
    if (!inHandle.is_open()) {
        std::cerr << "Failed to open input file: " << inPath << '\n';
        return 1;
    }

    // open output file
    std::ofstream outHandle(outPath);
    if (!outHandle.is_open()) {
        inHandle.close();
        std::cerr << "Failed to open output file: " << outPath << '\n';
        return 1;
    }

    // read the current and last instructions into memory (break/reset on labels)
    std::vector<std::pair<std::string, std::string>> cachedInsts;
    std::string line, strippedLine;
    long long lineNum = -1;

    while (std::getline(inHandle, line)) {
        ++lineNum;

        // strip whitespace and comments
        ltrimString(line); // strip whitespace
        strippedLine = line;
        if (line.size() == 0) continue; // skip blank lines
        
        // strip comments for temp line (used to match args)
        stripComments(strippedLine);
        rtrimString(strippedLine);

        // strip comments from actual line if needed
        if (opts.stripComments) stripComments(line);
        if (line.size() == 0) continue; // skip blank lines

        // check for any chained arguments
        if (cachedInsts.size() > 0) {
            // handle any consecutive pushes
            if (cachedInsts[0].second.find("push ") == 0) {
                if (strippedLine.find("push ") == 0) { // combine the two
                    // extract values
                    int valA, valB;
                    try {
                        valA = std::stoi( cachedInsts[0].second.substr(5) );
                        valB = std::stoi( strippedLine.substr(5) );
                    } catch (std::invalid_argument& e) {
                        std::cerr << "Invalid argument on line " << lineNum << '\n';
                        inHandle.close();
                        outHandle.close();
                        std::filesystem::remove(outPath);
                        return 1;
                    }

                    std::string newInst = "pushw " + std::to_string( ((valB << 8) | valA) & 0xFFFF );

                    // skip default
                    cachedInsts.clear();
                    write(opts, outHandle, newInst, newInst);
                    continue;
                } else {
                    // just write the instruction
                    write(opts, outHandle, cachedInsts[0].first, cachedInsts[0].second);
                }
            }

            // clear regardless
            cachedInsts.clear();
        } else {
            // handle arguments & such (storing lines in cachedInsts if needed)
            if (strippedLine.find("push ") == 0) {
                cachedInsts.push_back({line, strippedLine}); // record cached push instruction
            }
        }

        // just write the instruction to the file
        if (cachedInsts.size() == 0) {
            write(opts, outHandle, line, strippedLine);
        }
    }

    // close file handles & overwrite temp .tpu file
    inHandle.close();
    outHandle.close();
    return 0;
}

/*************************************************************/
/*                          TOOLBOX                          */
/*************************************************************/

void write(post_process_opts& opts, std::ofstream& outHandle, const std::string& line, const std::string& strippedLine) {
    if (opts.minify) { // remove any leading whitespace
        outHandle << line << '\n';
    } else { // don't minify
        bool isLabelStart = strippedLine.back() == ':';
        if (isLabelStart) {
            outHandle << line << '\n'; // don't indent
        } else {
            outHandle << TAB << line << '\n'; // indent
        }
    }
}

// helper for trimming strings in place
void ltrimString(std::string& str) {
    if (str.size() == 0) return; // skip blank lines

    while (str.length() > 0 && std::isspace(str[0])) {
        str.erase(str.begin());
    }
}

void rtrimString(std::string& str) {
    if (str.size() == 0) return; // skip blank lines

    while (str.length() > 0 && std::isspace(str.back())) {
        str.pop_back();
    }
}

// strips comments from a given line
void stripComments(std::string& line) {
    if (line.size() == 0) return; // skip blank lines

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
    while (line.size() > len)
        line.pop_back();
}

/*************************************************************/
/*                          TOOLBOX                          */
/*************************************************************/