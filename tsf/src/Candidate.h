#pragma once
// 候选条目（引擎查出的原始数据，UTF-8）
#include <string>

namespace qy {

struct Candidate {
    std::string text; // UTF-8
    std::string code; // UTF-8
};

} // namespace qy
