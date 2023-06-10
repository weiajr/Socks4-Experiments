#include <WiFi.h>

const size_t m_socket_limit = 4;

WiFiServer server;

uint8_t m_deny[8] = { 0, 0x5b, 0, 0, 0, 0, 0, 0 };
uint8_t m_accept[8] = { 0, 0x5a, 0, 0, 0, 0, 0, 0 };

typedef enum
{
	PROXY_AWAIT_IDENTIFY,
	PROXY_OPENING_BRIDGE,
	PROXY_FORWARDING
} proxy_stage_t;

typedef struct proxy_client
{
	bool occupied;
	proxy_stage_t state;
	WiFiClient from;
	WiFiClient dest;
};

proxy_client m_clients[m_socket_limit];
uint8_t* m_buffer;
size_t m_buffer_size = 1024 * 4;

void setup()
{
	m_buffer = (uint8_t*) malloc(m_buffer_size);

	Serial.begin(115200);

	WiFi.begin("", "");
	Serial.println(F("Connecting"));
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(F("."));
		delay(1000);
	}

	Serial.println(F("\nConnected to the WiFi network"));
	Serial.print(F("Local ESP32 IP: "));
	Serial.println(WiFi.localIP());

	server.begin(1080);
	server.setNoDelay(true);
}

size_t find_open_client()
{
	for (size_t i = 0; i < m_socket_limit; i++)
	{
		if (!m_clients[i].occupied) return i;
	}

	return -1;
}

void forwardData(WiFiClient &source, WiFiClient &destination)
{
	size_t availableBytes = source.available();
	if (availableBytes)
	{
		size_t bytesRead = source.read(m_buffer, min(availableBytes, m_buffer_size));
		destination.write(m_buffer, bytesRead);
	}
}

void loop()
{
	long start = millis();

	size_t clientIndex = find_open_client();
	if (server.hasClient() && clientIndex != -1)
	{
		m_clients[clientIndex].occupied = true;
		m_clients[clientIndex].from = server.accept();
		m_clients[clientIndex].state = PROXY_AWAIT_IDENTIFY;
	}

	for (size_t i = 0; i < m_socket_limit; i++)
	{
		if (!m_clients[i].occupied) continue;

		if (m_clients[i].state == PROXY_AWAIT_IDENTIFY && m_clients[i].from.available())
		{
			size_t bytesRead = m_clients[i].from.read(m_buffer, m_buffer_size);
			if (bytesRead == 0) continue;
			if (m_buffer[0] != 0x04 || m_buffer[1] != 0x1 || bytesRead < 9)
			{
				m_clients[i].occupied = false;
				m_clients[i].from.write(m_deny, sizeof(m_deny));
				m_clients[i].from.stop();
				continue;
			}

			m_clients[i].from.write(m_accept, sizeof(m_accept));
			m_clients[i].dest.connect(IPAddress(m_buffer[4] + (m_buffer[5] << 8) + (m_buffer[6] << 16) + (m_buffer[7] << 24)), ntohs(m_buffer[2] + (m_buffer[3] << 8)));
			m_clients[i].state = PROXY_OPENING_BRIDGE;
		}

		if (m_clients[i].state == PROXY_OPENING_BRIDGE && m_clients[i].dest.connected())
            m_clients[i].state = PROXY_FORWARDING;

		if (m_clients[i].state == PROXY_FORWARDING)
		{
			bool bridgeOpen = m_clients[i].from.connected() && m_clients[i].dest.connected();
			if (!bridgeOpen)
			{
				m_clients[i].occupied = false;
				m_clients[i].from.stop();
				m_clients[i].dest.stop();
				continue;
			}

			forwardData(m_clients[i].from, m_clients[i].dest);
			forwardData(m_clients[i].dest, m_clients[i].from);
		}
	}

	// Serial.print(F("loop took "));
	// Serial.print(millis() - start);
	// Serial.println(F("ms"));
}