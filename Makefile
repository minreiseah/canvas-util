default: build

buildfiles: cleanfiles
	g++ downloadFiles.cpp -Wall -o downloadFiles -lcurl -ljsoncpp

cleanfiles:
	rm -rf downloadFiles

files: buildfiles
	./downloadFiles

buildvideos: cleanvideos
	g++ downloadVideos.cpp -Wall -o downloadVideos -lcurl -ljsoncpp

cleanvideos:
	rm -rf downloadVideos

videos: buildvideos
	./downloadVideos

run: buildfiles buildvideos
	./downloadFiles
	./downloadVideos
