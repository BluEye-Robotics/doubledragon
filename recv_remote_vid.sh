#!/bin/bash
expect -c \
" \
  spawn /bin/bash -c \
    \"scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@192.168.1.101:/videos/out.mp4 .\" ; \
    expect -re \".*ssword.*\"; \
    send \"chimaera\r\n\"; \
    interact \
"
