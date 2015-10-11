#!/bin/bash
for fyl in $1/*.pcap;
do
    ./a.out $fyl > out
    diff -B -ignore-all-space out $(echo $fyl | cut -d '.' -f1).out
done
#diff -B -ignore-all-space out  
