default: build

build: clean
	g++ main.cpp -Wall -o main -lcurl -ljsoncpp

clean:
	rm -rf main

run: build
	./main
