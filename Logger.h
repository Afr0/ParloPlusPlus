/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#pragma once

#include <iostream>
#include <string>
#include <functional>

//Usage example
/*int main() {
    Logger::onMessageLogged = [](const LogMessage& msg) {
        std::cout << "Logged: " << msg.message << std::endl;
        };

    Logger::Log("This is a test message", LogLevel::info);

    return 0;
}*/

namespace Parlo
{
    /// <summary>
    /// The log level.
    /// </summary>
    enum class LogLevel
    {
        error = 1,
        warn,
        info,
        verbose
    };

    /// <summary>
    /// Represents a log message.
    /// </summary>
    class LogMessage
    {
    public:
        LogMessage(const std::string& msg, LogLevel lvl) : message(msg), level(lvl) {}

        std::string message;
        LogLevel level;
    };

    /// <summary>
    /// A logger for Parlo.
    /// </summary>
    class Logger
    {
    public:
        static std::function<void(const LogMessage&)> onMessageLogged;

        static void Log(const std::string& message, LogLevel lvl) {

#ifdef DEBUG
            LogMessage msg(message, lvl);
            if (onMessageLogged)
                onMessageLogged(msg);
            else
                std::cout << msg.message << std::endl; //Equivalent to Debug.WriteLine in C#
#endif
        }
    };
}
