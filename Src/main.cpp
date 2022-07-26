#pragma once

#include "HttpClient.hpp"

int main(int argc, char* argv[])
{
    ClientMJPEG::HttpClient client("31.160.161.51", 8081);
    std::string error;
    client.Connect(&error);
    client.SendRequestGetOnStream("/mjpg/video.mjpg", &error);

    // Receive until the peer closes the connection
    /*do 
    {
        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
            printf("Bytes received: %d\n", iResult);
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while (iResult > 0);*/

    return 0;
}