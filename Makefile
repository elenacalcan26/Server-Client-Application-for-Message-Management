build:
	gcc -Wall -g server.c -o server 
	gcc -Wall -g subscriber.c -o subscriber 

# Ruleaza serverul
run_server:
	./server 

# Ruleaza clientul
run_subscriber:
	./subscriber 

clean:
	rm -f *.o server subscriber
