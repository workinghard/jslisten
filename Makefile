CFLAGS 		?= -g -O2 -Wall -std=c99

PROGRAMS	= jslisten


compile: $(PROGRAMS)

jslisten.o: src/jslisten.c src/axbtnmap.h src/minIni.h

jslisten: src/jslisten.o src/axbtnmap.o src/minIni.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ -ludev -o bin/$@

clean:
	$(RM) src/*.o src/*.swp bin/$(PROGRAMS) src/*.orig src/*.rej map *~

.PHONY: compile clean
