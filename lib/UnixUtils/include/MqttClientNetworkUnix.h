#ifndef MQTT_CLIENT_NETWORK_UNIX_H_
#define MQTT_CLIENT_NETWORK_UNIX_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <iostream>
#include <chrono>

class MqttClientNetworkUnix {

public:
	MqttClientNetworkUnix(const char* host, int port = 1883) {
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			std::cerr << "Can't open socket" << std::endl;
			exit(-1);
		}

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;
		if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
			std::cerr << "Can't set socket RECV timeout" << std::endl;
			exit(-1);
		}
		if (setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
			std::cerr << "Can't set socket SEND timeout" << std::endl;
			exit(-1);
		}

		std::cout << "Connecting to " << host << ":" << port << std::endl;

		hostent *server = gethostbyname(host);
		if (server == NULL) {
			std::cerr << "No such host" << std::endl;
			exit(-1);
		}

		sockaddr_in serv_addr;
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr,
			(char *)&serv_addr.sin_addr.s_addr,
			server->h_length
		);
		serv_addr.sin_port = htons(port);

		auto const startTime = std::chrono::system_clock::now();
		while(connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
			if (std::chrono::system_clock::now() - startTime > std::chrono::seconds(30))
			{
				std::cerr << "Connect time-out, error: " << std::endl;
				exit(-1);
			}

			if (errno == EINPROGRESS || errno == EALREADY)
			{
				continue;
			}

			std::cerr << "Can't connect, error: " << errno  << std::endl;
			exit(-1);
		}
	}

	~MqttClientNetworkUnix() {
		close(sockfd);
	}

	int read(unsigned char* buffer, int len, unsigned long timeoutMs) {
		return ::read(sockfd, buffer, len);
	}

	int write(unsigned char* buffer, int len, unsigned long timeoutMs) {
		return ::write(sockfd, buffer, len);
	}

private:
	int sockfd = -1;
};

#endif
