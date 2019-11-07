#NAME: Daniel Chen,Winston Lau
#EMAIL: kim77chi@gmail.com,winstonlau99@gmail.com 
#ID: 605006027,504934155 


lab3a: lab3a.c
	gcc -g -Wall -Wextra -o $@ lab3a.c

dist: lab3a-605006027.tar.gz  
files= README Makefile lab3a.c ext2_fs.h

lab3a-605006027.tar.gz : $(files)
	tar -czf $@ $(files) 

clean: 
	rm lab3a 
