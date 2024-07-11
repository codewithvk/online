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

#include <string>

#include <Poco/File.h>
#include <Poco/Path.h>

namespace JailUtil
{

/// General temporary directory owned by us.
constexpr const char CHILDROOT_TMP_PATH[] = "/tmp";

/// Files uploaded by users are stored in this sub-directory of child-root.
constexpr const char CHILDROOT_TMP_INCOMING_PATH[] = "/tmp/incoming";

/// The LO installation directory with jail.
constexpr const char LO_JAIL_SUBPATH[] = "lo";

/// Try to put this process into its own user and mount namespace and
/// set as root within that namespace to allow mounting
/// There cannot be other threads running when calling these or it will
/// fail.
bool becomeMountingUser(uid_t uid, gid_t gid);
/// We could switch back to normal user within the namespace too
bool restorePremountUser(uid_t uid, gid_t gid);

/// return the local path to the jailPath under localJailRoot
/// localJailRoot /chroot/jailId
/// jailPath /tmp/user/doc/childId
/// with usingMountNamespaces false then simply return:
/// -> /chroot/jailId/tmp/user/doc/childId
/// otherwise replaces jailPath's in /tmp with the tmp dir that is mounted
/// from, e.g. return:
/// -> /chroot/tmp/cool-jailId/tmp/user/doc/childId
std::string buildLocalPathToJail(bool usingMountNamespaces, std::string localJailRoot, std::string jailPath);

/// Bind mount a jail directory.
bool bind(const std::string& source, const std::string& target);

/// Remount a bound mount point as readonly.
bool remountReadonly(const std::string& source, const std::string& target);

/// Marks a jail as having been copied instead of mounted.
void markJailCopied(const std::string& root);

/// Returns true iff the jail in question was copied and not mounted.
bool isJailCopied(const std::string& root);

/// Remove the jail directory and all its contents.
bool tryRemoveJail(const std::string& root);

/// Remove all jails.
void cleanupJails(const std::string& jailRoot);

/// Creates the jail directory path recursively.
void createJailPath(const std::string& path);

/// Setup the Child-Root directory.
void setupChildRoot(bool bindMount, const std::string& jailRoot, const std::string& sysTemplate);

/// Enable bind-mounting in this process.
void enableBindMounting();

/// Disable bind-mounting in this process.
void disableBindMounting();

/// Returns true iff bind-mounting is enabled in this process.
bool isBindMountingEnabled();

/// Enable namespace-mounting in this process.
void enableMountNamespaces();

/// Returns true iff namespace-mounting is enabled in this process.
bool isMountNamespacesEnabled();

namespace SysTemplate
{
/// Setup links for /dev/random and /dev/urandom in systemplate.
void setupRandomDeviceLinks(const std::string& root);

/// Setup the dynamic files within the sysTemplate by either
/// copying or linking. See updateJail_DynamicFilesInSysTemplate.
/// If the dynamic files need updating and systemplate is read-only,
/// this will fail and mark files for copying.
void setupDynamicFiles(const std::string& sysTemplate);

/// Update the dynamic files within the sysTemplate before each child fork.
/// Returns false on failure.
bool updateDynamicFiles(const std::string& sysTemplate);

} // namespace SysTemplate

} // end namespace JailUtil

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
