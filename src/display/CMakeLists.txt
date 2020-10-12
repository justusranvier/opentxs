# Copyright (c) 2010-2020 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(cxx-sources "Definition.cpp" "Scale.cpp")
set(cxx-install-headers
    "${opentxs_SOURCE_DIR}/include/opentxs/display/Types.hpp"
)
set(cxx-headers
    ${cxx-install-headers}
    "Definition.hpp"
    "Definition_imp.hpp"
    "Scale.hpp"
    "Scale_imp.hpp"
)

add_library(opentxs-display OBJECT ${cxx-sources} ${cxx-headers})
target_link_libraries(opentxs-display PRIVATE Boost::headers opentxs::messages)

if(OPENTXS_STANDALONE)
  install(
    FILES ${cxx-install-headers}
    DESTINATION include/opentxs/display
    COMPONENT dev
  )
endif()
