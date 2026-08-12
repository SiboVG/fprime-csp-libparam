#pragma once

#include "FprimeCsp/Runtime/Stack.hpp"

#include <cstdint>

extern "C" {
#include <param/param.h>
}

namespace FprimeCsp {

/** Registers application-owned dynamic descriptors with libparam. */
class Registry final {
  public:
    bool add(param_t& parameter);
    int lastError() const;

  private:
    int m_lastError = 0;
};

/** Binds libparam's stateless request handler to its standard CSP port. */
class Server final {
  public:
    bool bind(Stack& stack);
    bool isBound() const;
    int lastError() const;

  private:
    bool m_bound = false;
    int m_lastError = 0;
};

/** Synchronous, typed operations on application-owned remote descriptors. */
class Client final {
  public:
    explicit Client(std::uint8_t protocolVersion = 2);

    bool pullU32(param_t& parameter,
                 std::uint16_t peerNode,
                 std::uint32_t timeoutMs,
                 std::uint32_t& value);
    bool pushU32(param_t& parameter,
                 std::uint16_t peerNode,
                 std::uint32_t timeoutMs,
                 std::uint32_t value);

    int lastError() const;

  private:
    std::uint8_t m_protocolVersion;
    int m_lastError = 0;
};

}  // namespace FprimeCsp
