#pragma once

class NonMovable {
public:
    NonMovable(NonMovable &&) noexcept = delete;
    NonMovable &operator=(NonMovable &&) noexcept = delete;
protected:
    NonMovable() = default;
    ~NonMovable() = default;
    NonMovable(const NonMovable &) = default;
    NonMovable &operator=(const NonMovable &) = default;
};
