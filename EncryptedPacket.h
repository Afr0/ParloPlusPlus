/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the Parlo library.

The Initial Developer of the Original Code is
Mats 'Afr0' Vederhus. All Rights Reserved.

Contributor(s): ______________________________________.
*/

#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "Packet.h"
#include "EncryptionArgs.h"
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/modes.h>
#include <cryptopp/osrng.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/twofish.h>

namespace Parlo
{
    /// <summary>
    /// Represents an encrypted packet.
    /// </summary>
	class EncryptedPacket : Packet
	{
	public: 

        /// <summary>
        /// Creates a new instance of EncryptedPacket.
        /// </summary>
        /// <param name="args">The <see cref="EncryptionArgs"/> used for this packet's encryption.</param>
        /// <param name="id">The ID of the packet.</param>
        /// <param name="serializedData">The serialized data to send.</param>
		EncryptedPacket(EncryptionArgs *args, uint8_t id, const std::vector<uint8_t>& serializedData) : 
            m_args(args), Packet(id, serializedData, false)
		{
			if(args == nullptr)
				throw std::invalid_argument("EncryptionArgs cannot be null!");
			if(serializedData.empty())
				throw std::invalid_argument("SerializedData cannot be null!");
		};

        ~EncryptedPacket() {};

        /// <summary>
        /// Decrypts the serialized data of a packet.
        /// </summary>
        /// <returns>The serialied data as an array of bytes.</returns>
        std::vector<uint8_t> decryptPacket() 
        {
            if (m_args->Mode == EncryptionMode::AES)
                return decryptAES(data, m_args->Key, m_args->Salt);
            else if (m_args->Mode == EncryptionMode::Twofish) 
                return decryptTwofish(data, m_args->Key, m_args->Salt);
            else
                throw std::runtime_error("Unsupported encryption mode");
        }

        /// <summary>
        /// Builds a encrypted packet ready for sending.
        /// An encrypted packet has an extra byte after 
        /// the length indicating that it is encrypted.
        /// </summary>
        /// <returns>The packet as an array of bytes.</returns>
        std::vector<uint8_t> buildPacket() const override 
        {
            std::vector<uint8_t> encryptedData;

            if (m_args->Mode == EncryptionMode::AES)
                encryptedData = encryptAES(data, m_args->Key, m_args->Salt);
            else if (m_args->Mode == EncryptionMode::Twofish)
                encryptedData = encryptTwofish(data, m_args->Key, m_args->Salt);
            else
                throw std::runtime_error("Unsupported encryption mode");

            std::vector<uint8_t> packetData;
            packetData.push_back(getID());
            packetData.push_back(getIsCompressed());
            packetData.insert(packetData.end(), encryptedData.begin(), encryptedData.end());
            return packetData;
        }

    private:
        std::shared_ptr<EncryptionArgs> m_args;

        std::vector<uint8_t> encryptAES(const std::vector<uint8_t>& data, const std::string& key, const std::string& salt) const
        {
            CryptoPP::byte keyBytes[CryptoPP::AES::DEFAULT_KEYLENGTH], ivBytes[CryptoPP::AES::BLOCKSIZE];

            //Deriving key and IV from password and salt
            CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf;
            CryptoPP::byte derivedBytes[CryptoPP::AES::DEFAULT_KEYLENGTH + CryptoPP::AES::BLOCKSIZE];
            pbkdf.DeriveKey(
                derivedBytes, sizeof(derivedBytes),
                0, //Purpose byte (unused)
                reinterpret_cast<const CryptoPP::byte*>(key.data()), key.size(),
                reinterpret_cast<const CryptoPP::byte*>(salt.data()), salt.size(),
                10000 //Iterations
            );

            //Splitting the derived bytes into key and IV
            std::copy(derivedBytes, derivedBytes + CryptoPP::AES::DEFAULT_KEYLENGTH, keyBytes);
            std::copy(derivedBytes + CryptoPP::AES::DEFAULT_KEYLENGTH, derivedBytes + sizeof(derivedBytes), ivBytes);

            //Setting up AES encryption
            CryptoPP::AES::Encryption aesEncryption(keyBytes, sizeof(keyBytes));
            CryptoPP::CBC_Mode_ExternalCipher::Encryption cbcEncryption(aesEncryption, ivBytes);

            std::string cipher;
            CryptoPP::StreamTransformationFilter stfEncryptor(cbcEncryption, new CryptoPP::StringSink(cipher));
            stfEncryptor.Put(data.data(), data.size());
            stfEncryptor.MessageEnd();

            std::vector<uint8_t> encryptedData(cipher.begin(), cipher.end());
            return encryptedData;
        }

