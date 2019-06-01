#pragma once

#include "envoy/upstream/retry.h"
#include "envoy/upstream/upstream.h"

namespace Envoy {
  class CanaryHostOmittingRetryPredicate : public Upstream::RetryHostPredicate {
  public:
    CanaryHostOmittingRetryPredicate(uint32_t) {}

    // if the host is a canary host, try somewhere else
    bool shouldSelectAnotherHost(const Upstream::Host& candidate_host) override {
      return candidate_host.canary();
    }

    void onHostAttempted(Upstream::HostDescriptionConstSharedPtr) override {}
  };
} // namespace Envoy

