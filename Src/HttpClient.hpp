#pragma once
#define WIN32_LEAN_AND_MEAN

#include <string>
#include <mutex>
#include <stdlib.h>
#include <stdio.h>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


namespace ClientMJPEG
{
	class HttpClient
	{
	public:
		HttpClient(const std::string& host, const int& port);
		~HttpClient();

		HttpClient(const HttpClient& other) = delete;
		HttpClient(HttpClient&& other) = delete;

		HttpClient& operator=(HttpClient&& other) = delete;
		HttpClient& operator=(const HttpClient& other) = delete;

		bool Connect(std::string* outErrorMessage);
		void Close();

		bool SendRequestGetOnStream(const std::string& url, std::string* outErrorMessage);

		const std::string& GetPort() const { return _port; }
		const std::string& GetHost() const { return _host; }

	private:
		std::string _host;
		std::string _port;
		SOCKET _connectSocket = INVALID_SOCKET;
		std::mutex _m;
		bool _sendShutdown = false;
		bool _isConnected = false;
	};
}//namespace ClientMJPEG