if [ $# == 1 ]
then
    a=$1
else
    a=3
fi
if [ $a == 1 ]
then
    conf="./participant_sample.conf"
elif [ $a == 2 ]
then
    conf="./participant_sample2.conf"
else
    conf="./participant_sample3.conf"
fi
./participant --config_path $conf