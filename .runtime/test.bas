10 FOR I=0 TO 31
20 PAPER I
30 PRINT " "
40 NEXT I
45 SAY "hello world"
50 PAPER 0
60 PRINTL
70 PRINTL
80 PRINTL "REBASIC TEST PROGRAM"
90 PRINTL
100 LOADSONG 1,"ICEMAN.MID"
110 PLAYSONG 1
120 PRINT "ENTER TEXT: "
130 A$=INPUT()
140 SAY A$
150 LOADSONG 1,"LARRY.MID"
160 LOADPALETTE "PALETTE.PNG"
170 LOADSPRITE 1,"SPR0.PNG"
180 SPRITE 1,250,70,1
190 PRINTL
200 FOR I=0 TO 31
210 PEN 31 - I
220 PAPER I
230 PRINT A$ + " "
240 NEXT I
250 PRINTL
260 PRINTL
270 FOR X= 10 TO 250
280 SPRITE 1,260 - X,70
290 WAITVBL
300 NEXT X
310 A$=INPUT()
