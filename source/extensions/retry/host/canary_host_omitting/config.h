#include "envoy/upstream/retry.h"

#include "extensions/retry/host/canary_host_omitting/omit_canary_hosts.h"
#include "extensions/retry/host/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace Retry {
namespace Host {

class CanaryHostOmittingRetryPredicateFactory : public Upstream::RetryHostPredicateFactory {
  
public:
  Upstream::RetryHostPredicateSharedPtr createHostPredicate(const Protobuf::Message&,
                                                            uint32_t retry_count) override {
    return std::make_shared<CanaryHostOmittingRetryPredicate>(retry_count);
  }

  std::string name() override { return RetryHostPredicateValues::get().CanaryHostOmittingPredicate; }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new Envoy::ProtobufWkt::Empty()};
  }
};

} // namespace Host
} // namespace Retry
} // namespace Extensions
} // namespace Envoy
