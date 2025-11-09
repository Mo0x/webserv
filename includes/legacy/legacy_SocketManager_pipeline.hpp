// LEGACY: retained for reference; not used in current pipeline.
// Original functions: SocketManager::locateHeaders, enforceHeaderLimits,
// parseAndValidateRequest, processFirstTimeHeaders, ensureBodyReady
// Source moved from: srcs/server/SocketManager.cpp
// Last moved: 2025-02-15
//
// To re-enable the legacy header/body processing pipeline, add
// srcs/legacy/legacy_SocketManager_pipeline.cpp to the build and make sure
// SocketManager.hpp exposes the method declarations (they remain in place).
// No additional includes are required beyond SocketManager.hpp.

#ifndef LEGACY_SOCKETMANAGER_PIPELINE_HPP
#define LEGACY_SOCKETMANAGER_PIPELINE_HPP

#include "SocketManager.hpp"

#endif
