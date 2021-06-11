#!/bin/bash
src=$1
passwd=$2
cishu=$3
file="test"
if [ ! -d $file ];then
mkdir $file
fi

for ((index=0;index<$cishu;index++))
do
    ./lab3_testing.sh $src $passwd > "./test/testfile_"$index
done
