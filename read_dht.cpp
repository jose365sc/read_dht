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
	set_max_priority();

	pinMode(pin, OUTPUT);
	digitalWrite(pin, LOW);
	delay(18);

	pinMode(pin, INPUT);
	digitalWrite(pin, HIGH);

    uint16_t rawHumidity = 0;
    uint16_t rawTemperature = 0;
    uint16_t data = 0;

    for ( int i = -3 ; i < 2 * 40; i++ ) {
		unsigned int age;
		unsigned int startTime = micros();
		do {
			age = (unsigned long)(micros() - startTime);
			if ( age > 90 ) {
				fprintf(stderr, "timeout: %d\n", i);
                return false;
  		    }
		} while ( digitalRead(pin) == (i & 1) ? HIGH : LOW );

	    if ( i >= 0 && (i & 1) ) {
      		data <<= 1;
      		if ( age > 30 ) {
        		data |= 1;
      		}
	    }

    	switch ( i ) {
		case 31:
			rawHumidity = data;
        	break;
		case 63:
			rawTemperature = data;
			data = 0;
			break;
    	}
	}
	set_default_priority();

	if ( (uint8_t)(((uint8_t)rawHumidity) + (rawHumidity >> 8) + ((uint8_t)rawTemperature) + (rawTemperature >> 8)) != data ) {
		fprintf(stderr, "checksum error\n");
		return false;
	}

 	// Store readings
	if ( type == DHT11 ) {
		humidity = rawHumidity >> 8;
		temperature = rawTemperature >> 8;
	}
	else
	{
		humidity = rawHumidity * 0.1;
		if ( rawTemperature & 0x8000 )
		{
		    rawTemperature = -(int16_t)(rawTemperature & 0x7FFF);
	    }
	    temperature = ((int16_t)rawTemperature) * 0.1;
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
