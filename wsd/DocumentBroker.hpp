/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_DOCUMENTBROKER_HPP
#define INCLUDED_DOCUMENTBROKER_HPP

#include <csignal>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <Poco/URI.h>

#include "IoUtil.hpp"
#include "Log.hpp"
#include "TileDesc.hpp"
#include "Util.hpp"
#include "net/Socket.hpp"
#include "net/WebSocketHandler.hpp"

#include "common/SigUtil.hpp"

// Forwards.
class PrisonerRequestDispatcher;
class DocumentBroker;
class StorageBase;
class TileCache;
class Message;

class TerminatingPoll : public SocketPoll
{
public:
    TerminatingPoll(const std::string &threadName) :
        SocketPoll(threadName) {}

    bool continuePolling() override
    {
        return SocketPoll::continuePolling()
            && !TerminationFlag && !ShutdownRequestFlag;
    }
};

/// Represents a new LOK child that is read
/// to host a document.
class ChildProcess
{
    // FIXME: urk ...
    friend class PrisonerRequestDispatcher;
public:
    /// @param pid is the process ID of the child.
    /// @param socket is the underlying Sockeet to the child.
    ChildProcess(const Poco::Process::PID pid, const std::shared_ptr<StreamSocket>& socket, const Poco::Net::HTTPRequest &request) :

        _pid(pid),
        _ws(std::make_shared<WebSocketHandler>(socket, request)),
        _socket(socket)
    {
        LOG_INF("ChildProcess ctor [" << _pid << "].");
    }

    ChildProcess(ChildProcess&& other) = delete;

    const ChildProcess& operator=(ChildProcess&& other) = delete;

    ~ChildProcess()
    {
        if (_pid > 0)
        {
            LOG_DBG("~ChildProcess dtor [" << _pid << "].");
            close(true);

            // No need for the socket anymore.
            _ws.reset();
            _socket.reset();
        }
    }

    void setDocumentBroker(const std::shared_ptr<DocumentBroker>& docBroker);

    void stop()
    {
        // FIXME: stop !?
        LOG_ERR("What do we do for stop?");
        try
        {
            if (isAlive())
            {
                LOG_DBG("Stopping ChildProcess [" << _pid << "]");
                sendTextFrame("exit");
            }
        }
        catch (const std::exception&)
        {
            // Already logged in sendTextFrame.
        }
    }

    void close(const bool rude)
    {
        if (_pid < 0)
            return;

        try
        {
            LOG_DBG("Closing ChildProcess [" << _pid << "].");

            // First mark to stop the thread so it knows it's intentional.
            stop();

            // Shutdown the socket to break the thread if blocked on it.
            if (_ws)
                _ws->shutdown();
        }
        catch (const std::exception& ex)
        {
            LOG_ERR("Error while closing child process: " << ex.what());
        }

        // Kill or abandon the child.
        if (_pid != -1 && rude && kill(_pid, 0) != 0 && errno != ESRCH)
        {
            LOG_INF("Killing child [" << _pid << "].");
            if (SigUtil::killChild(_pid))
            {
                LOG_ERR("Cannot terminate lokit [" << _pid << "]. Abandoning.");
            }
        }

        _pid = -1;
    }

    Poco::Process::PID getPid() const { return _pid; }

    /// Send a text payload to the child-process WS.
    bool sendTextFrame(const std::string& data)
    {
        try
        {
            if (_ws)
            {
                LOG_TRC("DocBroker to Child: " << data);
                _ws->sendFrame(data);
                return true;
            }
        }
        catch (const std::exception& exc)
        {
            LOG_ERR("Failed to send child [" << _pid << "] data [" <<
                    data << "] due to: " << exc.what());
            throw;
        }

        LOG_WRN("No socket between DocBroker and child to send [" << data << "]");
        return false;
    }

