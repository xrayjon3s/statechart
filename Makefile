.PHONY: all clean test traffic_light

CXXFLAGS = -std=c++2a -Wall -Wextra

all:
	-mkdir -p build
	cd build && cmake .. && make -j4

clean:
	rm -rf build

test: all
	cd build && ./statechart_test

traffic_light: all
	cd build && g++ $(CXXFLAGS) -I.. -o traffic_light ../traffic_light.cc -pthread
