TARGET=dioshark dioparse
SHARK_OBJ=dio_shark.o
PARSE_OBJ=dio_parse.o rbtree.o

all : $(TARGET)

dioshark: $(SHARK_OBJ)
	gcc -o $@ $< -pthread

dioparse: $(PARSE_OBJ)
	gcc -o $@ $^

%.o : %.c
	gcc -c $<

clean : 
	rm -f $(SHARK_OBJ) $(PARSE_OBJ) $(TARGET)
