#include "rec.h"
#include <iostream>
#include <bitset>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <utility>
#include <queue>
typedef unsigned short int _bit;
using namespace std;

rec::PackageInter::PackageInter(_bit b, char * d, int nseq, int nack, bool last) {
    this->bit  = b;
    this->data = d;
    this->_cli_id = -1;
    this->_port   = -1;
    this->_last   = last;
    this->_n_seq  = nseq;
    this->_n_ack  = nack;
}

_bit rec::PackageInter::get_bit(){
    return this->bit;
}

rec::pkg_addr::pkg_addr(PackageInter& _p, sockaddr_in _addr) {
    this->pkg  = _p;
    this->addr = _addr;
}