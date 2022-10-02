#pragma once
#include <string>
#include <vector>

std::string readShaderFile(const char *fileName);
void printShaderSource(const char *text);
int endsWith(const char *s, const char *part);

template <typename T>
inline void mergeVectors(std::vector<T> &v1, const std::vector<T> &v2)
{
    v1.insert(v1.end(), v2.begin(), v2.end());
}