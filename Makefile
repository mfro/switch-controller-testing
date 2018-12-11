.PHONY = clean
CXXDEBUG = -Wall -std=c++17 -ggdb
CXXFLAGS = -Wall -std=c++17 -O3

debug/%.o: %.cc *.h
	$(CXX) $(CXXDEBUG) $< -c -o $@

release/%.o: %.cc *.h
	$(CXX) $(CXXFLAGS) $< -c -o $@
	
debug/main: $(patsubst %.cc,debug/%.o,$(wildcard *.cc)) *.h
	$(CXX) $(CXXDEBUG) -o $@ debug/*.o -lbluetooth -pthread -std=c++17
	
release/main: $(patsubst %.cc,release/%.o,$(wildcard *.cc)) *.h
	$(CXX) $(CXXFLAGS) -o $@ release/*.o -lbluetooth -pthread -std=c++17

clean:
	rm debug/* release/*
