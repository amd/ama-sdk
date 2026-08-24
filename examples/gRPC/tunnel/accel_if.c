/*
 * Copyright (C) 2024, Xilinx Inc - All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <fcntl.h>
#include <errno.h>


#define BUFSIZE 2000

int tun_alloc(char *dev, int flags) {

  struct ifreq ifr;
  int fd, err;
  char *clonedev = "/dev/net/tun";

  if( (fd = open(clonedev , O_RDWR)) < 0 ) {
    perror("Opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;

  if (*dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
    perror("ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }
  if( (err = ioctl(fd, TUNSETPERSIST, 1)) < 0){
    perror("Enabling TUNSETPERSIST");
    close(fd);
    return err;
  }
  strcpy(dev, ifr.ifr_name);

  return fd;
}


int main(int argc, char **argv)
{
    int tap_fd;
    int flags = IFF_TAP;
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NIC\n",argv[0]);
        exit(-1);
    }
    char *if_name=calloc(IFNAMSIZ, sizeof(char));
    strncpy(if_name, argv[1],IFNAMSIZ);
    if ( (tap_fd = tun_alloc(if_name, flags | IFF_NO_PI)) < 0 ) {
        perror("Error connecting to tun/tap interface!\n");
        exit(1);
     }
    printf("NIC name=%s\n", if_name);
    return 0;
}
