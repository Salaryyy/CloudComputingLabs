#!/bin/bash
thread_num=1
inputfile="input.txt"
result="result.dat"
if [ -e $result ]
    then 
        true > $result
    else
        touch $result
fi
while(( $thread_num<=15 ))
do
    test_num=0
    while(($test_num<=2))
    do
        ./sudoku $inputfile $thread_num > /dev/null
        let "test_num++"
    done
    test_num=0
    while(($test_num<10))
    do
        echo -e "$thread_num \c" | tee -a $result
        ./sudoku $inputfile $thread_num | tee -a $result
        let "test_num++"
    done
    let "thread_num++"
done
python3 test.py
