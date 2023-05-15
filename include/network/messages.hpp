#pragma once
#include "network/http_helper.hpp"
#include "network/io_uring_socket.hpp"
#include <memory>
#include <string>
//---------------------------------------------------------------------------
// AnyBlob - Universal Cloud Object Storage Library
// Dominik Durner, 2021
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
// SPDX-License-Identifier: MPL-2.0
//---------------------------------------------------------------------------
namespace anyblob {
//---------------------------------------------------------------------------
namespace utils {
class Timer;
template <typename T>
class DataVector;
}; // namespace utils
//---------------------------------------------------------------------------
namespace network {
//---------------------------------------------------------------------------
/// Current status of the message task
enum class MessageState : uint8_t {
    Init,
    InitSending,
    Sending,
    InitReceiving,
    Receiving,
    Finished,
    Aborted
};
//---------------------------------------------------------------------------
/// This is the original request message struct
struct OriginalMessage {
    /// The message
    std::unique_ptr<utils::DataVector<uint8_t>> message;
    /// The result
    std::unique_ptr<utils::DataVector<uint8_t>> result;

    /// The hostname
    std::string hostname;
    /// The port
    uint32_t port;

    /// Optional receive buffer
    uint8_t* receiveBuffer;
    /// Optional size of the receive buffer
    uint64_t bufferSize;
    /// Optional trace info
    uint64_t traceId;

    /// If it is a put request store the additional data
    /// The raw data ptr for put requests
    const uint8_t* putData;
    /// The length
    uint64_t putLength;

    /// The state
    MessageState state;

    /// The constructor
    OriginalMessage(std::unique_ptr<utils::DataVector<uint8_t>> message, std::string hostname, uint32_t port, uint8_t* receiveBuffer = nullptr, uint64_t bufferSize = 0, uint64_t traceId = 0) : message(std::move(message)), result(), hostname(hostname), port(port), receiveBuffer(receiveBuffer), bufferSize(bufferSize), traceId(traceId), putData(nullptr), putLength(), state(MessageState::Init) {}

    /// The destructor
    virtual ~OriginalMessage() = default;

    /// Add the put request data to the message
    void setPutRequestData(const uint8_t* data, uint64_t length) {
        this->putData = data;
        this->putLength = length;
    }

    /// Callback required
    virtual bool requiresFinish() { return false; }

    /// Callback
    virtual void finish() {}
};
//---------------------------------------------------------------------------
/// The callback original message
template <typename Callback>
struct OriginalCallbackMessage : public OriginalMessage {
    /// The callback
    Callback callback;

    /// The constructor
    OriginalCallbackMessage(Callback&& callback, std::unique_ptr<utils::DataVector<uint8_t>> message, std::string hostname, uint32_t port, uint8_t* receiveBuffer = nullptr, uint64_t bufferSize = 0, uint64_t traceId = 0) : OriginalMessage(move(message), hostname, port, receiveBuffer, bufferSize, traceId), callback(std::forward<Callback>(callback)) {}

    /// The destructor
    virtual ~OriginalCallbackMessage() override = default;

    /// Override if callback required
    bool requiresFinish() override { return true; }

    void finish() override {
        callback(move(result));
    }
};
//---------------------------------------------------------------------------
/// This implements a message task
/// After each execute invocation a new request was added to the uring queue and requires submission
struct MessageTask {
    /// Type of the message task
    enum class Type : uint8_t {
        HTTP
    };

    /// Original sending message
    OriginalMessage* originalMessage;
    /// Send message
    std::unique_ptr<IOUringSocket::Request> request;
    /// The reduced offset in the send buffers
    int64_t sendBufferOffset;
    /// The reduced offset in the receive buffers
    int64_t receiveBufferOffset;
    /// The message task class
    Type type;
    /// The failures
    unsigned failures;
    /// The failure limit
    const unsigned failuresMax = 128;

    /// The constructor
    MessageTask(OriginalMessage* sendingMessage, uint8_t* receiveBuffer = nullptr, uint64_t bufferSize = 0);
    /// The pure virtual  callback
    virtual MessageState execute(IOUringSocket& socket) = 0;
    /// The pure virtual destuctor
    virtual ~MessageTask(){};
};
//---------------------------------------------------------------------------
/// Implements a http message roundtrip
struct HTTPMessage : public MessageTask {
    /// The receive chunk size
    uint64_t chunkSize;
    /// The tcp settings
    IOUringSocket::TCPSettings tcpSettings;
    /// HTTP info header
    std::unique_ptr<HTTPHelper::Info> info;
    /// The constructor
    HTTPMessage(OriginalMessage* sendingMessage, uint64_t chunkSize, uint8_t* receiveBuffer = nullptr, uint64_t bufferSize = 0);
    /// The message excecute callback
    MessageState execute(IOUringSocket& socket) override;
    /// The destructor
    ~HTTPMessage() override = default;
    /// Reset for restart
    void reset(IOUringSocket& socket, bool aborted);
};
//---------------------------------------------------------------------------
} // namespace network
} // namespace anyblob
