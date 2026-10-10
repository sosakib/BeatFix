#pragma once

#include <string>
#include <utility>
#include <stdexcept>

namespace bf {

template <typename T>
class Result {
public:
    static Result<T> ok(T value) {
        return Result<T>(std::move(value), "");
    }

    static Result<T> error(std::string message) {
        return Result<T>(T{}, std::move(message));
    }

    bool is_ok() const noexcept { return m_error.empty(); }
    bool is_error() const noexcept { return !m_error.empty(); }
    explicit operator bool() const noexcept { return is_ok(); }

    const T& value() const {
        if (!is_ok()) {
            throw std::runtime_error("Attempted to access value of failed Result: " + m_error);
        }
        return m_value;
    }

    T& value() {
        if (!is_ok()) {
            throw std::runtime_error("Attempted to access value of failed Result: " + m_error);
        }
        return m_value;
    }

    T take_value() {
        if (!is_ok()) {
            throw std::runtime_error("Attempted to access value of failed Result: " + m_error);
        }
        return std::move(m_value);
    }

    const std::string& error_message() const noexcept {
        return m_error;
    }

    T value_or(T fallback) const {
        return is_ok() ? m_value : fallback;
    }

private:
    Result(T val, std::string err)
        : m_value(std::move(val)), m_error(std::move(err)) {}

    T m_value{};
    std::string m_error;
};

// Specialization for void
template <>
class Result<void> {
public:
    static Result<void> ok() {
        return Result<void>("");
    }

    static Result<void> error(std::string message) {
        return Result<void>(std::move(message));
    }

    bool is_ok() const noexcept { return m_error.empty(); }
    bool is_error() const noexcept { return !m_error.empty(); }
    explicit operator bool() const noexcept { return is_ok(); }

    const std::string& error_message() const noexcept {
        return m_error;
    }

private:
    explicit Result(std::string err) : m_error(std::move(err)) {}
    std::string m_error;
};

} // namespace bf
