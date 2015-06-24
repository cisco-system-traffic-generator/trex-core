GTEST_DIR = ../common
USER_DIR = .
CPPFLAGS += -isystem ../

CXXFLAGS += -g -Wall -Wextra -pthread

TESTS = tuple_gen_test

GTEST_HEADERS = $(GTEST_DIR)/gtest.h

all : $(TESTS)

clean : 
	rm -f $(TESTS) gtest.a gtest_main.a *.o

GTEST_SRCS_ = $(GTEST_HEADERS)

gtest-all.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
		$(GTEST_DIR)/gtest-all.cc

gtest_main.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
		$(GTEST_DIR)/gtest_main.cc

gtest.a : gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

gtest_main.a : gtest-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

tuple_gen.o : ../tuple_gen.cpp ../tuple_gen.h $(GTEST_HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../tuple_gen.cpp

tuple_gen_test.o : ./tuple_gen_test.cpp  \
	../tuple_gen.h $(GTEST_HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ./tuple_gen_test.cpp

tuple_gen_test : tuple_gen.o tuple_gen_test.o gtest_main.a
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -lpthread $^ -o $@
