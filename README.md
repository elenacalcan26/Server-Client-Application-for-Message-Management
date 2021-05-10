# Server-Client-Application-for-Message-Management 


<h5>Calcan Elena-Claudia <br/>
321CA</h5>

Programul reprezinta implementarea unui model client-server pentru gestionarea
mesajelor.


server
-------------------------------------------------------------------------------	

  - are rolul de a redirectiona mesajele primite de la clientul UDP catre toti
	clientii TCP care sunt abonati la un anumit topic
  - pentru gestionarea mesajelor de la clienti se foloseste multiplexare I/O
  - se creeaza un socket pentru comunicarea cu stdin, clientul UDP si un socket pasiv,
	folosit la cererea de conexiuni de la clientii TCP
  - informatiile despre un subscriber se retin intr-o structutace contine id-ul,
	topic-urile la care se abonat, sf-ul topicurilor la care s-a abonat, mesajele
	pe care trebuia sa le primeasca atunci cand era deconectat si socket-ul prin
	care se realizeaza comunicare dintre el si server
  - toti subscriberii conectati la server sunt retinuti intr-un vector
	
  - server-ul ruleaza pana cand primeste comanda de "exit" de la stdin
  - cand primeste comanda, se iese din bucla si se inchid toti socketii activi
	
  - daca se primeste o cerere de conexiune pe socket-ul pasiv, se dezactiveaza
	algoritmul lui Nagle
  - se verifica daca subscriber-ul este nou, deconectat sau mai exista un 
	subscriber cu acelasi id
  - daca este un client nou, atunci se creeaza structura si se retin id-ul si 
	socket-ul
  - daca se reconecteaza atunci se retine noul socket intors de metoda accept()
	si i se trimit mesajele pierdute, cand acesta era deconectat
  - daca exista un client cu acelasi id, se inchide socket-ul noului subscriber
	
  - daca se primeste un mesaj de la clientul UDP, acesta este parsat si se creeaza
	mesajul pe care server-ul il trimite tuturor abonatilor la topicul respectiv
  - mesajul este trimis sub forma de buffer 
  - se parcurg subscriberii si topicurile la care sunt abonati
  - server-ul trimite mesajele daca subscriber-ul este activat sau le salveaza daca
	subscriber-ul este deconectat si sf-ul topicului este 1

  - daca se primeste un mesaj de la un client TCP, se verifica tipul acestuia
  - daca mesajul este gol, atunci clientul s-a deconectat si se inchide conexiunea,
	setand starea acestuia ca fiind deconectat
  - altfel s-a primit o comanda de subscribe sau unsubscribe 
  - la comanda de subscribe se audaga in lista de topicuri la care este abonat 
	subscriber-ul si sf-ul acestuia; daca mai primeste o comanda de subscribe 
	pe un topic la care s-a abonat, dar are alt sf, atunci se inlocuieste sf-ul
	topicului cu cel nou
  - la comanda de unsubscribe, se sterge topicul si sf-ul acestuia din vectori 



subscriber
-------------------------------------------------------------------------------

  - pentru gestionarea mesajelor se foloseste multiplexare I/O
  - se creeaza socketi pentru comunicarea cu server-ul si stdin
  - dupa ce s-a realizat conexiunea dintre client si servere, se trimite id-ul
	subscriber-ului
  - se dezactiveaza algoritmul lui Nagle

  - se ruleaza pana cand primeste comanda de "exit" de la stdin
  - cand primeste aceasta comanda, clientul anunta server-ul si se inchide
	conexiunea

  - subscriber-ul primeste comenzi de subscribe / unsubscribe, acestea fiind
	verificate 
  - cand se primeste o comanda gresita, eroarea se afiseaza la stderr, iar clientul
	asteapta o noua comanda de la stdin
  - altfel trimite comanda primita catre server

  - cand primeste mesaj de la server, acesta este afisat la stdout
