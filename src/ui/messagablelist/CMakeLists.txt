# Copyright (c) 2010-2021 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_library(
  opentxs-ui-messagablelist OBJECT
  "MessagableList.cpp"
  "MessagableList.hpp"
  "MessagableListItem.cpp"
  "MessagableListItem.hpp"
)
set(cxx-install-headers
    "${opentxs_SOURCE_DIR}/include/opentxs/ui/MessagableList.hpp"
)
target_link_libraries(opentxs-ui-messagablelist PRIVATE opentxs::messages)

if(OT_WITH_QT)
  set(ot-qobject-install-headers
      "${opentxs_SOURCE_DIR}/include/opentxs/ui/qt/MessagableList.hpp"
  )
  qt5_wrap_cpp(MOC_SRCS ${ot-qobject-install-headers})
  target_sources(opentxs-ui-messagablelist PRIVATE ${MOC_SRCS})

  if(OPENTXS_STANDALONE)
    install(
      FILES ${ot-qobject-install-headers}
      DESTINATION include/opentxs/ui/qt
      COMPONENT dev
    )
  endif()
endif()

opentxs_set_qt(opentxs-ui-messagablelist)
target_sources(opentxs-ui-messagablelist PRIVATE ${cxx-install-headers})
target_sources(opentxs PRIVATE $<TARGET_OBJECTS:opentxs-ui-messagablelist>)
add_dependencies(opentxs-ui-messagablelist generated_code)

if(OPENTXS_STANDALONE)
  install(
    FILES ${cxx-install-headers}
    DESTINATION include/opentxs/ui
    COMPONENT dev
  )
endif()
