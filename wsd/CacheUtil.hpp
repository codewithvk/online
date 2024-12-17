/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * Copyright the Collabora Online contributors.
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Cache
{
public:
    static void initialize(const std::string& path);

    static std::string getConfigId(const std::string& uri);

    static void cacheConfigFile(const std::string& configId, const std::string& uri,
                                const std::string& stamp, const std::string& filename);

    static bool supplyConfigFile(const std::string& configId, const std::string& uri,
                                 const std::string& stamp, const std::string& filename);

private:
    static std::string locateConfigFile(const std::string& configId, const std::string& uri);
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
