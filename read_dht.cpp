#include <wiringPi.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <string.h>

void set_max_priority(void)
{
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	// Use FIFO scheduler with highest priority for the lowest chance of the kernel context switching.
	sched.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &sched);
}

void set_default_priority(void)
{
	struct sched_param sched;
	memset(&sched, 0, sizeof(sched));
	// Go back to default scheduler with default 0 priority.
	sched.sched_priority = 0;
	sched_setscheduler(0, SCHED_OTHER, &sched);
}

#define DHT11 11
#define DHT22 22
#define AM2302 22

bool read_dht(int type, int pin, float &humidity, float &temperature)
{
	const int DHT_MAXCOUNT = 32000;
	const int DHT_PULSES = 41;

	pinMode(pin, OUTPUT);

	set_max_priority();

	digitalWrite(pin, HIGH);
	delay(100);

	digitalWrite(pin, LOW);
	delay(10);
//	delayMicroseconds(2000);

	digitalWrite(pin, HIGH);
	pinMode(pin, INPUT);

	bool timeouted = false;
	{
		int count = 0;
		while (digitalRead(pin))
		{
			if (++count >= DHT_MAXCOUNT)
			{
				set_default_priority();
				fprintf(stderr, "timeout: -1\n");
				timeouted = true;
				break;
			}
			//			delayMicroseconds(1);
		}
	}

	int pulseCounts[DHT_PULSES * 2] = {0};
	for (int i = 0;  !timeouted && i < DHT_PULSES; ++i)
	{
		while (!timeouted && !digitalRead(pin))
		{
			if (++pulseCounts[i * 2 + 0] > DHT_MAXCOUNT)
			{
				set_default_priority();
				fprintf(stderr, "timeout: %d\n", i * 2 + 0);
				timeouted = true;
				break;
			}
			//			delayMicroseconds(1);
		}
		while (!timeouted && digitalRead(pin))
		{
			if (++pulseCounts[i * 2 + 1] > DHT_MAXCOUNT)
			{
				set_default_priority();
				fprintf(stderr, "timeout: %d\n", i * 2 + 1);
				timeouted = true;
				break;
			}
			//			delayMicroseconds(1);
		}
	}

	set_default_priority();
	// time critical parts end here

	int threshold = 0;
	for (int i = 2; i < DHT_PULSES * 2; i += 2)
		threshold += pulseCounts[i];
	threshold /= DHT_PULSES - 1;

	uint8_t data[5] = {0};
	for (int i = 3; i < DHT_PULSES * 2; i += 2)
	{
		const int index = (i - 3) / 16;
		data[index] <<= 1;
		if (pulseCounts[i] >= threshold)
			data[index] |= 1;
	}

#if 1
	// Useful debug info:
	{
		fprintf(stderr, "debug :");
		for (int i = 0; i < DHT_PULSES; ++i)
			fprintf(stderr, "(%d,%d)%s", pulseCounts[i*2 + 0], pulseCounts[i*2 + 1], (i % 8) ? "" : "\ndebug :");
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "debug: threshold = %d\n", threshold);
	fprintf(stderr, "debug: Data: 0x%x 0x%x 0x%x 0x%x 0x%x\n", data[0], data[1], data[2], data[3], data[4]);
	fprintf(stderr, "debug: checksum 0x%x\n", (data[0] + data[1] + data[2] + data[3]) & 0xff);
#endif

	if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
	{
		fprintf(stderr, "checksum error\n");
		return false;
	}

	if (type == DHT11)
	{
		humidity = (float)data[0];
		temperature = (float)data[2];
	}
	else if (type == DHT22)
	{
		humidity = (data[0] * 256 + data[1]) / 10.0f;
		temperature = ((data[2] & 0x7F) * 256 + data[3]) / 10.0f;
		if (data[2] & 0x80)
		{
			temperature *= -1.0f;
		}
	}
	return true;
}

void usage()
{
	fprintf(stderr, "usage: read_dht [--dump] type(11 or 22) bcm_pin\n");
}

int main(int argc, char *argv[])
{
	int type = 0;
	int bcmpin = 0;
	bool is_dump = false;

	if (argc < 3)
	{
		usage();
		return 1;
	}

	int c = 1;
	if (strcmp(argv[c], "--dump") == 0)
	{
		is_dump = true;
		c++;
	}

	type = strtol(argv[c++], NULL, 0);
	bcmpin = strtol(argv[c++], NULL, 0);

	if (type == 0 || bcmpin == 0)
	{
		usage();
		return 1;
	}

	fprintf(stderr, "type = %d, bcmpin = %d, is_dump = %d\n", type, bcmpin, is_dump);

	if (wiringPiSetupGpio() == -1)
		return 1;

	for (int i = 0; i < 10; ++i)
	{
		float humidity, temperature;

		if (!read_dht(type, bcmpin, humidity, temperature))
			fprintf(stderr, "fail\n");
		else
		{
			if (is_dump)
				printf("%f %f\n", temperature, humidity);
			else
				printf("%f *c, %f %%\n", temperature, humidity);
			return 0;
		}

		delay(2000);
	}

	fprintf(stderr, "retry fail\n");
	return 1;
}
