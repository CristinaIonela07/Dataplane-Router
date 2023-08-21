#include "queue.h"
#include "lib.h"
#include "protocols.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/*
	Interschimbare 
	adresa destinatie - adresa sursa 
	in header-ul de ethernet
*/ 
void swap_addr(struct ether_header* ethernet, char* buf){
	for (int i = 0; i < 6; i++){
		uint8_t eth_aux = ethernet->ether_shost[i];
		ethernet->ether_shost[i] = ethernet->ether_dhost[i];
		ethernet->ether_dhost[i] = eth_aux;
	}
}

/*
	Interschimbare 
	adresa destinatie - adresa sursa 
	in header-ul ip
*/ 
void swap_ip(struct iphdr *ip, char* buf){
	uint32_t addr = ip->saddr;
	ip->saddr = ip->daddr;
	ip->daddr = addr;
}

/*
	Algoritm eficient de cautare a celei mai bune rute
	Complexitate temporala O(lgn)
*/ 
void binarySearch(struct route_table_entry *route_t, uint32_t addr_dest, int left, int right,  int *best) {
    if (left <= right) {
		int middle = left + (right - left) / 2;
		if (ntohl(route_t[middle].prefix) == ntohl(route_t[middle].mask & addr_dest)) {
			*best = middle;
			binarySearch(route_t, addr_dest, middle + 1, right, best);
		}
		else if (ntohl(route_t[middle].prefix) < ntohl(route_t[middle].mask & addr_dest))
			binarySearch(route_t, addr_dest, middle + 1, right, best);
		else
            binarySearch(route_t, addr_dest, left, middle - 1, best);
	}
}

/*
	Cautare in tabela de arp a unei intrari care sa 
	se potriveasca cu adresa destinatie ip_dest
*/ 
int get_mac_entry(struct arp_entry *arp_t, int arp_size, uint32_t ip_dest) {
	for (int i = 0; i < arp_size; i++) {
		if (arp_t[i].ip == ip_dest) {
			return i;
		}
	}
	return -1;
}

/*
	Functie de comparare pentru qsort
	Sortare crescatoare dupa prefix si masca
*/ 
int comp(const void *p1, const void *p2) {
	struct route_table_entry *r1 = (struct route_table_entry *)p1;
	struct route_table_entry *r2 = (struct route_table_entry *)p2;
	if ((ntohl(r1->prefix) == ntohl(r2->prefix)) && (ntohl(r1->mask) == ntohl(r2->mask)))
		return 0; 
	else if (ntohl(r1->prefix & r1->mask) == ntohl(r2->prefix & r2->mask) && ntohl(r1->mask) != ntohl(r2->mask))
		return ntohl(r1->mask) - ntohl(r2->mask);
	else return ntohl(r1->prefix & r1->mask) -  ntohl(r2->prefix & r2->mask);
}

/*
	Algoritm pentru crearea pachetului trimis in caz de eroare
*/ 
void error(int interface, char* buf, int type, struct ether_header *ethernet, struct iphdr *ip) {
	struct iphdr *received_ip = (struct iphdr*)malloc(sizeof(struct iphdr));
	memcpy(received_ip, ip, sizeof(struct iphdr));

	swap_addr(ethernet, buf);
	swap_ip(ip, buf);
	ip->protocol = 1;
	ip->tot_len = sizeof(struct iphdr) + 64;

	memcpy(buf + sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr), 
		received_ip, 64);

	struct icmphdr *icmp = (struct icmphdr*)(buf + sizeof(struct ether_header) + sizeof(struct iphdr));
	icmp->type = type;
	
	/*
		Se actualizeaza checksum-ul, realizat atat peste 
		hearder-ul de icmp, cat si peste data
	*/
	icmp->checksum = checksum((uint16_t *)icmp, sizeof(struct iphdr) + sizeof(struct icmphdr) + 8);

	int length = sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr) + 64;
	send_to_link(interface, buf, length);
}

int main(int argc, char *argv[]) {
	char buf[MAX_PACKET_LEN];

	struct route_table_entry *route_t = (struct route_table_entry *)malloc(sizeof(struct route_table_entry) * 100000);
	struct arp_entry *arp_t = (struct arp_entry *)malloc(sizeof(struct arp_entry) * 100);
	
	/*
		Citire tabela de rutare
		Parsare tabela de arp
	*/ 
	int route_size = read_rtable(argv[1], route_t);
	int arp_size = parse_arp_table("arp_table.txt", arp_t);

	/*
		Sortare tabela de rutare
	*/ 
	qsort(route_t, route_size, sizeof(struct route_table_entry), comp);	

	// Do not modify this line
	init(argc - 2, argv + 2);

	while (1) {
		int interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");
		
		struct ether_header *ethernet = (struct ether_header *)buf;
		struct iphdr *ip = (struct iphdr *)(buf + sizeof(struct ether_header));

		/*
			Daca pachetul primit este de tip IP
		*/
		if (ntohs(ethernet->ether_type) == 0x0800) {
			struct in_addr* net_byte_order = (struct in_addr*)malloc(sizeof(struct in_addr));
			inet_aton(get_interface_ip(interface), net_byte_order);
			
			/*
				Se verifica daca adresa destinatie a pachetului 
				coincide cu cea a routerului si daca pachetul 
				primit este de tip Echo Request
			*/ 
			if (net_byte_order->s_addr == ip->daddr) {
				struct icmphdr *icmp = (struct icmphdr*)(buf + sizeof(struct ether_header) + sizeof(struct iphdr));
				if(icmp->type == 8) {
					/*
						Se trimite pachetul inapoi facand modificarile necesare 
					*/
					swap_addr(ethernet, buf);
					swap_ip(ip, buf);
					ip->tot_len = sizeof(struct iphdr);
					icmp->type = 0;
					send_to_link(interface, buf, len);
				}
			}
		}

		/*
			Daca a expirat campul ttl
			routerul emite un pachet creat 
			special pentru cazul de eroare "Time exceeded"
		*/
		if (ip->ttl <= 1)
			error(interface, buf, 11, ethernet, ip);
		
		/*
			Se verifica integritatea headerului ip
			Daca checksum-ul este incorect, se arunca pachetul
		*/
		uint16_t old_check = ip->check;
		ip->check = 0;
		uint16_t new_check = checksum((uint16_t *)ip, sizeof(*ip));
		if (ntohs(new_check) != old_check) {
			continue;
		}

		/*
			Se decrementeaza ttl-ul 
			Se recalculeaza checksum-ul
		*/
		ip->ttl--;
		ip->check = ntohs(checksum((uint16_t *)ip, sizeof(*ip)));

		/*
			Se cauta cea mai specifica intrare din 
			tabela de rutare care sa ajunga la destinatie
		*/
		int r_index = -1;
		binarySearch(route_t, ip->daddr, 0, route_size-1, &r_index);

		/*
			Daca se gaseste o ruta, se trimite pachetul
		*/
		if (r_index != -1) {
			int a_index = get_mac_entry(arp_t, arp_size, route_t[r_index].next_hop);
			get_interface_mac(route_t[r_index].interface, ethernet->ether_shost);
			memcpy(ethernet->ether_dhost, arp_t[a_index].mac, 6);
			send_to_link(route_t[r_index].interface, buf, len);
		}

		/*
			Altfel, se emite un pachet creat pentru 
			cazul de eroare "Destination unreachable"
		*/
		else 
			error(interface, buf, 3, ethernet, ip);
	}

	return 0;
}