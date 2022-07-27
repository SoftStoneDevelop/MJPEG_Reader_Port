#pragma once

#include "HttpClient.hpp"
#include "../ArrayPool.hpp"
#include "Helper/ArrayExt.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[])
{
    bool stop = false;

    ClientMJPEG::HttpClient client("31.160.161.51", 8081);
    std::string error;
    client.Connect(&error);
    client.SendRequestGetOnStream("/mjpg/video.mjpg", &error);

    auto pool = ArrayPool::ArrayPool<char>();
    std::string contentLengthBytes = "\nContent-Length: ";
    std::string newLineBytes = "\n";
    std::string carriageReturnSize = "\r";
    
    int readBufferSize = 1024;
    int realSize;
    char* readBuffer = pool.Rent(readBufferSize, realSize);
    readBufferSize = realSize;

    int imageBufferSize = readBufferSize;
    char* imageBuffer = pool.Rent(imageBufferSize, realSize);
    imageBufferSize = realSize;
    int newIBufferSize = -1;
    int payloadSize = 0;
    int payloadOffset = 0;

    char* lengthImageBuffer = pool.Rent(sizeof(int), realSize);

    std::future<int> readAsync = client.ReadAsync(readBuffer, readBufferSize);
    while (!stop)
    {
        auto readSize =  readAsync.get();
        if (readSize <= 0)
        {
            break;
        }

        if (newIBufferSize != -1)
        {
            char* newImageBuffer = pool.Rent(newIBufferSize, realSize);
            std::copy(imageBuffer, imageBuffer + payloadSize, newImageBuffer);
            payloadOffset = 0;
            pool.Return(imageBuffer);
            imageBuffer = newImageBuffer;
            imageBufferSize = realSize;

            char* newReadBuffer = pool.Rent(newIBufferSize/2, realSize);
            std::copy(readBuffer, readBuffer + readBufferSize, newReadBuffer);
            pool.Return(readBuffer);
            readBuffer = newReadBuffer;
            readBufferSize = realSize;

            newIBufferSize = -1;
        }

        if (imageBufferSize - payloadSize < readBufferSize)
        {
            char* newImageBuffer = pool.Rent(imageBufferSize * 2, realSize);
            std::copy(imageBuffer, imageBuffer + payloadSize, newImageBuffer);
            pool.Return(imageBuffer);
            imageBuffer = newImageBuffer;
            imageBufferSize = realSize;
        }

        if (imageBufferSize - payloadOffset - payloadSize < readBufferSize)
        {
            std::shift_left(imageBuffer, imageBuffer + payloadOffset + payloadSize, imageBufferSize - payloadSize);
            payloadOffset = 0;
        }

        std::copy(readBuffer, readBuffer + readSize, imageBuffer + payloadOffset + payloadSize);
        payloadSize += readSize;

        readAsync = client.ReadAsync(readBuffer, readBufferSize);

        int processOffset = 0;
        bool process = true;
        while (process)
        {
            if (payloadSize - payloadOffset <= 0)
            {
                process = false;
                continue;
            }

            int currentIndex = 0;
            char* processStart = imageBuffer + payloadOffset;
            int processSize = payloadSize;

            currentIndex =
                ArrayExt::FindBytesIndex<char>(
                    processStart,
                    processSize,
                    contentLengthBytes.c_str(),
                    contentLengthBytes.size()
                );

            processSize -= currentIndex;
            if (currentIndex == -1)
            {
                process = false;
                continue;
            }

            currentIndex += contentLengthBytes.size();
            processSize -= contentLengthBytes.size();
            if (currentIndex > payloadSize)
            {
                process = false;
                continue;
            }

            auto endNewLine = 
                ArrayExt::FindBytesIndex<char>(
                    processStart + currentIndex,
                    processSize,
                    newLineBytes.c_str(),
                    newLineBytes.size()
                    );
            if (endNewLine == -1)
            {
                process = false;
                continue;
            }

            auto imageSize = std::strtol(processStart + currentIndex, nullptr, 10);
            if (imageSize < 1000)
            {
                int s = 15;
            }
            currentIndex += endNewLine;
            processSize -= endNewLine;
            if (imageSize * 2 > imageBufferSize)
            {
                newIBufferSize = imageSize * 2;
            }

            if (processSize <= newLineBytes.size() * 2 + carriageReturnSize.size())
            {
                process = false;
                continue;
            }
            else
            {
                currentIndex += newLineBytes.size() * 2 + carriageReturnSize.size();
                processSize -= newLineBytes.size() * 2 + carriageReturnSize.size();
            }

            if (processSize < imageSize)
            {
                process = false;
                continue;
            }

            if (imageSize == processSize)
            {
                payloadSize = 0;
                payloadOffset = 0;
                process = false;
            }
            else
            {
                payloadOffset += imageSize + currentIndex;
                payloadSize -= imageSize + currentIndex;
            }

            std::cout << "Image with size:" << imageSize << std::endl;
            std::cout << std::endl;
            /*std::ofstream stream;
            stream.open(std::format("Images\image{}.jpg", 1), std::ios::app | std::ios::binary);
            if (!stream)
            {
                std::cout << "Fail open file" << std::endl;
            }
            stream.write(processStart + currentIndex, imageSize);
            if (!stream)
            {
                std::cout << "Fail write into the file" << std::endl;
            }
            stream.flush();
            stream.close();*/
        }
    }

    pool.Return(imageBuffer);
    pool.Return(readBuffer);
    pool.Return(lengthImageBuffer);

    return 0;
}