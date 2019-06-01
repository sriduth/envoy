#include "extensions/retry/host/canary_host_omitting/config.h"

#include "envoy/registry/registry.h"
#include "envoy/upstream/retry.h"

namespace Envoy {
  namespace Extensions {
    namespace Retry {
      namespace Host {

	REGISTER_FACTORY(CanaryHostOmittingRetryPredicateFactory, Upstream::RetryHostPredicateFactory);

      }
    } // namespace Retry
  } // namespace Extensions
} // namespace Envoy
