# Copyright (c) 2010-2021 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_library(opentxs-otx-client-workflow OBJECT "Common.cpp")
set(cxx-install-headers
    "${opentxs_SOURCE_DIR}/include/opentxs/otx/client/workflow/Base.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/otx/client/workflow/Cash.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/otx/client/workflow/Cheque.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/otx/client/workflow/Invoice.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/otx/client/workflow/State.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/otx/client/workflow/Transfer.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/otx/client/workflow/Type.hpp"
    "${opentxs_SOURCE_DIR}/include/opentxs/otx/client/workflow/Types.hpp"
)
target_link_libraries(
  opentxs-otx-client-workflow PRIVATE Boost::headers opentxs::messages
)
target_link_libraries(opentxs PUBLIC Boost::system)
target_include_directories(
  opentxs-otx-client-workflow SYSTEM PRIVATE "${opentxs_SOURCE_DIR}/deps/"
)
target_sources(opentxs-otx-client-workflow PRIVATE ${cxx-install-headers})
target_sources(opentxs PRIVATE $<TARGET_OBJECTS:opentxs-otx-client-workflow>)
add_dependencies(opentxs-otx-client-workflow generated_code)

if(OPENTXS_STANDALONE)
  install(
    FILES ${cxx-install-headers}
    DESTINATION include/opentxs/otx/client/workflow
    COMPONENT dev
  )
endif()
