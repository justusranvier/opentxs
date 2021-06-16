# Copyright (c) 2010-2021 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_library(
  opentxs-blockchain-node-base OBJECT
  "Base.cpp"
  "Base.hpp"
  "SyncCache.cpp"
  "SyncCache.hpp"
  "SyncClient.cpp"
  "SyncClient.hpp"
  "SyncPipeline.cpp"
  "SyncPipeline.hpp"
  "SyncServer.hpp"
  "SyncTask.cpp"
  "SyncTask.hpp"
)
target_link_libraries(
  opentxs-blockchain-node-base
  PRIVATE
    "${OT_ZMQ_TARGET}"
    Boost::headers
    opentxs::messages
)
target_link_libraries(opentxs PUBLIC "${OT_ZMQ_TARGET}")
target_include_directories(
  opentxs-blockchain-node-base SYSTEM PRIVATE "${opentxs_SOURCE_DIR}/deps/"
)
target_sources(opentxs-blockchain-node-base PRIVATE ${cxx-install-headers})
target_sources(opentxs PRIVATE $<TARGET_OBJECTS:opentxs-blockchain-node-base>)
add_dependencies(opentxs-blockchain-node-base generated_code)
