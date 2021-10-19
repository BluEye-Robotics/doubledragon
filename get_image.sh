#!/bin/bash
expect -c \
" \
  spawn /bin/bash -c \
    \"scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@192.168.1.101:/data/out.jpg .\" ; \
    expect -re \".*ssword.*\"; \
    send \"chimaera\r\n\"; \
    interact \
"
