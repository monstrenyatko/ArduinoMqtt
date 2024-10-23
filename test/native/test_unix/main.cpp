#include <unity.h>
#include <string.h>
#include <memory>
#include <vector>
#include <chrono>

// Enable MqttClient logs
#define MQTT_LOG_ENABLED 1
// Include library
#include <MqttClient.h>

#include <MqttClientSystemUnix.h>
#include <MqttClientLoggerUnix.h>
#include <MqttClientNetworkUnix.h>

#define MQTT_ID "TEST-ID"
const char *MQTT_TOPIC_SUB = "test/" MQTT_ID "/sub";
static constexpr char const *kSomeMqttId = "SOME_MQTT_ID";
static constexpr char const *kPubPayload = "Test Message !!!";
static std::vector<MqttClient::QoS> const kAllQoS{MqttClient::QOS0, MqttClient::QOS1, MqttClient::QOS2};

struct RecvMessage
{
	std::string topic;
	MqttClient::QoS qos;
	std::string payload;
};

std::vector<RecvMessage> gRecvMessages;

void processMessage(MqttClient::MessageData &md)
{
	std::string topic = md.topicName.cstring ? std::string(md.topicName.cstring) : std::string(md.topicName.lenstring.data, md.topicName.lenstring.len);
	RecvMessage msg{std::move(topic), md.message.qos, std::string(static_cast<char *>(md.message.payload), md.message.payloadLen)};
	std::cout << "Message arrived: qos " << msg.qos << ", payload: [" << msg.payload << "]" << std::endl;
	gRecvMessages.push_back(std::move(msg));
}

struct TestContext
{
	std::unique_ptr<MqttClientSystemUnix> system;
	std::unique_ptr<MqttClientLoggerUnix> logger;
	std::unique_ptr<MqttClientNetworkUnix> network;

	std::unique_ptr<MqttClient::Logger> mqttLogger;
	std::unique_ptr<MqttClient::Network> mqttNetwork;
	std::unique_ptr<MqttClient::Buffer> mqttSendBuffer;
	std::unique_ptr<MqttClient::Buffer> mqttRecvBuffer;
	std::unique_ptr<MqttClient::MessageHandlers> mqttMessageHandlers;
	MqttClient::Options mqttOptions;
};

static std::unique_ptr<TestContext> createContext()
{
	auto context = std::make_unique<TestContext>();

	context->system = std::make_unique<MqttClientSystemUnix>();
	context->logger = std::make_unique<MqttClientLoggerUnix>();
	context->network = std::make_unique<MqttClientNetworkUnix>("localhost");

	context->mqttLogger = std::make_unique<MqttClient::LoggerImpl<MqttClientLoggerUnix>>(*context->logger);
	context->mqttNetwork = std::make_unique<MqttClient::NetworkImpl<MqttClientNetworkUnix>>(*context->network, *context->system);
	context->mqttSendBuffer = std::make_unique<MqttClient::ArrayBuffer<128>>();
	context->mqttRecvBuffer = std::make_unique<MqttClient::ArrayBuffer<128>>();
	context->mqttMessageHandlers = std::make_unique<MqttClient::MessageHandlersImpl<1>>();

	context->mqttOptions.commandTimeoutMs = 5000;

	return context;
}

static std::unique_ptr<MqttClient> createMqttClient(TestContext &context)
{
	return std::make_unique<MqttClient>(context.mqttOptions, *context.mqttLogger, *context.system, *context.mqttNetwork,
										*context.mqttSendBuffer, *context.mqttRecvBuffer, *context.mqttMessageHandlers);
}

static void waitMessage(MqttClient &mqtt)
{
	auto const startTime = std::chrono::system_clock::now();
	do
	{
		mqtt.yield(10);
		if (!gRecvMessages.empty())
			break;
	} while (std::chrono::system_clock::now() - startTime < std::chrono::seconds(5));
}

