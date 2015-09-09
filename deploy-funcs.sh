function install() {
	echo == executing install on host: $HOSTNAME ==
	sudo cp ~/src/xtree/ctests/departures/departures /usr/local/bin/
	/usr/local/bin/departures -v
}

function info() {
	echo == executing info on host: $HOSTNAME ==
	crontab -l|grep departures
	/usr/local/bin/departures -v
}

function test() {
	echo == executing test on host: $HOSTNAME ==
	crontab -l|grep departures
	/usr/local/bin/departures -v
	/usr/local/bin/departures -f XG -t HB -m
}

function build() {
	if [ ! -d ~/src/xtree ]; then
		mkdir -p ~/src/xtree
		cd ~/src/xtree
		git clone https://github.com/serge-v/ctests
		cd ctests
	fi
	cd ~/src/xtree/ctests/departures
	cmake .
	echo == executing build on host: $HOSTNAME ==
	git pull
	make clean
	make
	echo == new version ==
	./departures -v
	echo == deployed version ==
	/usr/local/bin/departures -v
	echo == crontab ==
	crontab -l|grep departures
	echo
	echo Project was build on $host. For install run script with -i parameter.
}

