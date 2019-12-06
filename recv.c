#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "link_emulator/lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

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
    msg r, first;               //mesaj trimis
    int res;                    //mesaj primit
    int nextloss = 0;           //id pachet pierdut
    init(HOST,PORT);

    //----deschidere fisier output----//
    int fd = open("recv_fileX", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    res = recv_message(&first);
    if (res < 0) {
        perror("[RECEIVER] Receive error. Exiting.\n");
        return -1;
    }

    int* adresa_cadre = (int *)first.payload;
    int nrcadre = *adresa_cadre;        //numar pachete
    msg frames[900];                    //vector de stocare cadre
    int upperedge = -1;                 //upper edge al ferestrei glisante
    int ackexpected  = -1;              //id ul pachetului asteptat corespunzator lower edge al ferestrei

    //----asteptam cadre si verificam corectitudinea lor----//
    while (1) {
        //----conditie oprire----//
        if(ackexpected  >= nrcadre) {
            break;
        }
        res = recv_message_timeout(&r, 50);
        int* id = (int*)(&r.payload);
        char* checksumvalue = (char*)id + sizeof(int);
        char* payload = (char*)checksumvalue + sizeof(char);
        int* id2 = (int*)((char*)payload + r.len + 1);
        int* id3 = (int*)((char*)id2 + sizeof(int));

        if (res < 0) {
            nextloss++;                 //inregistram o pierdere de pachet

            //----caz prim cadru----//
            if(ackexpected  == -1) {
                char *adresa_resend = (char *) frames[ackexpected  + 1].payload + sizeof(int);
                *adresa_resend = 'A';
                frames[ackexpected  + 1].len = 5;
                res = send_message(&frames[ackexpected  + 1]);
            } else {

                //----caz in care se pierd pachete pe link----//
                int* id = (int*)(&frames[ackexpected - 1].payload);
                char* adresa_resend = (char*)((char*)frames[ackexpected - 1].payload + sizeof(int));
                *id = ackexpected + 1;
                *adresa_resend = 'N';
                frames[ackexpected - 1].len = 5;
                res = send_message(&frames[ackexpected - 1]);
                id = (int*)(&frames[ackexpected].payload);
                adresa_resend = (char*)frames[ackexpected - 1].payload + sizeof(int);
                *adresa_resend = 'N';
                frames[ackexpected ].len = 5;
                res = send_message(&frames[ackexpected ]);
            }
        } else {

            //---- pachet corupt----//
            if(checksum(payload, r.len) != *checksumvalue || *id != *id2) {
                if (*id != *id3) {
                    *id = *id2;
                }
                *checksumvalue = 'N';
                res = send_message(&r);     //trimitem NACK
            } else {
                *checksumvalue = 'A';
                frames[*id] = setFrame(*id, payload, r.len);

                //----update upper edge al ferestrei glisante----//
                if(upperedge < *id) {
                    upperedge = *id;
                }

                //----verificam id cadru primit = lower edge al ferestrei glisante
                if(ackexpected  + 1 != *id) {
                    *id = ackexpected ;             //updatam lower edge al ferestrei glisante
                    *checksumvalue = 'A';
                    r.len = 5;
                    res = send_message(&r);         //trimitem ACK
                } else {

                    //----verificam fereastra glisanta pentru cadre primite corect----//
                    while(ackexpected  <= upperedge) {
                        ackexpected++;
                        int* receivedframe = (int*)(&frames[ackexpected].payload);

                        //----scriem in fisier cadrul corespunzator----//
                        if((*receivedframe) == ackexpected){
                            write(fd, frames[ackexpected].payload + 5, frames[ackexpected].len);
                        } else {
                            ackexpected --;
                            int* id = (int*)(&frames[ackexpected + 1].payload);
                            char* adresa_ack = (char*)frames[ackexpected + 1].payload + sizeof(int);
                            *id = ackexpected + 1;
                            *adresa_ack = 'N';
                            frames[ackexpected ].len = 5;
                            res = send_message(&frames[ackexpected]);
                            break;
                        }
                    }
                    char* adresa_ack = (char*)frames[ackexpected].payload + sizeof(int);
                    *adresa_ack = 'A';
                    frames[ackexpected ].len = 5;
                    res = send_message(&frames[ackexpected]);
                }
            }
            if (res < 0) {
                perror("[RECEIVER] Send ACK error. Exiting.\n");
                return -1;
            }
        }
    }
    close(fd);
    printf("[RECEIVER] Finished receiving..\n");
    return 0;
}
