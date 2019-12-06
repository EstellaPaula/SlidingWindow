#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include "link_emulator/lib.h"

#define HOST "127.0.0.1"
#define PORT 10000
#define BUFLEN 1380


char checksum(char *c, int len) {
    char result = 0;
    int i;
    for(i = 0; i < len; i++) {
        result = result ^ c[i];
    }
    return result;
}

msg setFrame(int id, char *buf, int length) {
    msg t;
    int* adresa_id =(int*) t.payload;
    char* adresa_checksum = (char*)adresa_id + sizeof(int);
    char* adresa_text = (char*)adresa_checksum + sizeof(char);
    int* adresa_id2 = (int *)((char*)adresa_text + length + 1);
    int* adresa_id3 = (int*)((char*)adresa_id2 + sizeof(int));
    memcpy(adresa_text, buf, length + 1);
    *adresa_checksum = checksum(buf, length);
    *adresa_id = id;
    *adresa_id2 = id;
    *adresa_id3 = id;
    t.len = length;
    return t;
}

int main(int argc,char** argv){
    init(HOST,PORT);
    msg r;    //mesaj trimis

    int fd = 0; //file pointer
    int currentframe = 0; //retine pachetul de trimis

    int df = (atoi(argv[2]) * atoi(argv[3]) * 1000) / (8 * MSGSIZE); //dimensiune fereastra
    char buf[MSGSIZE];  //buffer
    memset(buf, 0, MSGSIZE);  //toresend buffer

    //----dimensiune fisier----//
    FILE* fp = fopen(argv[1], "rb");
    fseek(fp, 0L, SEEK_END);
    int bytes = ftell(fp);

    //----numar pachete de trimis----//
    int sizefisier = bytes / (BUFLEN);  //numar pachete
    if(bytes % (BUFLEN) != 0) {
        sizefisier++;
    }

    //---- trimite dimensiune fereastra si numar pachete-----//
    int* id = (int *)r.payload;
    int* dfsize = (int *)((char*)id + sizeof(int));
    *id = sizefisier;
    *dfsize = df;
    send_message(&r);
    fclose(fp);

    int receivedacks[sizefisier];  //vector to keep track of received acks
    memset( receivedacks, 0, sizefisier*sizeof(int) );
    msg frames[900]; //vector pachete
    fd = open(argv[1], O_RDONLY);
    int citit = read(fd, buf, BUFLEN);
    int res = 0;            //mesajul primit
    int ackexpected = 1;    //id ul corespunzator lower edge fereastra glisanta
    int upperedge = 1;      //upper edge al ferestrei glisante
    int toresend = 0;       //need to resend frame

    //---read packages & update frames and pachete structure----//
    while((currentframe < sizefisier) && (currentframe < df)) {
        frames[currentframe] = setFrame(currentframe, buf, citit);
        send_message(&frames[currentframe]);
        memset(buf, 0, MSGSIZE);
        currentframe++;
        citit = read(fd, buf, BUFLEN);
    }

    //----trimite restul fisierului----//
    while(currentframe <= sizefisier) {
        res = recv_message(&r);
        if(res < 0) {
            citit = 0;
        } else {
            int* id =(int*) r.payload;
            char* adresa_checksum = (char*)id + sizeof(int);
            if(*adresa_checksum == 'N') {
                toresend++;
                upperedge++;
                send_message(&frames[(*id)]);
            } else if(*adresa_checksum == 'A') {
                if(ackexpected == *id && !toresend) {
                    toresend++;
                    upperedge++;
                    send_message(&frames[(ackexpected + 1)]);
                } else {
                    if(toresend) {
                        toresend++;
                        if(toresend == (upperedge * df)) {
                            toresend = 0;
                        }
                    }
                    ackexpected = *id;
                    frames[currentframe] = setFrame(currentframe, buf, citit);
                    send_message(&frames[currentframe]);
                    memset(buf, 0, MSGSIZE);
                    currentframe++;
                    citit = read(fd, buf, BUFLEN);
                }
            }
        }
    }

    //---astept ack/nack uri ramase----//
    //upperedge = currentframe + 1;
    while(ackexpected < currentframe - 1) {
        res = recv_message(&r);
        char* adresa_checksum = (char*)id + sizeof(int);
        int* id = (int*)r.payload;
        if(*adresa_checksum == 'N') {
            send_message(&frames[(*id)]);
            toresend++;
            upperedge++;
        } else {
            if(ackexpected == *id && !toresend) {
                toresend++;
                send_message(&frames[(ackexpected + 1)]);
            } else {
                if(toresend) {
                    toresend++;
                    //----astept ack/nack dar luand in considerare  delay-ul----//
                    if(toresend >= (df * 2)) {
                        toresend = 0;
                    }
                }
                if(ackexpected <= *id) {
                    ackexpected = (int)*id;
                }
//                if((upperedge < ackexpected) || (upperedge > ackexpected + 2.5 * df)){
//                    upperedge = ackexpected + 1;
//                }
//                upperedge++;
                send_message(&frames[(upperedge)]);
            }
        }
    }
    return 0;
}
