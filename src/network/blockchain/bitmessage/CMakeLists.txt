# Copyright (c) 2010-2021 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_library(
  opentxs-network-blockchain-bitmessage OBJECT
  "${opentxs_SOURCE_DIR}/src/internal/network/blockchain/bitmessage/Factory.hpp"
  "Client.cpp"
  "Client.hpp"
  "PeerManager.cpp"
  "PeerManager.hpp"
)
set(cxx-install-headers
    "${opentxs_SOURCE_DIR}/include/opentxs/network/blockchain/bitmessage/Client.hpp"
)
target_link_libraries(
  opentxs-network-blockchain-bitmessage PRIVATE opentxs::messages
)
target_sources(
  opentxs-network-blockchain-bitmessage PRIVATE ${cxx-install-headers}
)
target_sources(
  opentxs PRIVATE $<TARGET_OBJECTS:opentxs-network-blockchain-bitmessage>
)
add_dependencies(opentxs-network-blockchain-bitmessage generated_code)

if(OPENTXS_STANDALONE)
  install(
    FILES ${cxx-install-headers}
    DESTINATION include/opentxs/network/blockchain/bitmessage
    COMPONENT dev
  )
endif()
