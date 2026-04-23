#pragma once

#include <memory>

#include "rdma/context.h"

namespace rdma {

class ProtectionDomain {
 public:
  explicit ProtectionDomain(std::shared_ptr<Context> context);

  const Context& context() const;
  std::shared_ptr<Context> context_handle() const;

 private:
  std::shared_ptr<Context> context_;
};

}  // namespace rdma
