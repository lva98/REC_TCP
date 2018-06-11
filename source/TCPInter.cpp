#include "rec.h"
#include <iostream>
#include <bitset>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <future>
#include <string>
#include <utility>
#include <queue>
#include <chrono>
typedef unsigned short int _bit;
using namespace std;

bool rec::checkBit(_bit b1, _bit b2) {
    if((b1 & b2) == b2) return true;

    return false;
}

rec::TCPInter::TCPInter(_bit b, int port, size_t bs) {
    if((this->_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        exit(0);
    }
    this->buffer_size = bs;
    this->entity = b;
    this->_n_seq = 0;
    this->_n_ack = 0;

    if(checkBit(b, rec::BIT_SVR)) {
        this->_cli_id_sum = 0;
        if((this->_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            exit(0);
        }

        this->_addr.sin_family      = AF_INET;
        this->_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        this->_addr.sin_port = htons(port);

        int rc;
        if((rc = bind(this->_socket, (struct sockaddr *) &this->_addr, sizeof(this->_addr))) < 0) {
            exit(0);
        }
    } else
        exit(0);
}

rec::TCPInter::TCPInter(_bit b, size_t bs) {
    if((this->_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        exit(0);
    }
    this->buffer_size = bs;
    this->entity = b;
    this->_n_seq = 0;
    this->_n_ack = 0;

    if(checkBit(b, rec::BIT_CLI)) {
        int rc = -1;
        do {
            this->_addr.sin_family      = AF_INET;
            this->_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            srand(time(NULL));
            this->_addr.sin_port        = htons(rand()%1000 + 4000);
            this->_cli_id               = -1;
            rc = bind(this->_socket, (struct sockaddr *) &this->_addr, sizeof(this->_addr));
        } while(rc < 0);

        getsockname(this->_socket, (struct sockaddr*) &this->_addr, (socklen_t *) sizeof(this->_addr));
        printf("The selected port is %d.\n\n", ntohs(this->_addr.sin_port));
    } else {
        exit(0);
    }
}
void rec::TCPInter::start() {
    this->_thread_listen = async(std::launch::async, &rec::TCPInter::listen, this);
    this->_thread_sender_buffer = async(std::launch::async, &rec::TCPInter::check_sender_buffer, this);
}
void rec::TCPInter::wait_close() {
    this->_thread_listen.get();
    this->_thread_sender_buffer.get();
}

void rec::TCPInter::connect(int svport, char * svaddr) {
    if(checkBit(this->entity, rec::BIT_CLI)) {
        struct hostent *h;
        h = gethostbyname(svaddr);
        if(h== NULL) {
            printf("unknown host");
            exit(1);
        }

        this->_sv_addr.sin_family = h->h_addrtype;
        memcpy((char *) &this->_sv_addr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
        this->_sv_addr.sin_port = htons(svport);

        PackageInter p(BIT_SYN, NULL, this->_n_seq, this->_n_ack, true);
        p._port = ntohs(this->_addr.sin_port);

        cout << "--sending SYN to server" << endl << endl;
        mtx_send_buffer.lock();
        this->sender_buffer.push(pkg_addr(p, this->_sv_addr));
        mtx_send_buffer.unlock();

        /*
        if(sendto(this->_socket, (void *) &p, sizeof(PackageInter), 0, (struct sockaddr *) &this->_sv_addr, sizeof(this->_sv_addr)) < 0) {
            close(this->_socket);
            exit(0);
        }*/


    } else exit(0);
}

/*
void rec::TCPInter::check_entry_buffer() {
    while(true) {
        if(!this->entry_buffer.empty()) {
            PackageInter p_    = this->entry_buffer.front().first[0];
            sockaddr_in  addr_ = this->entry_buffer.front().second;
            addr_.sin_port = htons(p_._port);
            this->assert(&p_, addr_);
            this->_n_ack++;
        }
    }
}
*/

void rec::TCPInter::check_sender_buffer() {
    while(true) {
        if(!this->sender_buffer.empty()) {
            PackageInter p_    = this->sender_buffer.front().pkg;
            sockaddr_in  addr_ = this->sender_buffer.front().addr;
            this->sendd(&p_, addr_);


            if(this->_pkg_received[p_._n_seq] || p_.get_bit() == rec::BIT_ACK) {
                mtx_send_buffer.lock();
                this->sender_buffer.pop();
                mtx_send_buffer.unlock();
            } else {
                cout << "Pacote não chegou ainda, nova tentativa de envio" << endl;
                this_thread::sleep_for(chrono::milliseconds(500));
            }
        }
    }
}

void rec::TCPInter::listen() {
    while(1) {
        PackageInter * p = new PackageInter(0x0, NULL, -1, -1, true);
        sockaddr_in senderAddr;

        /* receive message */
        int serverLenght = sizeof(struct sockaddr_in);
        int n = recvfrom(this->_socket, (void *) p, this->buffer_size, 0, (struct sockaddr *) &senderAddr, (socklen_t *) &serverLenght);
        if(n < 0) {
            printf("Cannot receive data \n");
            continue;
        }

        (this->entry_buffer) [(*p)._n_seq] = pkg_addr(*p, senderAddr);
        (this->_pkg_received)[(*p)._n_ack] = true;
        /* print received message */
        //printf("from %s:UDP%u : %d\n", inet_ntoa(senderAddr.sin_addr), ntohs(cliAddr.sin_port), (*p).get_info());
    }/* end of server infinite loop */
}

void rec::TCPInter::assert(rec::PackageInter * p, sockaddr_in & senderAddr) {
    if(rec::BIT_SYN == (*p).get_bit() && this->entity == rec::BIT_SVR) {
        this->_n_seq = 0;
        this->_n_ack = 0;

        cout << "SYN: From " << inet_ntoa(senderAddr.sin_addr) << endl;
        cout << "--sending SYN|ACK to Client " << _cli_id_sum << endl << endl;
        this->_cli_port_list.push_back((*p)._port);

        PackageInter * p_to_send = new PackageInter(rec::BIT_SYN_ACK, NULL, this->_n_seq, this->_n_ack, true);
        (*p_to_send)._cli_id = this->_cli_id_sum;
        (*p_to_send)._port   = (*p)._port;
        senderAddr.sin_port  = htons((*p)._port);

        mtx_send_buffer.lock();
        this->sender_buffer.push(pkg_addr(*p_to_send, senderAddr));
        mtx_send_buffer.unlock();

        this->_cli_id_sum++;
    }

    else if(rec::BIT_SYN_ACK == (*p).get_bit() && this->entity == rec::BIT_CLI){
        this->_cli_id = (*p)._cli_id;
        PackageInter * p_to_send = new PackageInter(rec::BIT_ACK, NULL, this->_n_seq, this->_n_ack, true);
        (*p_to_send)._cli_id = this->_cli_id;
        (*p_to_send)._port   = this->_port;

        mtx_send_buffer.lock();
        this->sender_buffer.push(pkg_addr(*p_to_send, this->_sv_addr));
        mtx_send_buffer.unlock();

        cout << "SYN|ACK: From " << inet_ntoa(senderAddr.sin_addr) << endl;
        cout << "--my _cli_id is " << (*p)._cli_id << endl;
        cout << "--sending ACK to server" << endl << endl;
    }

    else if(rec::BIT_ACK == (*p).get_bit()) {
        cout << "Recebido ACK" << endl;
    }
}

void rec::TCPInter::sendd(rec::PackageInter * p, sockaddr_in& recipient) {
    if(sendto(this->_socket, (void *) p, sizeof(PackageInter), 0, (struct sockaddr *) &recipient, sizeof(recipient)) < 0) {
        close(this->_socket);
        exit(0);
    }
}
