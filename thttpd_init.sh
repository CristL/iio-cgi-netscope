#!/bin/sh
cat /var/www/data/left_top.htm > /var/www/data/left.htm
for i in  `echo show | /usr/local/bin/iio_cmdsrv | grep ad` ;do echo "   <option value=\"$i\">`echo $i | tr a-z A-Z`</option>";done >> /var/www/data/left.htm
cat /var/www/data/left_bot.htm >> /var/www/data/left.htm