    /// Check whether this child is alive and socket not in error.
    /// Note: zombies will show as alive, and sockets have waiting
    /// time after the other end-point closes. So this isn't accurate.
    bool isAlive() const
    {
        try
        {
            return _pid > 1 && _ws && kill(_pid, 0) == 0;
        }
        catch (const std::exception&)
        {
        }

        return false;
    }

private:
    void socketProcessor();

private:
    Poco::Process::PID _pid;
    std::shared_ptr<WebSocketHandler> _ws;
    std::shared_ptr<Socket> _socket;
    std::weak_ptr<DocumentBroker> _docBroker;
};

class ClientSession;

/// DocumentBroker is responsible for setting up a document
/// in jail and brokering loading it from Storage
/// and saving it back.
/// Contains URI, physical path, etc.
class DocumentBroker : public std::enable_shared_from_this<DocumentBroker>
{
    class DocumentBrokerPoll;
public:
    static Poco::URI sanitizeURI(const std::string& uri);

    /// Returns a document-specific key based
    /// on the URI of the document.
    static std::string getDocKey(const Poco::URI& uri);

    /// Dummy document broker that is marked to destroy.
    DocumentBroker();

    /// Use create - not this constructor ...
    /// FIXME: friend with make_shared etc.
    DocumentBroker(const std::string& uri,
                   const Poco::URI& uriPublic,
                   const std::string& docKey,
                   const std::string& childRoot);
public:
    static std::shared_ptr<DocumentBroker> create(
                   const std::string& uri,
                   const Poco::URI& uriPublic,
                   const std::string& docKey,
                   const std::string& childRoot);


    ~DocumentBroker();

    /// Start processing events
    void startThread();

    /// Loads a document from the public URI into the jail.
    bool load(std::shared_ptr<ClientSession>& session, const std::string& jailId);
    bool isLoaded() const { return _isLoaded; }
    void setLoaded() { _isLoaded = true; }

    /// Save the document to Storage if it needs persisting.
    bool saveToStorage(const std::string& sesionId, bool success, const std::string& result = "");
    bool isModified() const { return _isModified; }
    void setModified(const bool value);

    /// Save the document if the document is modified.
    /// @param force when true, will force saving if there
    /// has been any recent activity after the last save.
    /// @return true if attempts to save or it also waits
    /// and receives save notification. Otherwise, false.
    bool autoSave(const bool force);

    Poco::URI getPublicUri() const { return _uriPublic; }
    Poco::URI getJailedUri() const { return _uriJailed; }
    const std::string& getJailId() const { return _jailId; }
    const std::string& getDocKey() const { return _docKey; }
    const std::string& getFilename() const { return _filename; };
    TileCache& tileCache() { return *_tileCache; }
    bool isAlive() const;
    size_t getSessionsCount() const
    {
        Util::assertIsLocked(_mutex);
        return _sessions.size();
    }

    void dumpState();

    std::string getJailRoot() const;

    /// Queue a new session to be added asynchronously.
    /// @return amount of session we have after all the queued ones will be
    /// created.
    size_t queueSession(std::shared_ptr<ClientSession>& session);

    /// Removes a session by ID. Returns the new number of sessions.
    size_t removeSession(const std::string& id, bool destroyIfLast = false);

    void addSocketToPoll(const std::shared_ptr<Socket>& socket);

    void alertAllUsers(const std::string& msg);

    void alertAllUsers(const std::string& cmd, const std::string& kind)
    {
        alertAllUsers("error: cmd=" + cmd + " kind=" + kind);
    }

    /// Invalidate the cursor position.
    void invalidateCursor(int x, int y, int w, int h)
    {
        _cursorPosX = x;
        _cursorPosY = y;
        _cursorWidth = w;
        _cursorHeight = h;
    }

    void invalidateTiles(const std::string& tiles);
    void handleTileRequest(TileDesc& tile,
                           const std::shared_ptr<ClientSession>& session);
    void handleTileCombinedRequest(TileCombined& tileCombined,
                                   const std::shared_ptr<ClientSession>& session);
    void cancelTileRequests(const std::shared_ptr<ClientSession>& session);
    void handleTileResponse(const std::vector<char>& payload);
    void handleTileCombinedResponse(const std::vector<char>& payload);

    void destroyIfLastEditor(const std::string& id);
    bool isMarkedToDestroy() const { return _markToDestroy; }

