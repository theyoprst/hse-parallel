#!/bin/sh
g++ -std=c++17 -I$TBB_INCLUDE seminar_test_func.cpp -L$TBB_LIBRARY_RELEASE -ltbb -lpthread
