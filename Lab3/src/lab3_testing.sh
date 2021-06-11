#!/bin/bash


LAB3_PATH="$1"
PASSWORD="$2"



# ######################################################################

COORDINATOR_IP=192.168.66.101
COORDINATOR_PORT=8001
NC_TIMEOUT=5
ERROR_RETRY_TIMES=10
START_RETYR_TIMES=$[1 * 5]     # 5 seconds
START_COORDINATOR_ONLY=1
START_COORDINATOR_AND_ALL_PARTICIPANTS=2
START_FIRST_PARTICIPANT_ONLY=3

DELAY=100 						   # ms
PACKET_LOSS_RATE=10 			   # percentage

LAB3_ABSOLUTE_PATH=""

PYTHON=1                           # PYTHON language
C_OR_CPP=2                         # C/C++ language
JAVA=3                             # JAVA language
UNKNOWN_LANGUAGE=255               # UNKNOWN language
NO_JAR_FILE=101              	   # have no jar file
NO_KVSTORE2PCSYSTEM_PY_FILE=102    # have no kvstore2pcsystem.py

SUCCESS=0                    	   # operate successfully
FAIL=255                      	   # operate failed

PASSED=66
FAILED=99

TEST_RESULT_ARR=("")

# ######################################################################

function get_absolute_path
{
	tmp_path="$1"

	# root_dir="/.*"
	if [[ ${tmp_path:0:1} = "/" ]]
	then
		retval=$tmp_path
		echo $retval
	else
		retval=`readlink -f ${tmp_path}`
		echo $retval
	fi
}

LAB3_ABSOLUTE_PATH=$(get_absolute_path $LAB3_PATH)
coordinator_config_path=${LAB3_ABSOLUTE_PATH}"/coordinator.conf"
participants_config_path=(${LAB3_ABSOLUTE_PATH}"/coordinator1.conf" \
	                      ${LAB3_ABSOLUTE_PATH}"/coordinator2.conf" \
	                      ${LAB3_ABSOLUTE_PATH}"/coordinator3.conf")

function generate_config_files
{
	echo -e "mode coordinator\n"\
"coordinator_info 192.168.66.101:8001\n"\
"participant_info 192.168.66.201:8002\n"\
"participant_info 192.168.66.202:8003\n"\
"participant_info 192.168.66.203:8004\n" > ${coordinator_config_path}

	echo -e "mode participant\n"\
"coordinator_info 192.168.66.101:8001\n"\
"participant_info 192.168.66.201:8002\n" > ${participants_config_path[0]}

	echo -e "mode participant\n"\
"coordinator_info 192.168.66.101:8001\n"\
"participant_info 192.168.66.202:8003\n" > ${participants_config_path[1]}

	echo -e "mode participant\n"\
"coordinator_info 192.168.66.101:8001\n"\
"participant_info 192.168.66.203:8004\n" > ${participants_config_path[2]}
}

function add_virtual_nics
{
	echo ${PASSWORD} | sudo -S ifconfig lo:0 192.168.66.101/24
	echo ${PASSWORD} | sudo -S ifconfig lo:1 192.168.66.201/24
	echo ${PASSWORD} | sudo -S ifconfig lo:2 192.168.66.202/24
	echo ${PASSWORD} | sudo -S ifconfig lo:3 192.168.66.203/24
}

function remove_virtual_nics
{
	echo ${PASSWORD} | sudo -S ifconfig lo:0 down
	echo ${PASSWORD} | sudo -S ifconfig lo:1 down
	echo ${PASSWORD} | sudo -S ifconfig lo:2 down
	echo ${PASSWORD} | sudo -S ifconfig lo:3 down
}

function set_virtual_nics_delay
{
	echo ${PASSWORD} | sudo -S tc qdisc add dev lo:0 root netem delay ${DELAY}ms
	echo ${PASSWORD} | sudo -S tc qdisc add dev lo:1 root netem delay ${DELAY}ms
	echo ${PASSWORD} | sudo -S tc qdisc add dev lo:2 root netem delay ${DELAY}ms
	echo ${PASSWORD} | sudo -S tc qdisc add dev lo:3 root netem delay ${DELAY}ms
}

