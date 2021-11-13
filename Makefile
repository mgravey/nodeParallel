UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    OS =linux
    OPENCL_LIB+= -lOpenCL -lclFFT
endif
ifeq ($(UNAME_S),Darwin)
    OS =mac
    OPENCL_LIB+= -framework OpenCL -lclFFT
endif

export OS


export CFLAGS=-O3 -std=c++11 -Wall -Wno-unused
export CXXFLAGS= #-DDEBUG=1
export SRCS=./src
export INC=-I./include
export LIBINC=-I/usr/include -I/opt/local/include
export LDFLAGS= -lzmq
export LIB_PATH= -L/usr/lib -L/usr/lib/x86_64-linux-gnu/ -L/opt/local/lib -L/usr/local/lib/

DEFAULT_JOB= np_server np_client


.PHONY: clean

.DEFAULT: $(DEFAULT_JOB)

default: $(DEFAULT_JOB)

%.o: $(SRCS)/%.cpp 
	$(CXX) -c -o $@ $< $(CFLAGS) $(CXXFLAGS) $(INC) $(LIBINC)

%: %.o 
	$(CXX) -o $@ $< $(LIB_PATH) $(LDFLAGS)

np_server:server
	mv $< $@
np_client:client
	mv $< $@

clean:
	rm -rf *.o server client $(DEFAULT_JOB)
