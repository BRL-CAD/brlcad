CXXFLAGS = -std=c++11 -Werror -Wall -Wextra -Weffc++ -Wpedantic -Wundef -Wfloat-equal -Wshadow -Winline -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wsign-conversion -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wunreachable-code -Wno-missing-braces

CXXFLAGS += -isystem other

LDFLAGS += -framework OpenCL

LD = $(CXX)

CXXFLAGS += -g -DDEBUG_BUILD


OBJS = main.o clhost.o


.PHONY: clean


trt: $(OBJS)
	$(CXX) -o trt $(OBJS) $(LDFLAGS)


clean:
	rm -f trt $(OBJS)


main.o: main.cpp clhost.hpp raytracer.hpp
clhost.o: clhost.cpp clhost.hpp raytracer.hpp