function set_virtual_nics_packet_loss
{
	echo ${PASSWORD} | sudo -S tc qdisc add dev lo:0 root netem loss ${PACKET_LOSS_RATE}%
	echo ${PASSWORD} | sudo -S tc qdisc add dev lo:1 root netem loss ${PACKET_LOSS_RATE}%
	echo ${PASSWORD} | sudo -S tc qdisc add dev lo:2 root netem loss ${PACKET_LOSS_RATE}%
	echo ${PASSWORD} | sudo -S tc qdisc add dev lo:3 root netem loss ${PACKET_LOSS_RATE}%
}

# function check_netcat
# {
# 	echo ${PASSWORD} | sudo -S apt install netcat
# }

function init_network_env
{
	# check_netcat
	add_virtual_nics
	set_virtual_nics_delay
	set_virtual_nics_packet_loss
}



function do_make
{
	echo "Start make, waiting for a while......"
	make -C ${LAB3_ABSOLUTE_PATH}/
	retval=$?

	if [ $retval -eq 0 ]
	then
		echo "Make successfully"
		return $SUCCESS
	else
		echo "[Warning] : Make failed"
		return $FAIL
	fi
}

function check_background_process_start_status
{
	bp_pid=$1

	if ps | grep "$bp_pid[^[]" >/dev/null
	then
    	return $SUCCESS
	else
    	if wait $bp_pid # check if process executed successfully or not
    	then
        	return $SUCCESS
    	else
        	return $FAILED
    	fi
	fi
}

function language_checking
{
	echo "Language checking......"
	java_file_counter=`ls -1 ${LAB3_ABSOLUTE_PATH}/*.java ${LAB3_ABSOLUTE_PATH}/*.jar 2>/dev/null | wc -l`
	jar_file_counter=`ls -1 ${LAB3_ABSOLUTE_PATH}/kvstore2pcsystem.jar 2>/dev/null | wc -l`
	c_file_counter=`ls -1 ${LAB3_ABSOLUTE_PATH}/*.c 2>/dev/null | wc -l`
	cpp_file_counter=`ls -1 ${LAB3_ABSOLUTE_PATH}/*.cc ${LAB3_ABSOLUTE_PATH}/*.cpp ${LAB3_ABSOLUTE_PATH}/*.hpp 2>/dev/null | wc -l`
	python_file_counter=`ls -1 ${LAB3_ABSOLUTE_PATH}/*.py 2>/dev/null | wc -l`
	kvstore2pcsystem_py_file_counter=`ls -1 ${LAB3_ABSOLUTE_PATH}/kvstore2pcsystem.py 2>/dev/null | wc -l`

	if [ $java_file_counter -gt 0 ]
	then
		if [ $c_file_counter -eq 0 -a $cpp_file_counter -eq 0 ] && [ $python_file_counter -eq 0 ]
		then
			echo "Language: [ JAVA ]"
			if [ $jar_file_counter -eq 0 ]
			then
				echo "[Warning] : Have no file: kvstore2pcsystem.jar"
				return $NO_JAR_FILE
			fi
			return $JAVA
		fi
	fi

	if [ $python_file_counter -gt 0 ]
	then
		if [ $c_file_counter -eq 0 -a $cpp_file_counter -eq 0 ] && [ $java_file_counter -eq 0 ]
		then
			echo "Language: [ PYTHON ]"
			if [ $kvstore2pcsystem_py_file_counter -eq 0 ]
			then
				echo "Warning: [ Have no file: kvstore2pcsystem.py ]"
				return $NO_KVSTORE2PCSYSTEM_PY_FILE
			fi
			return $PYTHON
		fi
	fi

	if [ $c_file_counter -gt 0 ] || [ $cpp_file_counter -gt 0 ]
	then
		if [ $java_file_counter -eq 0 ] && [ $python_file_counter -eq 0 ]
		then
			echo "Language: [ C/C++ ]"
			return $C_OR_CPP
		fi
	else
		echo "Warning: [ UNKNOWN programing_language ]"
		return $UNKNOWN_LANGUAGE
	fi
}

