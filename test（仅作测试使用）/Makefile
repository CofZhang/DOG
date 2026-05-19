# Makefile for motor control program

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lm
TARGET = control_12motors
SOURCES = motor_protocol.c control_12motors.c
HEADERS = motor_protocol.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
	rm -f $(TARGET).exe

