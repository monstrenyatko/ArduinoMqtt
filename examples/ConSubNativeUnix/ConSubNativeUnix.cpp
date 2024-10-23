#include <string.h>
#include <iostream>

// Enable MqttClient logs
#define MQTT_LOG_ENABLED 1
// Include library
#include <MqttClient.h>

#include <MqttClientSystemUnix.h>
#include <MqttClientLoggerUnix.h>
#include <MqttClientNetworkUnix.h>

#define MQTT_ID "TEST-ID"
const char* MQTT_TOPIC_SUB = "test/" MQTT_ID "/sub";

void processMessage(MqttClient::MessageData& md) {
	const MqttClient::Message& msg = md.message;
	char payload[msg.payloadLen + 1];
	memcpy(payload, msg.payload, msg.payloadLen);
	payload[msg.payloadLen] = '\0';
	std::cout << "Message arrived: qos " << msg.qos << ", payload: [" << payload << "]" << std::endl;
}

int main(int argc, char** argv) {
	// Init
	MqttClientSystemUnix mqttSystem;
	MqttClientLoggerUnix mqttLoggerUnix;
	MqttClientNetworkUnix testNetwork("test.mosquitto.org");
	MqttClient::Logger *mqttLogger = new MqttClient::LoggerImpl(mqttLoggerUnix);
	MqttClient::Network *mqttNetwork = new MqttClient::NetworkImpl(testNetwork, mqttSystem);
	MqttClient::Buffer *mqttSendBuffer = new MqttClient::ArrayBuffer<128>();
	MqttClient::Buffer *mqttRecvBuffer = new MqttClient::ArrayBuffer<128>();
	MqttClient::MessageHandlers *mqttMessageHandlers = new MqttClient::MessageHandlersImpl<1>();
	MqttClient::Options options;
	options.commandTimeoutMs = 5000; // Set command timeout to 5 seconds
	MqttClient *mqtt = new MqttClient (options, *mqttLogger, mqttSystem, *mqttNetwork, *mqttSendBuffer, *mqttRecvBuffer, *mqttMessageHandlers);

	// Setup message handlers
	mqttMessageHandlers->set(MQTT_TOPIC_SUB, processMessage);

	// Connect
	MqttClient::ConnectResult connectResult;
	MQTTPacket_connectData connectOptions = MQTTPacket_connectData_initializer;
	connectOptions.MQTTVersion = 4;
	connectOptions.clientID.cstring = (char*)MQTT_ID;
	connectOptions.cleansession = true;
	connectOptions.keepAliveInterval = 10; // 10 seconds
	MqttClient::Error::type rc = mqtt->connect(connectOptions, connectResult);
	if (rc != MqttClient::Error::SUCCESS) {
		std::cout << "Connection error: " << rc << std::endl;
	}

	// Subscribe
	{
		MqttClient::Error::type rc = mqtt->subscribe(MQTT_TOPIC_SUB, MqttClient::QOS1);
		if (rc != MqttClient::Error::SUCCESS) {
			std::cerr << "Subscribe error: " << rc << std::endl;
			std::cerr << "Drop connection" << std::endl;
			mqtt->disconnect();
			return -1;
		}
	}

	// Rest and process keep-alives for 20 seconds
	mqtt->yield(20000);

	// Disconnect
	mqtt->disconnect();

	return 0;
}
