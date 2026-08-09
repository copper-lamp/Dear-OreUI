#pragma once

#include "api/types/Error.h"

#include <optional>
#include <variant>

namespace dearoreui::api {

template <typename T, typename E = Error>
class Result {
public:
    Result(T value) : mStorage(std::move(value)) {}

    Result(E error) : mStorage(std::move(error)) {}

    [[nodiscard]] static Result success(T value) { return Result(std::move(value)); }

    [[nodiscard]] static Result failure(E error) { return Result(std::move(error)); }

    [[nodiscard]] bool isOk() const { return std::holds_alternative<T>(mStorage); }

    [[nodiscard]] bool isErr() const { return std::holds_alternative<E>(mStorage); }

    [[nodiscard]] T const& value() const { return std::get<T>(mStorage); }

    [[nodiscard]] T& value() { return std::get<T>(mStorage); }

    [[nodiscard]] E const& error() const { return std::get<E>(mStorage); }

    [[nodiscard]] E& error() { return std::get<E>(mStorage); }

    [[nodiscard]] T const& operator*() const { return value(); }

    [[nodiscard]] T& operator*() { return value(); }

    [[nodiscard]] T const* operator->() const { return &value(); }

    [[nodiscard]] T* operator->() { return &value(); }

private:
    std::variant<T, E> mStorage;
};

template <typename E>
class Result<void, E> {
public:
    Result() : mError(), mHasValue(true) {}

    Result(E error) : mError(std::move(error)), mHasValue(false) {}

    [[nodiscard]] static Result success() { return Result{}; }

    [[nodiscard]] static Result failure(E error) { return Result(std::move(error)); }

    [[nodiscard]] bool isOk() const { return mHasValue; }

    [[nodiscard]] bool isErr() const { return !mHasValue; }

    void value() const {}

    [[nodiscard]] E const& error() const { return mError; }

    [[nodiscard]] E& error() { return mError; }

private:
    E    mError;
    bool mHasValue{true};
};

} // namespace dearoreui::api
