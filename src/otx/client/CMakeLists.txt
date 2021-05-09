# Copyright (c) 2010-2021 The Open-Transactions developers
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_subdirectory(workflow)

add_library(
  opentxs-otx-client OBJECT
  "DepositPayment.cpp"
  "DepositPayment.hpp"
  "Operation.cpp"
  "Operation.hpp"
  "PaymentTasks.cpp"
  "PaymentTasks.hpp"
  "StateMachine.cpp"
  "StateMachine.hpp"
  "StateMachine.tpp"
  "${opentxs_SOURCE_DIR}/src/internal/otx/client/Client.hpp"
)
target_link_libraries(opentxs-otx-client PRIVATE opentxs::messages)
target_include_directories(
  opentxs-otx-client SYSTEM PRIVATE "${opentxs_SOURCE_DIR}/deps/"
)
target_sources(opentxs PRIVATE $<TARGET_OBJECTS:opentxs-otx-client>)
add_dependencies(opentxs-otx-client generated_code)