    bool handleInput(const std::vector<char>& payload);

    /// Forward a message from client session to its respective child session.
    bool forwardToChild(const std::string& viewId, const std::string& message);

    int getRenderedTileCount() { return _debugRenderedTileCount; }

    void closeDocument(const std::string& reason);

    /// Called by the ChildProcess object to notify
    /// that it has terminated on its own.
    /// This happens either when the child exists
    /// or upon failing to process an incoming message.
    void childSocketTerminated();

    /// This gracefully terminates the connection
    /// with the child and cleans up ChildProcess etc.
    /// We must be called under lock and it must be
    /// passed to us so we unlock before waiting on
    /// the ChildProcess thread, which can take our lock.
    void terminateChild(std::unique_lock<std::mutex>& lock, const std::string& closeReason);

    /// Get the PID of the associated child process
    Poco::Process::PID getPid() const { return _childProcess->getPid(); }

    std::unique_lock<std::mutex> getLock() { return std::unique_lock<std::mutex>(_mutex); }
    std::unique_lock<std::mutex> getDeferredLock() { return std::unique_lock<std::mutex>(_mutex, std::defer_lock); }

    void updateLastActivityTime();

    std::size_t getIdleTimeSecs() const
    {
        const auto duration = (std::chrono::steady_clock::now() - _lastActivityTime);
        return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }

private:
    /// Sends the .uno:Save command to LoKit.
    bool sendUnoSave(const bool dontSaveIfUnmodified);

    /// Saves the doc to the storage.
    bool saveToStorageInternal(const std::string& sesionId, bool success, const std::string& result = "");

    /// Removes a session by ID. Returns the new number of sessions.
    size_t removeSessionInternal(const std::string& id);

    /// Forward a message from child session to its respective client session.
    bool forwardToClient(const std::shared_ptr<Message>& payload);

    /// Add a new session. Returns the new number of sessions.
    size_t addSession(std::shared_ptr<ClientSession>& session);

    /// The thread function that all of the I/O for all sessions
    /// associated with this document.
    void pollThread();

private:
    const std::string _uriOrig;
    const Poco::URI _uriPublic;
    const std::string _docKey;
    const std::string _childRoot;
    const std::string _cacheRoot;
    std::shared_ptr<ChildProcess> _childProcess;
    Poco::URI _uriJailed;
    std::string _jailId;
    std::string _filename;

    /// The last time we tried saving, regardless of whether the
    /// document was modified and saved or not.
    std::chrono::steady_clock::time_point _lastSaveTime;

    /// The document's last-modified time on storage.
    Poco::Timestamp _documentLastModifiedTime;

    /// The jailed file last-modified time.
    Poco::Timestamp _lastFileModifiedTime;
    std::map<std::string, std::shared_ptr<ClientSession> > _sessions;

    /// Private class to remember what sessions we have to create, and what
    /// messages to send to them after they are really created.
    class NewSession {
    public:
        NewSession(const std::shared_ptr<ClientSession>& session) : _session(session)
        {
        }

        std::shared_ptr<ClientSession> _session;
        std::deque<std::string> _messages;
    };

    /// Sessions that are queued for addition.
    std::deque<NewSession> _newSessions;

    std::unique_ptr<StorageBase> _storage;
    std::unique_ptr<TileCache> _tileCache;
    std::atomic<bool> _markToDestroy;
    std::atomic<bool> _lastEditableSession;
    std::atomic<bool> _isLoaded;
    std::atomic<bool> _isModified;
    int _cursorPosX;
    int _cursorPosY;
    int _cursorWidth;
    int _cursorHeight;
    mutable std::mutex _mutex;
    std::unique_ptr<DocumentBrokerPoll> _poll;
    std::atomic<bool> _stop;

    /// Versioning is used to prevent races between
    /// painting and invalidation.
    std::atomic<size_t> _tileVersion;

    int _debugRenderedTileCount;

    std::chrono::steady_clock::time_point _lastActivityTime;

    static constexpr auto IdleSaveDurationMs = 30 * 1000;
    static constexpr auto AutoSaveDurationMs = 300 * 1000;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
