TARGET=dio-shark.out dio-parse.out
SHARK_OBJ=dio_shark.o
PARSE_OBJ=dio_parse.o

all : $(TARGET)

dio-shark.out: $(SHARK_OBJ)
	gcc -o $@ $< -pthread

dio-parse.out: $(PARSE_OBJ)
	gcc -o $@ $<

%.o : %.c
	gcc -c $<

clean : 
	rm -f $(SHARK_OBJ) $(PARSE_OBJ) $(TARGET)
