#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace worker::model {

class Tensor {
 public:
  Tensor() = default;
  Tensor(std::string name, std::vector<std::size_t> shape);

  void FillRamp(float start, float step);
  std::size_t ElementCount() const;
  std::size_t ByteSize() const;
  std::vector<std::byte> Serialize() const;
  std::string Summary() const;

 private:
  std::string name_;
  std::vector<std::size_t> shape_;
  std::vector<float> data_;
};

}  // namespace worker::model
