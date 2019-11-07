#NAME: Daniel Chen,Winston Lau
#EMAIL: kim77chi@gmail.com,winstonlau99@gmail.com
#ID: 605006027,504934155

lab3b: lab3b.py 
	ln lab3b.py lab3b

dist: lab3b-605006027.tar.gz
lab3b-605006027.tar.gz: lab3b.py Makefile README
	tar -czf $@ lab3b.py Makefile README

clean: 
	rm lab3b-605006027.tar.gz 
