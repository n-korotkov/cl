mkdir obj\tasks
@ set cc=cl /EHsc /std:c++latest /O2 /I:src\ /nologo
%cc% /Fo:obj\ /c src\*.cpp
%cc% /Fo:obj\tasks\ /c src\tasks\*.cpp
@ for %%t in (obj\tasks\*) do %cc% /Fe:"%%~nt.exe" obj\*.obj "%%t"
