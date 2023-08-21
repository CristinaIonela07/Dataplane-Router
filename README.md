# Dataplane-Router

Am realizat implementarea urmatoarelor: Procesul de dirijare, 
Longest Prefix Match eficient, Protocolul ICMP.

Pentru inceput am realizat citirea tabelei de rutare si a tabelei de arp, 
urmand ca tabela de rutare sa fie sortata crescator, dupa prefix si masca.

In cazul in care adresa destinatiei din headerul ip al pachetului primit 
este adresa router-ului si daca pachetul primit este de tip "Echo request"
(type 8), se trimite pachetul de unde a venit prin interschimbarea adreselor 
destinatar-sursa din headerele ip si mac (fapt realizat cu ajutorul a doua 
functii, switch_ip si respectiv switch_addr), si setarea tipului mesajului 
pe 0 (mesaj de tip "Echo reply").

Pentru cazurile de eroare, expirarea campului ttl sau cand nu s-a gasit o 
ruta pana la destinatie, am folosit o functie auxiliara (error) pentru a 
evita codul duplicat. Se schimba adresele sursa-destinatie (similar cazului 
precedent), se seteaza tipul, iar checksum-ul este facut peste hearder-ul 
de icmp si peste data (header-ul ip al pachetului primit + 8 bytes din pachet).

Se recalculeaza checksum-ul. Daca este diferit de cel din header-ul ip, se 
arunca pachetul. Altfel se decrementeaza ttl-ul, si se afla noul checksum.

Pentru descoperirea celei mai bune rute, folosesc un algortim de cautare binara 
recursiv. Pentru rezolvarea acestei cerinte am construit un alt program ajutator 
cu o cautare binara pentru a gasi cel mai mare index la care se gaseste un element, 
intr-un vector sortat care contine duplicate. Am adaptat acel program la cerintele 
problemei actuale. Am determinat adresa urmatorului hop, am actualizat header-ul 
de ethernet si am trimis pachetul pe acea ruta.
