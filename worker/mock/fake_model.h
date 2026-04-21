#pragma once

#include <string>

namespace worker {

class FakeModel {
 public:
  std::string infer(const std::string& prompt) const;
};

}  // namespace worker
