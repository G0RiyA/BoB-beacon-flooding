#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <linux/if.h>
#include <linux/if_ether.h> 
#include <linux/if_packet.h>

#include "main.h"

const uint8_t RADIOTAB_CONSTANTS[] = \
    "\x00\x00\x18\x00\x2e\x40\x00\xa0\x20\x08\x00\x00\x00\x02\x6c\x09" \
    "\xa0\x00\xbd\x00\x00\x00\xbd\x00";


int main(int argc, const char *argv[]) {
    int i;
    int idx;
    int sock_fd;
    int SSID_count;
    char *SSID;
    FILE *SSID_fp;
    mac_t *temp_mac;
    packet_t **packets;
    struct ifreq ifr;
    struct sockaddr_ll sadr;

    /*
        args check
    */
    if (argc != 3) {
        dprintf(STDERR_FILENO, "Usage: %s <interface> <ssid-list-file>\n", *argv);
        return 0;
    }

    /*
        interface name length check
    */
    if (strnlen(argv[1], IFNAMSIZ) >= IFNAMSIZ) {
        ERR("Invalid interface name!!");
    }

    /*
        open raw socket
    */
    if ((sock_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
        ERR("Could not open socket!!");
    }

    /*
        bind interface
    */
    strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);
    
    if (ioctl(sock_fd, SIOCGIFINDEX, &ifr) < 0) {
        ERR("Failed ioctl!!");
    }

    sadr.sll_family = AF_PACKET;
    sadr.sll_ifindex = ifr.ifr_ifindex;
    sadr.sll_protocol = htons(ETH_P_ALL);

    if (bind(sock_fd, (struct sockaddr*)&sadr, sizeof(sadr)) < 0) {
        ERR("Bind error!!");
    }

    /*
        open SSID list file
    */
    if ((SSID_fp = fopen(argv[2], "rt")) == NULL) {
        ERR("No such file '%s'", argv[2]);
    }

    /*
        read SSIDs
    */
    SSID_count = 0;
    while (fscanf(SSID_fp, "%*s") >= 0) ++SSID_count;
    packets = (packet_t**)calloc(SSID_count, sizeof(packet_t*));

    fseek(SSID_fp, 0, SEEK_SET);

    for (i = 0; fscanf(SSID_fp, "%ms", &SSID) >= 0; ++i) {
        if (SSID == NULL) {
            ERR("Memory error!");
        }

        packets[i] = (packet_t*)calloc(1, sizeof(packet_t));
        if (packets[i] == NULL) {
            ERR("Memory error!");
        }

        packets[i]->length = sizeof(radio_header_t) + sizeof(beacon_t);
        packets[i]->params_cur = 0;

        /*
            Radio information frame
        */
        memcpy(&packets[i]->radio_header, RADIOTAB_CONSTANTS, sizeof(radio_header_t));

        /*
            Beacon frame
        */
        packets[i]->beacon.version = 0;
        packets[i]->beacon.type = 0;
        packets[i]->beacon.subtype = 8;
        packets[i]->beacon.duration = 0;

        if (random_mac(&temp_mac) != 0) {
            return -1;
        }
        memcpy(&packets[i]->beacon.receiver, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", 6);
        memcpy(&packets[i]->beacon.trasmitter, temp_mac, 6);
        memcpy(&packets[i]->beacon.bss_id, temp_mac, 6);
        packets[i]->beacon.fragment = 0;
        packets[i]->beacon.sequence = 0;
        packets[i]->beacon.interval = 0x64;
        packets[i]->beacon.capabilities = 0x1511;

        /*
            Tagged parameters
        */
        if ( push_tagged_param(&packets[i], 0, strlen(SSID), (uint8_t*)SSID) || \
                push_tagged_param(&packets[i], 1, 8, (uint8_t*)"\x82\x84\x8b\x96\x24\x30\x48\x6c") || \
                push_tagged_param(&packets[i], 3, 1, (uint8_t*)"\x01") || \
                push_tagged_param(&packets[i], 42, 1, (uint8_t*)"\x04") || \
                push_tagged_param(&packets[i], 50, 4, (uint8_t*)"\x30\x48\x60\x6c")
        ) {
            return -1;
        }

        free(temp_mac);
        free(SSID);
    }


    fclose(SSID_fp);

    idx = 0;
    while(1) {
        send_packet(sock_fd, packets[idx++]);
        idx %= SSID_count;
    }   
}

int random_mac(mac_t **p_mac) {
    int urandom;

    if ((urandom = open("/dev/urandom", O_RDONLY)) < 0) {
        ERR("Could not open '/dev/urandom'!!");
    }

    if( (*p_mac = (mac_t*)calloc(1, sizeof(mac_t))) == NULL) {
        ERR("Memory error!");
    }

    if (read(urandom, *p_mac, sizeof(mac_t)) != sizeof(mac_t)) {
        ERR("Random error!");
    }

    close(urandom);

    return 0;
}

int push_tagged_param(packet_t **p_packet, uint8_t number, uint8_t length, uint8_t *data) {
    packet_t *packet;
    tagged_parameter_t *tagged_param;

    packet = *p_packet;

    if ( (*p_packet = (packet_t*)realloc(packet, PACKET_EXTEND(packet, length))) == NULL){
        ERR("Memory error!");
    }
    
    packet = *p_packet;
    packet->length += sizeof(tagged_parameter_t) + length;

    tagged_param = (tagged_parameter_t*)(packet->tag_params + packet->params_cur);

    tagged_param->length = length;
    tagged_param->number = number;
    memcpy(tagged_param->data, data, length);

    packet->params_cur += length + 2;

    return 0;
}

int send_packet(int socket_fd, packet_t* packet) {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    packet->beacon.timestamp = currentTime.tv_sec * (uint64_t)1e6 + currentTime.tv_usec;

    if (write(socket_fd, &packet->radio_header, packet->length) != packet->length) {
        ERR("Packet write error!");
    }

    packet->beacon.sequence += 1;

    return 0;
}