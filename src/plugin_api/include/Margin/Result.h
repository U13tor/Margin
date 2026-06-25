// Result<T, E> - return type of PluginInterface::onLoad.
// Spec: docs/04-plugin-spec.md §2 (PluginInterface surface).

#pragma once

#include <utility>

namespace Margin {

template <typename T, typename E>
class Result {
public:
    static Result ok(T value) {
        Result r;
        r.m_isOk = true;
        r.m_value = std::move(value);
        return r;
    }
    static Result err(E error) {
        Result r;
        r.m_isOk = false;
        r.m_error = std::move(error);
        return r;
    }

    bool isOk() const  { return m_isOk; }
    bool isErr() const { return !m_isOk; }

    const T& value() const { return m_value; }
    const E& error() const { return m_error; }

private:
    Result() = default;
    bool m_isOk = false;
    T m_value{};
    E m_error{};
};

// void specialization: onLoad returns no payload, only success/failure + reason.
template <typename E>
class Result<void, E> {
public:
    static Result ok() {
        Result r;
        r.m_isOk = true;
        return r;
    }
    static Result err(E error) {
        Result r;
        r.m_isOk = false;
        r.m_error = std::move(error);
        return r;
    }

    bool isOk() const  { return m_isOk; }
    bool isErr() const { return !m_isOk; }

    const E& error() const { return m_error; }

private:
    Result() = default;
    bool m_isOk = false;
    E m_error{};
};

} // namespace Margin
