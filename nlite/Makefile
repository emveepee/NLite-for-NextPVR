XX = clang++
SDL = -L /usr/lib/i386-linux-gnu/ -lcurl -lvlc -lSDL2 -lSDL2_image -lSDL2_ttf
# If your compiler is a bit older you may need to change -std=c++11 to -std=c++0x
CXXFLAGS = -Wall -c -std=c++11 -I /usr/include/SDL2
LDFLAGS = $(SDL)
EXE = nlite

OBJS = UserInterface.o CurlHelper.o MD5.o tinyxml2.o Utility.o Player.o NPVRSession.o
O_FILES = $(SRC_FILES:%.cpp=%o)

all: $(EXE)

$(EXE): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

.cpp.o: 
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm *.o && rm $(EXE)
