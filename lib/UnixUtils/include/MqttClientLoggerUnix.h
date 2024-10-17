#ifndef MQTT_CLIENT_LOGGER_UNIX_H_
#define MQTT_CLIENT_LOGGER_UNIX_H_

#include <iostream>
#include <iomanip>

class MqttClientLoggerUnix {
public:
	void println(const char* v) {
		std::cout<<v<<std::endl;
	}
};

#endif
