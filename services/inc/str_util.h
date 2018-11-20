/*
 * Copyright (c) 2018 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstring>
#include <cctype>

namespace particle {

inline char* toUpperCase(char* str, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        str[i] = std::toupper((unsigned char)str[i]);
    }
    return str;
}

inline char* toUpperCase(char* str) {
    return toUpperCase(str, std::strlen(str));
}

inline char* toLowerCase(char* str, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        str[i] = std::tolower((unsigned char)str[i]);
    }
    return str;
}

inline char* toLowerCase(char* str) {
    return toLowerCase(str, std::strlen(str));
}

inline bool isPrintable(const char* str, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (!std::isprint((unsigned char)str[i])) {
            return false;
        }
    }
    return true;
}

inline bool isPrintable(const char* str) {
    return isPrintable(str, std::strlen(str));
}

inline bool startsWith(const char* str, size_t strSize, const char* prefix, size_t prefixSize) {
    if (strSize < prefixSize) {
        return false;
    }
    if (strncmp(str, prefix, prefixSize) != 0) {
        return false;
    }
    return true;
}

inline bool startsWith(const char* str, const char* prefix) {
    return startsWith(str, strlen(str), prefix, strlen(prefix));
}

inline bool endsWith(const char* str, size_t strSize, const char* suffix, size_t suffixSize) {
    if (strSize < suffixSize) {
        return false;
    }

    if (strncmp(str + strSize - suffixSize, suffix, suffixSize) != 0) {
        return false;
    }

    return true;
}

inline bool endsWith(const char* str, const char* suffix) {
    return endsWith(str, strlen(str), suffix, strlen(suffix));
}

/**
 * Escapes a set of characters by prepending them with an escape character.
 *
 * @param str Source string.
 * @param strSize Size of the source string.
 * @param spec Characters that need to be escaped.
 * @param specSize Number of characters that need to be escaped.
 * @param esc Escape character.
 * @param dest Destination buffer.
 * @param destSize Size of the destination buffer.
 *
 * @return Destination buffer.
 */
char* escape(const char* src, size_t srcSize, const char* spec, size_t specSize, char esc, char* dest, size_t destSize);

/**
 * Escapes a set of characters by prepending them with an escape character.
 *
 * @param str Source string.
 * @param spec Characters that need to be escaped.
 * @param esc Escape character.
 * @param dest Destination buffer.
 * @param destSize Size of the destination buffer.
 *
 * @return Destination buffer.
 */
inline char* escape(const char* src, const char* spec, char esc, char* dest, size_t destSize) {
    return escape(src, strlen(src), spec, strlen(spec), esc, dest, destSize);
}

} // particle
