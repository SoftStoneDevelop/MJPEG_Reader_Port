#pragma once

#include "HttpClient.hpp"
#include <mutex>
#include <stdlib.h>
#include <stdio.h>
#include <cassert>

namespace ClientMJPEG
{
	HttpClient::HttpClient(
		const std::string& host,
		const std::string& port,
		std::shared_ptr<ThreadPool::ThreadPool> threadPool
	) : _host{ host }, _port{ std::move(port) }, threadPool{ std::move(threadPool) }
	{
	}

	HttpClient::~HttpClient()
	{
		Close();
	}

	void HttpClient::Close()
	{
		{
			std::lock_guard lg(_m);
			_streamInProcess = false;
			_cv.notify_all();
		}

		if (_readTask.valid())
		{
			_readTask.get();
		}
		_readBuffer = nullptr;

		if (_isConnected)
		{
			closesocket(_connectSocket);
			WSACleanup();
			_isConnected = false;
		}
	}

	bool HttpClient::Connect(std::string* outErrorMessage)
	{
		std::lock_guard lg(_m);
		if (_isConnected)
		{
			if (outErrorMessage)
			{
				*outErrorMessage = "already connected";
			}
			return false;
		}
		int iResult;

		// Initialize Winsock
		WSADATA wsaData;
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0)
		{
			if (outErrorMessage)
			{
				*outErrorMessage = "WSAStartup failed with error: " + iResult;
			}
			return false;
		}

		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		addrinfo* result = nullptr;
		// Resolve the server address and port
		iResult = getaddrinfo(_host.c_str(), _port.c_str(), &hints, &result);
		if (iResult != 0)
		{
			if (outErrorMessage)
			{
				*outErrorMessage = "getaddrinfo failed with error: " + iResult;
			}
			WSACleanup();
			return 1;
		}

		addrinfo* ptr = nullptr;
		// Attempt to connect to an address until one succeeds
		for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
		{
			// Create a SOCKET for connecting to server
			_connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
			if (_connectSocket == INVALID_SOCKET)
			{
				WSACleanup();
				freeaddrinfo(result);

				if (outErrorMessage)
				{
					*outErrorMessage = "socket failed with error: " + WSAGetLastError();
				}

				return false;
			}

			// Connect to server.
			iResult = connect(_connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (iResult == SOCKET_ERROR)
			{
				closesocket(_connectSocket);
				_connectSocket = INVALID_SOCKET;
				continue;
			}
			break;
		}

		freeaddrinfo(result);

		if (_connectSocket == INVALID_SOCKET)
		{
			if (outErrorMessage)
			{
				*outErrorMessage = "Unable to connect to server!";
			}
			WSACleanup();
			return false;
		}

		_isConnected = true;
		return _isConnected;
	}

	bool HttpClient::SendRequestGetOnStream(const std::string& url, std::string* outErrorMessage)
	{
		std::lock_guard lg(_m);
		if (!_isConnected || _sendShutdown)
		{
			return false;
		}

		std::string request = "GET " + url + " HTTP/1.1\r\nHost: " + _host + "\r\nContent-Length: 0\r\n\r\n";

		// Send an initial buffer
		int iResult = send(_connectSocket, request.c_str(), request.size(), 0);
		if (iResult == SOCKET_ERROR)
		{
			if (outErrorMessage)
			{
				*outErrorMessage = "send failed with error:" + WSAGetLastError();
			}

			closesocket(_connectSocket);
			WSACleanup();
			return false;
		}

		//TODO #2 swich between many sockets and read data in one thread not in thread per socket
		/*u_long iMode = 1;
		iResult = ioctlsocket(_connectSocket, FIONBIO, &iMode);
		if (iResult != NO_ERROR)
			printf("ioctlsocket failed with error: %ld\n", iResult);*/

		_streamInProcess = true;
		return _streamInProcess;
	}

	std::future<int> HttpClient::ReadAsync(char* buffer, const int& bufferSize)
	{
		std::lock_guard lg(_m);
		if (!_streamInProcess || _isReading)
		{
			throw std::runtime_error("Stream not started or trying to read from many threads");
		}

		_promise = std::promise<int>();
		_readBuffer = buffer;
		_readBufferSize = bufferSize;

		_readTask = threadPool->enqueue(&HttpClient::readData, this);
		_isReading = true;

		_cv.notify_all();
		return _promise.get_future();
	}

	void HttpClient::readData()
	{
		std::unique_lock<std::mutex> lock(_m);
		_cv.wait(lock, [&] {return _isReading || !_streamInProcess; });
		if (!_streamInProcess)
		{
			_promise.set_value(0);
			return;
		}

		assert(_readBuffer != nullptr && "_readBuffer is nullptr");
		auto recived = recv(_connectSocket, _readBuffer, _readBufferSize, 0);
		if (recived < 0)
		{
			_promise.set_value(recived);
			_isReading = false;
			_readBuffer = nullptr;
			return;
		}
		else if (recived == 0)
		{
			closesocket(_connectSocket);
			WSACleanup();
			_promise.set_value(recived);
			_streamInProcess = false;
			_isConnected = false;
			_isReading = false;
			_readBuffer = nullptr;
			return;
		}

		_promise.set_value(recived);
		_isReading = false;
		_readBuffer = nullptr;
	}

}//namespace ClientMJPEG