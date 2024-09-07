#include <string>
#include <sys/stat.h>

#include "toolbox.hpp"

// modified from https://stackoverflow.com/a/6296808
bool doesFileExist(const std::string& file) {
    struct stat buf;
    return stat(file.c_str(), &buf) != -1;
}