        std::vector<uint8_t> decryptAES(const std::vector<uint8_t>& data, const std::string& key, const std::string& salt) 
        {
            CryptoPP::byte keyBytes[CryptoPP::AES::DEFAULT_KEYLENGTH], ivBytes[CryptoPP::AES::BLOCKSIZE];
            //Deriving key and IV from password and salt
            CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf;
            CryptoPP::byte derivedBytes[CryptoPP::AES::DEFAULT_KEYLENGTH + CryptoPP::AES::BLOCKSIZE];
            pbkdf.DeriveKey(
                derivedBytes, sizeof(derivedBytes),
                0, //Purpose byte (unused)
                reinterpret_cast<const CryptoPP::byte*>(key.data()), key.size(),
                reinterpret_cast<const CryptoPP::byte*>(salt.data()), salt.size(),
                10000 //Iterations
            );

            CryptoPP::AES::Decryption aesDecryption(keyBytes, sizeof(keyBytes));
            CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(aesDecryption, ivBytes);

            std::string recovered;
            CryptoPP::StreamTransformationFilter stfDecryptor(cbcDecryption, new CryptoPP::StringSink(recovered));
            stfDecryptor.Put(data.data(), data.size());
            stfDecryptor.MessageEnd();

            return std::vector<uint8_t>(recovered.begin(), recovered.end());
        }

        std::vector<uint8_t> encryptTwofish(const std::vector<uint8_t>& data, const std::string& key, const std::string& salt) const 
        {
            CryptoPP::byte keyBytes[CryptoPP::Twofish::DEFAULT_KEYLENGTH], ivBytes[CryptoPP::Twofish::BLOCKSIZE];
            //Deriving key and IV from password and salt
            CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf;
            CryptoPP::byte derivedBytes[CryptoPP::AES::DEFAULT_KEYLENGTH + CryptoPP::AES::BLOCKSIZE];
            pbkdf.DeriveKey(
                derivedBytes, sizeof(derivedBytes),
                0, //Purpose byte (unused)
                reinterpret_cast<const CryptoPP::byte*>(key.data()), key.size(),
                reinterpret_cast<const CryptoPP::byte*>(salt.data()), salt.size(),
                10000 //Iterations
            );

            CryptoPP::Twofish::Encryption twofishEncryption(keyBytes, sizeof(keyBytes));
            CryptoPP::CBC_Mode_ExternalCipher::Encryption cbcEncryption(twofishEncryption, ivBytes);

            std::string cipher;
            CryptoPP::StreamTransformationFilter stfEncryptor(cbcEncryption, new CryptoPP::StringSink(cipher));
            stfEncryptor.Put(data.data(), data.size());
            stfEncryptor.MessageEnd();

            return std::vector<uint8_t>(cipher.begin(), cipher.end());
        }

        std::vector<uint8_t> decryptTwofish(const std::vector<uint8_t>& data, const std::string& key, const std::string& salt) 
        {
            CryptoPP::byte keyBytes[CryptoPP::Twofish::DEFAULT_KEYLENGTH], ivBytes[CryptoPP::Twofish::BLOCKSIZE];
            //Deriving key and IV from password and salt
            CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf;
            CryptoPP::byte derivedBytes[CryptoPP::AES::DEFAULT_KEYLENGTH + CryptoPP::AES::BLOCKSIZE];
            pbkdf.DeriveKey(
                derivedBytes, sizeof(derivedBytes),
                0, //Purpose byte (unused)
                reinterpret_cast<const CryptoPP::byte*>(key.data()), key.size(),
                reinterpret_cast<const CryptoPP::byte*>(salt.data()), salt.size(),
                10000 //Iterations
            );

            CryptoPP::Twofish::Decryption twofishDecryption(keyBytes, sizeof(keyBytes));
            CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(twofishDecryption, ivBytes);

            std::string recovered;
            CryptoPP::StreamTransformationFilter stfDecryptor(cbcDecryption, new CryptoPP::StringSink(recovered));
            stfDecryptor.Put(data.data(), data.size());
            stfDecryptor.MessageEnd();

            return std::vector<uint8_t>(recovered.begin(), recovered.end());
        }

        /// <summary>
        /// Converts a hex string to a byte array.
        /// </summary>
        /// <param name="hex">The hex string to convert.</param>
        /// <returns>A byte array containing the converted string.</returns>
        std::vector<uint8_t> hexStringToByteArray(const std::string& hex)
        {
            std::vector<uint8_t> bytes;
            for (size_t i = 0; i < hex.length(); i += 2)
            {
                std::string byteString = hex.substr(i, 2);
                uint8_t byte = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
                bytes.push_back(byte);
            }

            return bytes;
        }
	};
}
