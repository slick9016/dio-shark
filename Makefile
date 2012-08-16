OBJS=dio_shark.o

all : $(OBJS)
	gcc -o dio-shark $(OBJS) -lpthread

%.o : %.c
	gcc -c $<

clean : 
	rm -f $(OBJS)