static MqttClient::Error::type publishBy(std::string const &clientId, std::string const &topic, MqttClient::QoS const qos, std::string const &payload)
{
	std::string const kLogPrefix = "Publish by [" + clientId + "] : ";

	std::cout << kLogPrefix << "begin" << std::endl;

	auto context = createContext();
	auto mqtt = createMqttClient(*context);

	MqttClient::ConnectResult connectResult;
	MQTTPacket_connectData connectOptions = MQTTPacket_connectData_initializer;
	connectOptions.MQTTVersion = 4;
	connectOptions.clientID.cstring = const_cast<char *>(clientId.c_str());
	connectOptions.cleansession = true;
	connectOptions.keepAliveInterval = 10;

	auto rc = mqtt->connect(connectOptions, connectResult);
	if (MqttClient::Error::SUCCESS != rc)
	{
		std::cout << kLogPrefix << "connect failed" << std::endl;
	}

	if (MqttClient::Error::SUCCESS == rc)
	{
		MqttClient::Message message;
		message.qos = qos;
		message.retained = false;
		message.dup = false;
		message.payload = const_cast<char *>(payload.c_str());
		message.payloadLen = payload.length();

		rc = mqtt->publish(topic.c_str(), message);
		if (MqttClient::Error::SUCCESS != rc)
		{
			std::cout << kLogPrefix << "publish failed" << std::endl;
		}
	}

	mqtt->disconnect();

	std::cout << kLogPrefix << "end" << std::endl;
	return rc;
}

void setUp(void)
{
	std::cout << "\n------" << std::endl;
	gRecvMessages.clear();
}

void tearDown(void) {}

void test_Connect()
{
	auto context = createContext();
	auto mqtt = createMqttClient(*context);

	MQTTPacket_connectData connectOptions = MQTTPacket_connectData_initializer;
	connectOptions.MQTTVersion = 4;
	connectOptions.clientID.cstring = (char *)MQTT_ID;
	connectOptions.cleansession = true;
	connectOptions.keepAliveInterval = 10;

	MqttClient::ConnectResult connectResult;
	TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->connect(connectOptions, connectResult));
	TEST_ASSERT_FALSE(connectResult.sessionPresent);

	TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->disconnect());
}

void test_SubUnsub()
{
	auto context = createContext();
	auto mqtt = createMqttClient(*context);

	context->mqttMessageHandlers->set(MQTT_TOPIC_SUB, processMessage);

	MQTTPacket_connectData connectOptions = MQTTPacket_connectData_initializer;
	connectOptions.MQTTVersion = 4;
	connectOptions.clientID.cstring = (char *)MQTT_ID;
	connectOptions.cleansession = true;
	connectOptions.keepAliveInterval = 10;

	MqttClient::ConnectResult connectResult;
	TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->connect(connectOptions, connectResult));

	for (auto const qos : kAllQoS)
	{
		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->subscribe(MQTT_TOPIC_SUB, qos));
		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, publishBy(kSomeMqttId, MQTT_TOPIC_SUB, qos, kPubPayload));

		waitMessage(*mqtt);

		TEST_ASSERT_EQUAL(1, gRecvMessages.size());
		TEST_ASSERT_EQUAL(qos, gRecvMessages[0].qos);
		TEST_ASSERT_EQUAL_STRING(MQTT_TOPIC_SUB, gRecvMessages[0].topic.c_str());
		TEST_ASSERT_EQUAL_STRING(kPubPayload, gRecvMessages[0].payload.c_str());

		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->unsubscribe(MQTT_TOPIC_SUB));
		gRecvMessages.clear();
	}

	TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->disconnect());
}

