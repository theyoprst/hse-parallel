call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
:: set INCLUDE for profile.h which is located in ../include dir.
set INCLUDE=%INCLUDE%;../include
cl.exe sem1.cpp /O2 /std:c++latest
