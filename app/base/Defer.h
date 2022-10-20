/**
 * @brief lijun
 * 模仿go defer释放
 */

#pragma once

#include <functional>

using RELEASE_CALLBACK = std::function<void()>;

class Defer
{
public:
    Defer(RELEASE_CALLBACK releaseCallback);
    virtual ~Defer();

private:
    RELEASE_CALLBACK m_releaseCallback;
};
