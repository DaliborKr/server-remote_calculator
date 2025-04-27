CC = g++

TARGET = ipkcpd

all: $(TARGET)

$(TARGET): $(TARGET).cpp
	$(CC) -g -Wall -o $(TARGET) $(TARGET).cpp

clean:
	rm $(TARGET)

cliudp:
	./ipkcpc -h 0.0.0.0 -p 2023 -m udp

clitcp:
	./ipkcpc -h 0.0.0.0 -p 2023 -m tcp

servudp:
	./ipkcpd -h 0.0.0.0 -p 2023 -m udp

servtcp:
	./ipkcpd -h 0.0.0.0 -p 2023 -m tcp