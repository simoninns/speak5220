10 REM SPDX-FileCopyrightText: (c) 2026 Simon Inns
20 REM SPDX-License-Identifier: GPL-3.0-or-later
30 REM SPEAK: speaks the phrases of a Speak5220 disc
40 REM INDEXn lists PHROM n's phrases; PHROMn holds their streams
50 MODE 7
60 ON ERROR GOTO 900
70 A%=&EB:X%=0:Y%=&FF:IF (USR&FFF4 AND &FF00)=0 THEN PRINT"This BBC Micro has no speech system.":END
80 REM A 16 KB buffer below the screen holds the chosen PHROM's streams
90 B%=HIMEM-&4000:IF B%-TOP<&900 THEN PRINT"Not enough memory: PAGE must be &";~B%-&900-TOP+PAGE;" or lower.":END
100 HIMEM=B%
110 DIM C% 150,n%(8),t$(8)
120 PROCassemble
130 REM The PHROMs: INDEX1-4 on drive 0, INDEX5-8 on drive 2
140 FOR I%=1 TO 8:n%(I%)=-1:IF I%<5 OR n%(4)>=0 THEN PROCindex(I%)
150 NEXT
200 REM The PHROM list
210 s%=0:CLS:PROCtitle
220 PRINT"PHROMs on this disc:"'
230 c%=0:FOR I%=1 TO 8:IF n%(I%)>=0 THEN c%=c%+1:PRINT"  ";I%;"  ";t$(I%);" (";n%(I%);")"
240 NEXT
250 IF c%=0 THEN PRINT"  None: the disc has no INDEX files.":END
260 PRINT'"Press the number of a PHROM."
270 k%=GET-48:IF k%<1 OR k%>8 THEN 270
280 IF n%(k%)<0 THEN 270
300 REM The phrase list of PHROM p%, a page of 16 at a time
310 p%=k%:f%=OPENIN(FNfile("INDEX",p%)):s%=1:top%=0
320 CLS:PROCtitle:PRINT"Loading PHROM ";p%:OSCLI"LOAD "+FNfile("PHROM",p%)+" "+STR$~B%
330 CLS:PROCtitle:PRINT"PHROM ";p%;": ";t$(p%)
340 last%=top%+15:IF last%>n%(p%)-1 THEN last%=n%(p%)-1
350 PRINT"Phrases ";top%+1;"-";last%+1;" of ";n%(p%)'
360 FOR I%=top% TO last%:PROCrecord(I%):PRINT"  ";CHR$(65+I%-top%);"  ";r$:NEXT
370 PRINTTAB(0,21)"A-";CHR$(65+last%-top%);" speak  , . page  RETURN play all"'"ESCAPE back to the PHROMs";
380 k%=GET
390 IF k%=13 THEN 500
400 IF (k%=46 OR k%=62) AND top%+16<n%(p%) THEN top%=top%+16:GOTO 330
410 IF (k%=44 OR k%=60) AND top%>0 THEN top%=top%-16:GOTO 330
420 k%=(k% AND &DF)-65:IF k%<0 OR k%>last%-top% THEN 380
430 PROCrecord(top%+k%):PROCsay:GOTO 380
500 REM Play all: every phrase of the PHROM in order
510 s%=2:FOR I%=0 TO n%(p%)-1:PROCrecord(I%):PROCsay:NEXT
520 s%=1:GOTO 330
900 REM ESCAPE stops play all, and goes back from the phrase list
910 IF ERR<>17 THEN CLOSE#0:REPORT:PRINT" at line ";ERL:END
920 IF s%=2 THEN s%=1:GOTO 330
930 IF s%=1 THEN CLOSE#f%
940 GOTO 200
1000 DEF PROCtitle
1010 PRINTCHR$141;CHR$134;"Speak5220"'CHR$141;CHR$134;"Speak5220"
1020 ENDPROC
1100 REM Reads PHROM i%'s title and phrase count from INDEXi%
1110 DEF PROCindex(i%)
1120 LOCAL f%,j%
1130 f%=OPENIN(FNfile("INDEX",i%)):IF f%=0 THEN ENDPROC
1140 t$(i%)="":FOR j%=1 TO 30:t$(i%)=t$(i%)+CHR$BGET#f%:NEXT
1150 t$(i%)=FNtrim(t$(i%)):j%=BGET#f%:n%(i%)=j%+256*BGET#f%:CLOSE#f%
1160 ENDPROC
1200 REM Reads phrase i%'s record: its name, offset and length
1210 DEF PROCrecord(i%)
1220 LOCAL j%
1230 PTR#f%=32+20*i%:r$="":FOR j%=1 TO 16:r$=r$+CHR$BGET#f%:NEXT
1240 r$=FNtrim(r$):o%=BGET#f%:o%=o%+256*BGET#f%:l%=BGET#f%:l%=l%+256*BGET#f%
1250 ENDPROC
1300 REM Speaks the phrase from the buffer
1310 DEF PROCsay
1320 PRINTTAB(0,24)"Speaking ";r$;SPC(16-LEN(r$));
1330 ?&70=(B%+o%) MOD 256:?&71=(B%+o%) DIV 256:?&72=l% MOD 256:?&73=l% DIV 256:CALL C%
1340 PRINTTAB(0,24)SPC(25);
1350 ENDPROC
1400 DEF FNfile(a$,i%)=":"+CHR$(48-2*(i%>4))+"."+a$+STR$(i%)
1500 DEF FNtrim(a$)
1510 LOCAL j%
1520 j%=LEN(a$)
1530 IF j%>0 THEN IF MID$(a$,j%,1)=" " THEN j%=j%-1:GOTO 1530
1540 =LEFT$(a$,j%)
2000 REM Speak External from the buffer (Acorn Speech System User Guide)
2010 REM &70 the next byte, &72 the bytes left, &74 the bytes to send
2020 DEF PROCassemble
2030 LOCAL I%
2040 FOR I%=0 TO 2 STEP 2:P%=C%
2050 [OPT I%
2060 LDY #&60:JSR wr:LDA #16:STA &74:JSR send
2070 .poll LDA &72:ORA &73:BEQ sent:JSR rd:TYA:AND #&40:BEQ poll
2080 LDA #8:STA &74:JSR send:JMP poll
2090 .sent LDA #0:STA &75:STA &76
2100 .start JSR rd:TYA:BMI talk:INC &75:BNE start:INC &76:BNE start:RTS
2110 .talk JSR rd:TYA:BMI talk:RTS
2120 .send LDA &72:ORA &73:BEQ none:LDY #0:LDA (&70),Y:TAY:JSR wr
2130 INC &70:BNE s1:INC &71
2140 .s1 LDA &72:BNE s2:DEC &73
2150 .s2 DEC &72:DEC &74:BNE send
2160 .none RTS
2170 .wr LDA #&9F:JMP &FFF4
2180 .rd LDA #&9E:JMP &FFF4
2190 ]:NEXT
2200 ENDPROC
