#pragma once
#include <memory>

namespace rex::system {
class KernelState;
}
namespace sdoj::xam {
class CommunityService;
}

namespace sdoj {
class CommunitySession {
 public:
  void Start();
  void Stop(rex::system::KernelState* kernel);

 private:
  std::shared_ptr<sdoj::xam::CommunityService> service_;
};
}  // namespace sdoj
