#!/bin/bash
thread_num=1
inputfile="input.txt"
result="result.dat"
result_="result_.dat"
if [ -e $result ]
    then 
        true > $result
    else
        touch $result
fi
if [ -e $result_ ]
    then 
        true > $result_
    else
        touch $result_
fi
tmp=0
while ((tmp<=2))
do
    ./sudoku_ $inputfile d > /dev/null
    let "tmp++"
done
tmp=0
while ((tmp<=10))
do
    ./sudoku_ $inputfile d | tee -a $result_
    let "tmp++"
done
while(( $thread_num<=20 ))
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
