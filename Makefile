myshell: myshell.c
	gcc -I -Wall myshell.c -lreadline -o myshell

clean:
	rm myshell
	
