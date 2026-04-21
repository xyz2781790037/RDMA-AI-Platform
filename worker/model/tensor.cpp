#include "worker/model/tensor.h"

#include <cstring>
#include <numeric>
#include <sstream>

namespace worker::model {

Tensor::Tensor(std::string name, std::vector<std::size_t> shape)
    : name_(std::move(name)), shape_(std::move(shape)) {
  const auto elements = ElementCount();
  data_.resize(elements);
}

void Tensor::FillRamp(float start, float step) {
  for (std::size_t i = 0; i < data_.size(); ++i) {
    data_[i] = start + static_cast<float>(i) * step;
  }
}

std::size_t Tensor::ElementCount() const {
  if (shape_.empty()) {
    return 1U;
  }

  return std::accumulate(shape_.begin(), shape_.end(), std::size_t{1},
                         [](std::size_t lhs, std::size_t rhs) {
                           return lhs * rhs;
                         });
}

std::size_t Tensor::ByteSize() const { return data_.size() * sizeof(float); }

std::vector<std::byte> Tensor::Serialize() const {
  std::vector<std::byte> bytes(ByteSize());
  if (!data_.empty()) {
    std::memcpy(bytes.data(), data_.data(), bytes.size());
  }
  return bytes;
}

std::string Tensor::Summary() const {
  std::ostringstream stream;
  stream << name_ << "[";
  for (std::size_t i = 0; i < shape_.size(); ++i) {
    if (i > 0) {
      stream << "x";
    }
    stream << shape_[i];
  }
  stream << "] bytes=" << ByteSize();
  return stream.str();
}

}  // namespace worker::model
