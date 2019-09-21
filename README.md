# rebasic
Simple fantasy console. This is just a silly little weekend project, so don't expect too much. Twitter thread about it here: https://twitter.com/Mattias_G/status/1175111513725648896


Build instructions
------------------

From a Visual Studio command prompt, do:

     cl source\*.cpp /Fe.runtime\rebasic.exe
    
To run test program do:

    cd .runtime
    rebasic test.bas
    
Note that it takes a while before the music can be heard, as the midi song starts with some silence.
