#ifndef KEY_HANDLERS_H
#define KEY_HANDLERS_H

#include <string>

// Header for input event dispatching across app states
inline int getNextWordPos(const std::string& str, int pos) {
    int n = str.size();
    if (pos >= n) return n;
    int i = pos;
    while (i < n && (std::isalnum((unsigned char)str[i]) || str[i] == '_')) i++;
    while (i < n && std::isspace((unsigned char)str[i])) i++;
    return i;
}

inline int getPrevWordPos(const std::string& str, int pos) {
    if (pos <= 0) return 0;
    int i = pos - 1;
    while (i > 0 && std::isspace((unsigned char)str[i])) i--;
    while (i > 0 && (std::isalnum((unsigned char)str[i - 1]) || str[i - 1] == '_')) i--;
    return i;
}

#endif // KEY_HANDLERS_H
