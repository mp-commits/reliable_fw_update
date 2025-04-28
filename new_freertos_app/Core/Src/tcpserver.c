#include "lwip/sockets.h"
#include "lwip/inet.h"

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "tcpserver.h"

err_t tcp_process(void)
{
    static int sock = -1;
    static bool connActive = false;
    static char rxBuf[256];
    static char txBuf[1024];

    err_t err = ERR_OK;

    if (sock < 0)
    {
        printf("Creating new socket\r\n");

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("socket");
            return ERR_MEM;
        }

        // Set non-blocking for connect timeout
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(12345);
        inet_aton("192.168.1.100", &server_addr.sin_addr);

        printf("Trying to connect to 192.168.1.100:12345\r\n");

        int ret = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (ret < 0)
        {
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(sock, &writefds);

            struct timeval timeout;
            timeout.tv_sec = 2;  // 2 second timeout
            timeout.tv_usec = 0;

            ret = select(sock + 1, NULL, &writefds, NULL, &timeout);
            if (ret > 0)
            {
                int so_error;
                socklen_t len = sizeof(so_error);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
                if (so_error == 0)
                {
                    printf("Successfully connected!\r\n");
                    connActive = true;
                    err = ERR_OK;
                }
                else
                {
                    printf("Connect failed: %d\r\n", so_error);
                    close(sock);
                    sock = -1;
                    return ERR_CONN;
                }
            }
            else
            {
                printf("Connect timeout or error\r\n");
                close(sock);
                sock = -1;
                return ERR_TIMEOUT;
            }
        }
        else
        {
            // Immediate connect (rare for remote hosts)
            printf("Connected immediately!\r\n");
            connActive = true;
        }

        // Set back to blocking mode for simplicity
        fcntl(sock, F_SETFL, flags);

        // Set recv timeout
        struct timeval recv_timeout = { .tv_sec = 1, .tv_usec = 0 }; // 1s
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
    }

    if (connActive)
    {
        ssize_t bytes = recv(sock, rxBuf, sizeof(rxBuf) - 1, 0);
        if (bytes > 0)
        {
            rxBuf[bytes] = '\0'; // Null-terminate
            printf("Received %u bytes\r\n", (unsigned)bytes);

            snprintf(txBuf, sizeof(txBuf), "Response to \"%s\"\n", rxBuf);
            send(sock, txBuf, strlen(txBuf), 0);
        }
        else if (bytes == 0)
        {
            printf("Connection closed by server\r\n");
            close(sock);
            sock = -1;
            connActive = false;
            err = ERR_CLSD;
        }
        else
        {
            // recv() error
            int error = errno;
            if (error == EWOULDBLOCK || error == EAGAIN)
            {
                // Timeout, no data
            }
            else
            {
                printf("recv() error: %d\r\n", error);
                close(sock);
                sock = -1;
                connActive = false;
                err = ERR_RST;
            }
        }
    }

    return err;
}