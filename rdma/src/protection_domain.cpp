#include "rdma/protection_domain.h"

#include <utility>

namespace rdma {

ProtectionDomain::ProtectionDomain(std::shared_ptr<Context> context)
    : context_(context == nullptr ? MakeDefaultContext() : std::move(context)) {}

const Context& ProtectionDomain::context() const { return *context_; }

std::shared_ptr<Context> ProtectionDomain::context_handle() const {
  return context_;
}

}  // namespace rdma
