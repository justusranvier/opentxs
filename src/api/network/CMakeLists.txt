# Copyright (c) 2010-2021 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_subdirectory(blockchain)

add_library(
  opentxs-api-network OBJECT
  "${opentxs_SOURCE_DIR}/src/internal/api/network/Factory.hpp"
  "${opentxs_SOURCE_DIR}/src/internal/api/network/Network.hpp"
  "Asio.cpp"
  "Asio.hpp"
  "Bitmessage.cpp"
  "Bitmessage.hpp"
  "Blockchain.cpp"
  "Blockchain.hpp"
  "Dht.cpp"
  "Dht.hpp"
  "Network.cpp"
  "Network.hpp"
  "ZAP.cpp"
  "ZAP.hpp"
  "ZMQ.cpp"
  "ZMQ.hpp"
)
set(cxx-install-headers
    "${opentxs_SOURCE_DIR}/include/opentxs/api/network/Asio.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/api/network/Blockchain.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/api/network/Dht.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/api/network/Network.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/api/network/ZAP.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/api/network/ZMQ.hpp"
)
target_link_libraries(
  opentxs-api-network PRIVATE opentxs::messages Boost::headers
)
target_link_libraries(opentxs PUBLIC Boost::system Boost::thread)
target_include_directories(
  opentxs-api-network SYSTEM PRIVATE "${opentxs_SOURCE_DIR}/deps/"
)

if(NOT OT_BLOCKCHAIN_EXPORT)
  target_sources(opentxs-api-network PRIVATE "bitmessage/Null.cpp")
endif()

target_sources(opentxs-api-network PRIVATE ${cxx-install-headers})
target_sources(opentxs PRIVATE $<TARGET_OBJECTS:opentxs-api-network>)
add_dependencies(opentxs-api-network generated_code)

if(OPENTXS_STANDALONE)
  install(
    FILES ${cxx-install-headers}
    DESTINATION include/opentxs/api/network
    COMPONENT dev
  )
endif()
