CC = clang
CFLAGS = -g

opdump : opdump.c
	$(CC) $(CFLAGS) opdump.c -o opdump
	ldid -S../ent.xml ./opdump
