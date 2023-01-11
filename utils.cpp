#include "utils.h"
#include <regex>
using namespace utils;

std::string formatJsonString (std:: string str) {
    std::regex pattern("([^{])\\s*:");
    std::string replacement("$1 : ");
    std::string formattedStr = std::regex_replace(str, pattern, replacement);

    return formattedStr;
}
