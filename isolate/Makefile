CXXFLAGS := -std=c++11
isolate : main.cpp
	$(CXX) $(CXXFLAGS) $^ -o main
	witch mv main $@
	witch chown root:root $@
	witch chmod +s $@
