// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/filesystem.hpp>
#include <cassert>

#include "OTLowLevelTestEnvironment.hpp"
#include "opentxs/OT.hpp"

const ot::ArgList OTLowLevelTestEnvironment::test_args_{
    {OPENTXS_ARG_HOME, {OTLowLevelTestEnvironment::random_path()}},
};

namespace fs = boost::filesystem;

std::string OTLowLevelTestEnvironment::random_path()
{
    const auto path = fs::temp_directory_path() /
                      fs::unique_path("opentxs-test-%%%%-%%%%-%%%%-%%%%");

    assert(fs::create_directories(path));

    return path.string();
}

void OTLowLevelTestEnvironment::SetUp() {}

void OTLowLevelTestEnvironment::TearDown() { ot::Cleanup(); }

OTLowLevelTestEnvironment::~OTLowLevelTestEnvironment() = default;
