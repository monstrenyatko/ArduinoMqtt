#ifndef MQTT_CLIENT_SYSTEM_UNIX_H_
#define MQTT_CLIENT_SYSTEM_UNIX_H_

#include <MqttClient.h>

#include <chrono>

class MqttClientSystemUnix: public MqttClient::System {
	unsigned long millis() const {
		return std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::system_clock::now().time_since_epoch()).count();
	}
};

#endif