function run_kvstore2pcsystem_c_and_other_language_robustly
{
	chmod 777 ${LAB3_ABSOLUTE_PATH}/*
	
	for (( i=0; i<$START_RETYR_TIMES; i++ ))
	do
		if [[ $1 -eq $START_FIRST_PARTICIPANT_ONLY ]]
		then
			break
		fi
		${LAB3_ABSOLUTE_PATH}/kvstore2pcsystem --config_path ${coordinator_config_path} &
		check_background_process_start_status $!
		retval=$?
		sleep 0.5

		if [[ $retval -eq 0 ]]
		then
			echo "Run coordinator successfully"
			coordinator_pid=$!
			break
		else
			sleep 1
			continue
		fi
	done

	if [ $retval -eq 0 ]
	then
		if [[ $1 -eq $START_COORDINATOR_ONLY ]]
		then
			return $SUCCESS
		fi

		for (( i=0; i<3; i++ ))
		do
			for (( j=0; j<$START_RETYR_TIMES; j++ ))
			do
				${LAB3_ABSOLUTE_PATH}/kvstore2pcsystem --config_path ${participants_config_path[i]} &
				check_background_process_start_status $!
				retval=$?

				if [[ $retval -eq 0 ]]
				then
					echo "Run participant[$i] successfully"
					participants_pid[$i]=$!
					break
				else
					echo "Run participant[$i]. Retry times: [$j]"
					continue
				fi
			done

			if [[ $retval -ne 0 ]]
			then
				echo "Run participant[$i] failed"
				return $FAIL
			fi
			if [[ $1 -eq $START_FIRST_PARTICIPANT_ONLY ]]
			then
				break
			fi
		done
	else
		echo "Run coordinator failed"
		return $FAIL
	fi

	echo "Run kvstore2pcsystem successfully"
	return $SUCCESS
}


function run_kvstore2pcsystem_java_robustly
{
	chmod 777 ${LAB3_ABSOLUTE_PATH}/*

	for (( i=0; i<$START_RETYR_TIMES; i++ ))
	do
		if [[ $1 -eq $START_FIRST_PARTICIPANT_ONLY ]]
		then
			break
		fi
		java -jar ${LAB3_ABSOLUTE_PATH}/kvstore2pcsystem.jar --config_path ${coordinator_config_path} &
		check_background_process_start_status $!
		retval=$?
		sleep 0.5

		if [[ $retval -eq 0 ]]
		then
			echo "Run coordinator successfully"
			coordinator_pid=$!
			break
		else
			sleep 1
			continue
		fi
	done

	if [ $retval -eq 0 ]
	then
		if [[ $1 -eq $START_COORDINATOR_ONLY ]]
		then
			return $SUCCESS
		fi

		for (( i=0; i<3; i++ ))
		do
			for (( j=0; j<$START_RETYR_TIMES; j++ ))
			do
				java -jar ${LAB3_ABSOLUTE_PATH}/kvstore2pcsystem.jar --config_path ${participants_config_path[i]} &
				check_background_process_start_status $!
				retval=$?

				if [[ $retval -eq 0 ]]
				then
					echo "Run participant[$i] successfully"
					participants_pid[$i]=$!
					break
				else
					echo "Run participant[$i]. Retry times: [$j]"
					continue
				fi
			done

			if [[ $retval -ne 0 ]]
			then
				echo "Run participant[$i] failed"
				return $FAIL
			fi
			if [[ $1 -eq $START_FIRST_PARTICIPANT_ONLY ]]
			then
				break
			fi
		done
	else
		echo "Run coordinator failed"
		return $FAIL
	fi

	echo "Run kvstore2pcsystem successfully"
	return $SUCCESS
}

function run_kvstore2pcsystem_python_robustly
{
	chmod 777 ${LAB3_ABSOLUTE_PATH}/*

	for (( i=0; i<$START_RETYR_TIMES; i++ ))
	do
		if [[ $1 -eq $START_FIRST_PARTICIPANT_ONLY ]]
		then
			break
		fi
		python3 ${LAB3_ABSOLUTE_PATH}/kvstore2pcsystem.py --config_path ${coordinator_config_path} &
		check_background_process_start_status $!
		retval=$?
		sleep 0.5

		if [[ $retval -eq 0 ]]
		then
			echo "Run coordinator successfully"
			coordinator_pid=$!
			break
		else
			sleep 1
			continue
		fi
	done

	if [ $retval -eq 0 ]
	then
		if [[ $1 -eq $START_COORDINATOR_ONLY ]]
		then
			return $SUCCESS
		fi

		for (( i=0; i<3; i++ ))
		do
			for (( j=0; j<$START_RETYR_TIMES; j++ ))
			do
				python3 ${LAB3_ABSOLUTE_PATH}/kvstore2pcsystem.py --config_path ${participants_config_path[i]} &
				retval=$?

				if [[ $retval -eq 0 ]]
				then
					echo "Run participant[$i] successfully"
					participants_pid[$i]=$!
					break
				else
					echo "Run participant[$i]. Retry times: [$j]"
					continue
				fi
			done

			if [[ $retval -ne 0 ]]
			then
				echo "Run participant[$i] failed"
				return $FAIL
			fi
			if [[ $1 -eq $START_FIRST_PARTICIPANT_ONLY ]]
			then
				break
			fi
		done
	else
		echo "Run coordinator failed"
		return $FAIL
	fi

	echo "Run kvstore2pcsystem successfully"
	return $SUCCESS
}

function run_kvstore2pcsystem_robustly
{
	language_checking

	programing_language=$?

	# Do make if there's a makefile regardless of in what language the system is written.
	if [ -f ${LAB3_ABSOLUTE_PATH}/Makefile ] || [ -f ${LAB3_ABSOLUTE_PATH}/makefile ]
	then
		do_make
	fi

	# other language
	if [ $programing_language -eq $UNKNOWN_LANGUAGE ]
	then
		echo "Start system: [ UNKNOWN LANGUAGE ]"
		run_kvstore2pcsystem_c_and_other_language_robustly $1
		retval=$?

		if [ $retval -eq $SUCCESS ]
		then
			return $SUCCESS
		else
			return $FAIL
		fi
	fi

	# c or c++
	if [ $programing_language -eq $C_OR_CPP ]
	then
		retval=$?
		if [ $retval -eq $SUCCESS ]
		then
			echo "Start system: [ C/C++ ]"
			run_kvstore2pcsystem_c_and_other_language_robustly $1
			retval=$?
			if [ $retval -eq $SUCCESS ]
			then
				return $SUCCESS
			else 
				return $FAIL
			fi
		else
			return $FAIL
		fi
	fi

	# java
	if [ $programing_language -eq $JAVA ]
	then
		echo "Start system: [ JAVA ]"
		run_kvstore2pcsystem_java_robustly $1
		retval=$?
		if [ $retval -eq $SUCCESS ]
		then
			return $SUCCESS
		else
			return $FAIL
		fi
	fi

	# python
	if [ $programing_language -eq $PYTHON ]
	then
		echo "Start system: [ PYTHON ]"
		run_kvstore2pcsystem_python_robustly $1
		retval=$?
		if [ $retval -eq $SUCCESS ]
		then
			return $SUCCESS
		else
			return $FAIL
		fi
	fi
}

function kill_and_restart_coordinator_robustly
{
	echo "Kill coordinator and then restart."

	kill -9 ${coordinator_pid}
	sleep 1
	run_kvstore2pcsystem_robustly $START_COORDINATOR_ONLY

	retval=$?
	if [[ $retval -eq 0 ]]
	then
		return $SUCCESS
	else
		return $FAIL
	fi
}

function kill_coordinator_and_all_participants
{
	kill -9 ${coordinator_pid}

	for (( i=0; i<3; i++ ))
	do
		kill -9 ${participants_pid[i]}
	done
}

function kill_coordinator
{
	kill -9 ${coordinator_pid}
}

function kill_one_of_participants
{
	kill -9 ${participants_pid[0]}
}

function kill_two_of_participants
{
	for (( i=1; i<3; i++ ))
	do
		kill -9 ${participants_pid[i]}
	done	
}

function kill_all_participants
{
	for (( i=0; i<3; i++ ))
	do
		kill -9 ${participants_pid[i]}
	done
}

function restart_kvstore2pcsystem_if_down_abnormally
{
	if ! ps -p $coordinator_pid > /dev/null
	then
		kill_coordinator_and_all_participants
		run_kvstore2pcsystem_robustly $START_COORDINATOR_AND_ALL_PARTICIPANTS
		return
	fi

	for (( i=0; i<3; i++ ))
	do
		if ! ps -p ${participants_pid[i]} > /dev/null
		then
			kill_coordinator_and_all_participants
			run_kvstore2pcsystem_robustly $START_COORDINATOR_AND_ALL_PARTICIPANTS
			return
		fi
	done
}

set_result=""
function send_set_command
{
	key_len=$1
	key=$2
	value_len="$3"
	value=$4

	printf -v set_command "*3\r\n\$3\r\nSET\r\n\$${key_len}\r\n${key}\r\n\$${value_len}\r\n${value}\r\n"

	for (( i=0; i<$ERROR_RETRY_TIMES; i++ ))
	do
		retval_set=`printf "$set_command" | nc -w ${NC_TIMEOUT} ${COORDINATOR_IP} ${COORDINATOR_PORT}`

	    if [[ $retval_set =~ $standard_error ]]
	    then
	    	sleep 0.5
	    	continue
	    else
	    	break
	    fi
	done
	echo $retval_set
	printf -v set_result "%s" "${retval_set}"
}

get_result=""
function send_get_command
{
	key_len=$1
	key=$2

	printf -v get_command "*2\r\n\$3\r\nGET\r\n\$${key_len}\r\n${key}\r\n"
	for (( i=0; i<$ERROR_RETRY_TIMES; i++ ))
	do
		retval_get=`printf "$get_command" | nc -w ${NC_TIMEOUT} ${COORDINATOR_IP} ${COORDINATOR_PORT}`

	    if [[ $retval_get =~ $standard_error ]]
	    then
	    	sleep 0.5
	    	continue
	    else
	    	break
	    fi
	done

	printf -v get_result "%s" "$retval_get"
}

function send_get_command_ultimate_version
{
	key_len=$1
	key=$2

	printf -v get_command "*2\r\n\$3\r\nGET\r\n\$${key_len}\r\n${key}\r\n"
	for (( i=0; i<$ERROR_RETRY_TIMES; i++ ))
	do
		retval_get=`printf "$get_command" | nc -w ${NC_TIMEOUT} 192.168.66.202 8003`

	    if [[ $retval_get =~ $standard_error ]]
	    then
	    	sleep 0.5
	    	continue
	    else
	    	break
	    fi
	done

	printf -v get_result "%s" "$retval_get"
}

del_1_result=""
function send_del_command_1
{
	key1_len=$1
	key1=$2

	printf -v del_command_1 "*2\r\n\$3\r\nDEL\r\n\$${key1_len}\r\n${key1}\r\n"
	for (( i=0; i<$ERROR_RETRY_TIMES; i++ ))
	do
		retval_del1=`printf "$del_command_1" | nc -w ${NC_TIMEOUT} ${COORDINATOR_IP} ${COORDINATOR_PORT}`

	    if [[ $retval_del1 =~ $standard_error ]]
	    then
	    	sleep 0.5
	    	continue
	    else
	    	break
	    fi
	done

	printf -v del_1_result "%s" "$retval_del1"
}

del_2_result=""
function send_del_command_2
{
	key1_len=$1
	key1=$2
	key2_len=$3
	key2=$4
	printf -v del_command_2 "*3\r\n\$3\r\nDEL\r\n\$${key1_len}\r\n${key1}\r\n\$${key2_len}\r\n${key2}\r\n"
	for (( i=0; i<$ERROR_RETRY_TIMES; i++ ))
	do
		retval_del2=`printf "$del_command_2" | nc -w ${NC_TIMEOUT} ${COORDINATOR_IP} ${COORDINATOR_PORT}`

	    if [[ $retval_del2 =~ $standard_error ]]
	    then
	    	sleep 0.5
	    	continue
	    else
	    	break
	    fi
	done

	printf -v del_2_result "%s" "$retval_del2"
}


# ######################## basic version ########################

function set_tag
{
	echo "                                       | |                                   "
	echo "                                       | |                                   "
	echo "                                       \|/                                   "
}

printf -v standard_error "%s\r" "-ERROR"
printf -v standard_ok "+OK\r"
printf -v standard_nil "*1\r\n\$3\r\nnil\r"

standard_item1=""
function test_item1
{
	set_tag
	echo "---------------------------------- Test item 1 ----------------------------------"
	echo "Test item 1. Test point: Run kvstore2pcsystem."

	run_kvstore2pcsystem_robustly $START_COORDINATOR_AND_ALL_PARTICIPANTS

	retval=$?
	if [[ $retval -eq $SUCCESS ]]
	then
		echo "============================ [PASSED] : Test item 1 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 1 ============================"
		return $FAILED
	fi
}

standard_item2="$standard_ok"
function test_item2
{
	set_tag
	echo "---------------------------------- Test item 2 ----------------------------------"
	echo "Test item 2. Test point: Set key to hold string value."

	restart_kvstore2pcsystem_if_down_abnormally
	#sleep 10
	send_set_command 9 item2_key 11 item2_value

	echo "item set set_result: ${set_result}"
	if [[ $set_result = $standard_item2 ]]
	then
		echo "============================ [PASSED] : Test item 2 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 2 ============================"
		return $FAILED
	fi
}


printf -v standard_item3 "*1\r\n\$11\r\nitem3_value\r"
function test_item3
{
	set_tag
	echo "---------------------------------- Test item 3 ----------------------------------"
	echo "Test item 3. Test point: Get the value of key."
	# restart_kvstore2pcsystem_if_down_abnormally

	send_set_command 9 item3_key 11 item3_value

	kill_and_restart_coordinator_robustly
	send_get_command 9 item3_key
	echo "get_result: ${get_result}"
	if [[ $get_result = $standard_item3 ]]
	then
		echo "============================ [PASSED] : Test item 3 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 3 ============================"
		return $FAILED
	fi
}


standard_item4="$standard_nil"
function test_item4
{
	set_tag
	echo "---------------------------------- Test item 4 ----------------------------------"
	echo "Test item 4. Test point: Return nil if the key does no exist."
	# restart_kvstore2pcsystem_if_down_abnormally

	send_get_command 9 item4_key

	if [[ $get_result = $standard_item4 ]]
	then
		echo "============================ [PASSED] : Test item 4 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 4 ============================"
		return $FAILED
	fi

}


printf -v standard_item5 ":2\r"
function test_item5
{
	set_tag
	echo "---------------------------------- Test item 5 ----------------------------------"
	echo "Test item 5. Test point: If the DEL command executed, return the number of keys that were removed."
	# restart_kvstore2pcsystem_if_down_abnormally

	send_set_command 11 item5_key_1 13 item5_value_1
	send_set_command 11 item5_key_2 13 item5_value_2
	kill_and_restart_coordinator_robustly
	send_del_command_2 11 item5_key_1 11 item5_key_2

	if [[ $del_2_result = $standard_item5 ]]
	then
		echo "============================ [PASSED] : Test item 5 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 5 ============================"
		return $FAILED
	fi
}


printf -v standard_item6 "*1\r\n\$15\r\nitem6_value_new\r"
function test_item6
{
	set_tag
	echo "---------------------------------- Test item 6 ----------------------------------"
	echo "Test item 6. Test point: When associating a new value to an existing key," \
	     " it should overwrite the value of the existing entry,"
	# restart_kvstore2pcsystem_if_down_abnormally

	send_set_command 9 item6_key 11 item6_value
	send_set_command 9 item6_key 15 item6_value_new
	kill_and_restart_coordinator_robustly
	send_get_command 9 item6_key

	if [[ $get_result = $standard_item6 ]]
	then
		echo "============================ [PASSED] : Test item 6 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 6 ============================"
		return $FAILED
	fi
}


standard_item7="$standard_nil"
function test_item7
{
	set_tag
	echo "---------------------------------- Test item 7 ----------------------------------"
	echo "Test item 7. Test point: Correctness testing of DEL command."
	# restart_kvstore2pcsystem_if_down_abnormally

	send_set_command 9 item7_key 11 item7_value
	send_del_command_1 9 item7_key
	send_get_command 9 item7_key

	if [[ $get_result = $standard_item7 ]]
	then
		echo "============================ [PASSED] : Test item 7 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 7 ============================"
		return $FAILED
	fi
}


# ######################## advanced version ########################

printf -v standard_item8 "*1\r\n\$17\r\nitem8_key_value_3\r"
function test_item8
{
	set_tag
	echo "---------------------------------- Test item 8 ----------------------------------"
	echo "Test item 8. Test point: Kill one of the participants."

	send_set_command 9 item8_key 17 item8_key_value_1
	send_set_command 9 item8_key 17 item8_key_value_2
	kill_one_of_participants
	send_set_command 9 item8_key 17 item8_key_value_3

	send_get_command 9 item8_key

	if [[ $get_result = $standard_item8 ]]
	then
		echo "============================ [PASSED] : Test item 8 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 8 ============================"
		return $FAILED
	fi

}


standard_item9="$standard_error"
function test_item9
{
	set_tag
	echo "---------------------------------- Test item 9 ----------------------------------"
	echo "Test item 9. Test point: Kill all of the participants."

	send_set_command 9 item9_key 17 item9_key_value_1
	send_set_command 9 item9_key 17 item9_key_value_2
	kill_all_participants

	send_get_command 9 item9_key

	if [[ $get_result =~ $standard_item9 ]]
	then
		echo "============================ [PASSED] : Test item 9 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 9 ============================"
		return $FAILED
	fi

}


# ######################## extreme version ########################

printf -v standard_item10 "*1\r\n\$20\r\nitem10_key_2_value_2\r"
function test_item10
{
	set_tag
	echo "---------------------------------- Test item 10 ----------------------------------"
	echo "Test item 10. Test point: extreme version test."

	# Restart all participants
	restart_kvstore2pcsystem_if_down_abnormally

	send_set_command 12 item10_key_1 20 item10_key_1_value_1
	send_set_command 12 item10_key_2 20 item10_key_2_value_2
	kill_one_of_participants
	kill_and_restart_coordinator_robustly
	run_kvstore2pcsystem_robustly $START_FIRST_PARTICIPANT_ONLY
	sleep 10
	kill_two_of_participants

	send_get_command 12 item10_key_2

	if [[ $get_result = $standard_item10 ]]
	then
		echo "============================ [PASSED] : Test item 10 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 10 ============================"
		return $FAILED
	fi

}


# ######################## ultimate version ########################


printf -v standard_item11 "*1\r\n\$20\r\nitem11_key_2_value_2\r"
function test_item11
{
	set_tag
	echo "---------------------------------- Test item 11 ----------------------------------"
	echo "Test item 11. Test point: ultimate version test."

	send_set_command 12 item11_key_1 20 item11_key_1_value_1
	send_set_command 12 item11_key_2 20 item11_key_2_value_2
	kill_coordinator

	send_get_command_ultimate_version 12 item11_key_2

	if [[ $get_result = $standard_item11 ]]
	then
		echo "============================ [PASSED] : Test item 11 ============================"
		return $PASSED
	else
		echo "============================ [FAILED] : Test item 11 ============================"
		return $FAILED
	fi

}


# ######################################################################

function cloud_roll_up
{
	echo "---------------------------------- Global test start ----------------------------------"
	TEST_RESULT_ARR[0]=$programing_language
	test_item1
	TEST_RESULT_ARR[1]=$?
	if [[ ${TEST_RESULT_ARR[1]} -eq $FAILED ]]
	then
		echo "---------------------------------- Start system failed. Global test done ----------------------------------"
		clean_up
		return
	fi
	test_item2
	TEST_RESULT_ARR[2]=$?
	test_item3
	TEST_RESULT_ARR[3]=$?
	test_item4
	TEST_RESULT_ARR[4]=$?
	test_item5
	TEST_RESULT_ARR[5]=$?
	test_item6
	TEST_RESULT_ARR[6]=$?
	test_item7
	TEST_RESULT_ARR[7]=$?
	test_item8
	TEST_RESULT_ARR[8]=$?
	test_item9
	TEST_RESULT_ARR[9]=$?
	test_item10
	TEST_RESULT_ARR[10]=$?
	test_item11
	TEST_RESULT_ARR[11]=$?

	echo "---------------------------------- Global test done ----------------------------------"
}

function clean_up
{
	kill_coordinator_and_all_participants
	remove_virtual_nics
}

function show_test_result
{
	echo "Language: [${TEST_RESULT_ARR[0]}]"
	for (( i=1; i<12; i++ ))
	do
		if [[ ${TEST_RESULT_ARR[i]} -eq $PASSED ]]
		then
			echo "Test item ${i}: PASSED"
		else
			echo "Test item ${i}: FAILED"
		fi
	done
}

function prepare_test_env
{
	generate_config_files
	init_network_env
}



# ######################################################################
# get ready to start at once

prepare_test_env
cloud_roll_up
clean_up
show_test_result
