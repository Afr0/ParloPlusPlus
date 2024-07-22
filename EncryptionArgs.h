#pragma once

#include <string>
#include "EncryptionMode.h"

namespace Parlo
{
	struct EncryptionArgs
	{
		EncryptionMode Mode;

		/// <summary>
		/// The key used for encryption.
		/// </summary>
		std::string Key;

		/// <summary>
		/// The salt used for encryption.
		/// </summary>
		std::string Salt; 
	};
}
