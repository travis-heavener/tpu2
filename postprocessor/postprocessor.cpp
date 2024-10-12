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
void readNextLine(const post_process_opts&, std::string&, std::string&, std::ifstream&);
void writeInstruction(const post_process_opts&, std::ofstream&, const std::string&, const std::string&);
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

    // specify paths
    const std::string inPath(argv[1]);
    std::string outPath;

    // grab any extra args
    bool forceOverwrite = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "-f") {
            if (outPath.size() > 0) {
                std::cerr << "Error: Output path specified more than once.\n";
                return 1;
            }
            forceOverwrite = true;
            outPath = inPath;
        } else if (arg == "-minify" || arg == "--m") {
            opts.minify = true;
        } else if (arg == "-strip-comments" || arg == "--sc") {
            opts.stripComments = true;
        } else if (arg == "-o") {
            if (i+1 == argc) {
                std::cerr << "Error: Invalid usage, output file must be specified after \"-o\" flag.\n";
                return 1;
            } else if (outPath.size() > 0) {
                std::cerr << "Error: Output path specified more than once.\n";
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
    if (inPath == outPath) {
        if (!forceOverwrite) {
            std::cerr << "Error: Cannot use input file as output file (-f arg to bypass)\n";
            return 1;
        } else {
            // use temp file
            outPath += "_tmp";
        }
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
    std::string line, strippedLine;

    // read first line
    readNextLine(opts, line, strippedLine, inHandle);
    while (line != "") {
        // handle arguments & such
        if ((opts.mergeImm8Pushes || opts.reducePushPopsToRegs) && strippedLine.find("push ") == 0) {
            // get next line and check for another push to combine with
            std::string lineBuf, strippedLineBuf;
            readNextLine(opts, lineBuf, strippedLineBuf, inHandle);

            if (opts.mergeImm8Pushes && strippedLineBuf.find("push ") == 0) { // can be combined
                // extract values
                try {
                    int valA = std::stoi( strippedLine.substr(5) );
                    int valB = std::stoi( strippedLineBuf.substr(5) );

                    std::string newInst = "pushw " + std::to_string( ((valB << 8) | valA) & 0xFFFF );
                    writeInstruction(opts, outHandle, newInst, newInst);
                } catch (std::invalid_argument& e) {
                    // not pushing imm8s, handle as separate instructions
                    writeInstruction(opts, outHandle, line, strippedLine);
                    writeInstruction(opts, outHandle, lineBuf, strippedLineBuf);
                    line = lineBuf;
                    strippedLine = strippedLineBuf;
                }
            } else if (opts.reducePushPopsToRegs && strippedLine.find("pop ") == 0) {
                // move the value between registers
                std::string regA = strippedLine.substr(5);
                std::string regB = strippedLineBuf.substr(4);

                // write instruction
                std::string newInst = "mov " + regB + ", " + regA;
                writeInstruction(opts, outHandle, newInst, newInst);
            } else {
                // base case, current instruction not matched, so write that and the next one
                writeInstruction(opts, outHandle, line, strippedLine);
                writeInstruction(opts, outHandle, lineBuf, strippedLineBuf);
            }
        } else if (opts.reducePushPopsToRegs && strippedLine.find("pushw ") == 0) {
            // get next line and check for a popw to combine with
            std::string lineBuf, strippedLineBuf;
            readNextLine(opts, lineBuf, strippedLineBuf, inHandle);

            if (strippedLineBuf.find("popw ") == 0) {
                // move the value between registers
                std::string regA = strippedLine.substr(6);
                std::string regB = strippedLineBuf.substr(5);

                // skip default
                std::string newInst = "movw " + regB + ", " + regA;
                writeInstruction(opts, outHandle, newInst, newInst);
            } else {
                // base case, current instruction not matched, so write that and the next one
                writeInstruction(opts, outHandle, line, strippedLine);
                writeInstruction(opts, outHandle, lineBuf, strippedLineBuf);
                line = lineBuf;
                strippedLine = strippedLineBuf;
            }
        } else if (opts.dissolvePops && (strippedLine == "popw" || strippedLine == "pop")) {
            // fetch any successive pop/popw to combine
            size_t popSize = strippedLine == "popw" ? 2 : 1;

            std::string lineBuf, strippedLineBuf;
            readNextLine(opts, lineBuf, strippedLineBuf, inHandle);

            while (strippedLineBuf == "popw" || strippedLineBuf == "pop") {
                popSize += strippedLineBuf == "popw" ? 2 : 1;
                readNextLine(opts, lineBuf, strippedLineBuf, inHandle);
            }

            // write SP substraction instruction
            const std::string subInst = "sub SP, " + std::to_string(popSize) + '\n';
            writeInstruction(opts, outHandle, subInst, subInst);

            // stopped on a non-matching line, so write that one
            writeInstruction(opts, outHandle, lineBuf, strippedLineBuf);
        } else {
            // write instruction as usual
            writeInstruction(opts, outHandle, line, strippedLine);
        }

        // get next instruction
        readNextLine(opts, line, strippedLine, inHandle);
    }

    // close file handles & overwrite temp .tpu file
    inHandle.close();
    outHandle.close();

    // if force overwriting, overwrite
    if (forceOverwrite) {
        std::filesystem::remove(inPath);
        std::filesystem::rename(outPath, inPath);
    }
    return 0;
}

/*************************************************************/
/*                          TOOLBOX                          */
/*************************************************************/

void readNextLine(const post_process_opts& opts, std::string& buffer, std::string& strippedBuffer, std::ifstream& inHandle) {
    buffer.clear(); // clear buffer

    std::string line, strippedLine;
    while (std::getline(inHandle, line)) {
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

        // update buffer & escape
        buffer = line;
        strippedBuffer = strippedLine;
        return;
    }
}

void writeInstruction(const post_process_opts& opts, std::ofstream& outHandle, const std::string& line, const std::string& strippedLine) {
    if (opts.minify) { // remove any leading whitespace
        outHandle << line << '\n';
    } else { // don't minify
        // don't indent labels inside user functions
        bool isLabelStart = strippedLine.back() == ':';
        if (isLabelStart && strippedLine[strippedLine.size()-2] != 'E' &&
            (strippedLine.find("__UF") == 0 || strippedLine.find("main") == 0)) {
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