void test_ReuseSession()
{
	auto const kQos = MqttClient::QOS0;

	MQTTPacket_connectData connectOptions = MQTTPacket_connectData_initializer;
	connectOptions.MQTTVersion = 4;
	connectOptions.clientID.cstring = (char *)MQTT_ID;
	connectOptions.cleansession = false;
	connectOptions.keepAliveInterval = 10;

	// Init session
	{
		auto context = createContext();
		auto mqtt = createMqttClient(*context);

		MqttClient::ConnectResult connectResult;
		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->connect(connectOptions, connectResult));
		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->subscribe(MQTT_TOPIC_SUB, kQos));
		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->disconnect());
	}

	// Get message using session existing subscription
	{
		auto context = createContext();
		auto mqtt = createMqttClient(*context);

		context->mqttMessageHandlers->set(MQTT_TOPIC_SUB, processMessage);

		MqttClient::ConnectResult connectResult;
		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->connect(connectOptions, connectResult));
		TEST_ASSERT_TRUE(connectResult.sessionPresent);

		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, publishBy(kSomeMqttId, MQTT_TOPIC_SUB, kQos, kPubPayload));

		waitMessage(*mqtt);

		TEST_ASSERT_EQUAL(1, gRecvMessages.size());
		TEST_ASSERT_EQUAL(kQos, gRecvMessages[0].qos);
		TEST_ASSERT_EQUAL_STRING(MQTT_TOPIC_SUB, gRecvMessages[0].topic.c_str());
		TEST_ASSERT_EQUAL_STRING(kPubPayload, gRecvMessages[0].payload.c_str());

		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqtt->disconnect());
	}
}

void test_SetWill()
{
	static constexpr char const *kMqttIdA = "MQTT_ID_A";
	static constexpr char const *kMqttIdB = "MQTT_ID_B";
	static constexpr char const *kWillTopic = "test/MQTT_ID_A/will";
	static constexpr char const *kWillPayload = "Test Will Message !!!";
	static constexpr auto kWillQos = MqttClient::QOS0;

	auto contextA = createContext();
	auto mqttA = createMqttClient(*contextA);
	{
		MQTTPacket_connectData connectOptions = MQTTPacket_connectData_initializer;
		connectOptions.MQTTVersion = 4;
		connectOptions.clientID.cstring = const_cast<char *>(kMqttIdA);
		connectOptions.cleansession = true;
		connectOptions.keepAliveInterval = 10;
		connectOptions.willFlag = true;
		connectOptions.will.topicName.cstring = const_cast<char *>(kWillTopic);
		connectOptions.will.message.cstring = const_cast<char *>(kWillPayload);
		connectOptions.will.retained = false;
		connectOptions.will.qos = kWillQos;

		MqttClient::ConnectResult connectResult;
		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqttA->connect(connectOptions, connectResult));
	}

	auto contextB = createContext();
	auto mqttB = createMqttClient(*contextB);
	{
		MQTTPacket_connectData connectOptions = MQTTPacket_connectData_initializer;
		connectOptions.MQTTVersion = 4;
		connectOptions.clientID.cstring = const_cast<char *>(kMqttIdB);
		connectOptions.cleansession = true;
		connectOptions.keepAliveInterval = 10;

		MqttClient::ConnectResult connectResult;
		TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqttB->connect(connectOptions, connectResult));
	}

	contextB->mqttMessageHandlers->set(kWillTopic, processMessage);
	TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqttB->subscribe(kWillTopic, kWillQos));

	// Disconnect clientA with no disconnect call
	mqttA.reset();
	contextA.reset();

	waitMessage(*mqttB);
	TEST_ASSERT_EQUAL(1, gRecvMessages.size());
	TEST_ASSERT_EQUAL(kWillQos, gRecvMessages[0].qos);
	TEST_ASSERT_EQUAL_STRING(kWillTopic, gRecvMessages[0].topic.c_str());
	TEST_ASSERT_EQUAL_STRING(kWillPayload, gRecvMessages[0].payload.c_str());

	TEST_ASSERT_EQUAL(MqttClient::Error::SUCCESS, mqttB->disconnect());
}

int main(int argc, char **argv)
{
	UNITY_BEGIN();

	RUN_TEST(test_Connect);
	RUN_TEST(test_SubUnsub);
	RUN_TEST(test_ReuseSession);
	RUN_TEST(test_SetWill);

	UNITY_END();
}
