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

// the index of a previously added texture file is returned
inline int addUnique(std::vector<std::string> &files, const std::string &file)
{
    if (file.empty())
        return -1;

    auto i = std::find(std::begin(files), std::end(files), file);

    if (i == files.end())
    {
        files.push_back(file);
        return (int)files.size() - 1;
    }

    return (int)std::distance(files.begin(), i);
}