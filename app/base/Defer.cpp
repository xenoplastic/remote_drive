#include "Defer.h"

Defer::Defer(RELEASE_CALLBACK releaseCallback) :
    m_releaseCallback(releaseCallback)
{

}

Defer::~Defer()
{
    m_releaseCallback();
}
