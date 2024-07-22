#include "pch.h"
#include "Logger.h"

namespace Parlo
{
    //Definition of the static member
    std::function<void(const LogMessage&)> Logger::onMessageLogged = nullptr;
}