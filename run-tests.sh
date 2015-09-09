function server() {
	subdir=$1
	cd ../tests/$subdir
	python ../server.py 2>&1 >/dev/null  &
	PID=$!
	echo subdir: $subdir, server pid: $PID
	cd -
}

function check() {
	name=`printf "%-20s" "$1"`
	cmd=$2
	expected=$3

	$cmd > 1~.txt
	diff --brief 1~.txt $expected

	if [[ $? == 1 ]] ; then
		echo -e "$name" '\x1b[31m' -- FAIL'\x1b[0m'
		echo command: $cmd
		colordiff -U 1000 1~.txt ../tests/1.txt
	else
		echo "$name" -- OK
	fi
}

server "01"
sleep 1

check "XG to PO" "./departures -f XG -t PO -s" ../tests/1.txt
check "XG to PO cached" "./departures -f XG -t PO -s" ../tests/1.txt

kill $PID
wait 2> /dev/null




